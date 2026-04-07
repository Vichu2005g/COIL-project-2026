#include "RobotController.h"
#include <ctime>
#include <cstring>

RobotController::RobotController() : ipAddr(""), port(0), protocol("udp"), pktCounter(0), sentCount(0), ackOKCount(0), ackBadCount(0), socket(nullptr) {}

RobotController::~RobotController() {
    if (socket) {
        delete socket;
        socket = nullptr;
    }
}

bool RobotController::configure(const std::string& ip, int port, const std::string& protocol) {
    ipAddr = ip;
    this->port = port;
    this->protocol = protocol;

    if (socket) delete socket;
    
    if (protocol == "tcp") {
        socket = new MySocket(CLIENT, ipAddr, port, TCP);
        socket->ConnectTCP();
    } else {
        socket = new MySocket(CLIENT, ipAddr, port, UDP);
        socket->ConnectUDP();
    }

    // Handshake: Try to get telemetry to verify robot is alive
    std::string msg;
    bool ack;
    int pcount;
    TelemetryBody tel;
    bool alive = requestTelemetry(msg, ack, pcount, tel);

    if (!alive) {
        // If it failed, we clean up
        if (socket) {
            delete socket;
            socket = nullptr;
        }
        return false;
    }

    addLog("CONFIG", "IP: " + ip + " Port: " + std::to_string(port) + " Protocol: " + this->protocol);
    return true;
}

bool RobotController::sendDrive(int direction, int duration, int power, std::string& outMessage, bool& outAck, int& outPktCount) {
    if (!socket || !socket->IsConnectionSocketValid()) return false;

    PktDef pkt;
    pkt.SetPktCount(++pktCounter);
    pkt.SetCmd(DRIVE);

    DriveBody b;
    b.Direction = (unsigned char)direction;
    b.Duration = (unsigned char)duration;
    b.Power = (unsigned char)power;

    pkt.SetBodyData(reinterpret_cast<char*>(&b), sizeof(DriveBody));
    
    std::string dirName = (direction == FORWARD) ? "FORWARD" : "BACKWARD";
    addLog("SEND", "Cmd: " + dirName + " Duration: " + std::to_string(duration) + " Power: " + std::to_string(power) + " Pkt: " + std::to_string(pktCounter));

    return executeTransaction(pkt, outMessage, outAck, outPktCount);
}

bool RobotController::sendTurn(int direction, int duration, std::string& outMessage, bool& outAck, int& outPktCount) {
    if (!socket || !socket->IsConnectionSocketValid()) return false;

    PktDef pkt;
    pkt.SetPktCount(++pktCounter);
    pkt.SetCmd(DRIVE);

    TurnBody b;
    b.Direction = (unsigned char)direction;
    b.Duration = (unsigned short)duration;

    pkt.SetBodyData(reinterpret_cast<char*>(&b), sizeof(TurnBody));

    std::string dirName = (direction == LEFT) ? "LEFT" : "RIGHT";
    addLog("SEND", "Cmd: " + dirName + " Duration: " + std::to_string(duration) + " Pkt: " + std::to_string(pktCounter));

    return executeTransaction(pkt, outMessage, outAck, outPktCount);
}

bool RobotController::sendSleep(std::string& outMessage, bool& outAck, int& outPktCount) {
    if (!socket || !socket->IsConnectionSocketValid()) return false;

    PktDef pkt;
    pkt.SetPktCount(++pktCounter);
    pkt.SetCmd(SLEEP);
    pkt.SetBodyData(nullptr, 0);

    addLog("SEND", "Cmd: SLEEP Pkt: " + std::to_string(pktCounter));

    return executeTransaction(pkt, outMessage, outAck, outPktCount);
}

bool RobotController::requestTelemetry(std::string& outMessage, bool& outAck, int& outPktCount, TelemetryBody& outTelemetry) {
    if (!socket || !socket->IsConnectionSocketValid()) return false;

    std::lock_guard<std::mutex> lock(transactionMutex);
    sentCount++;

    PktDef pkt;
    pkt.SetPktCount(++pktCounter);
    pkt.SetCmd(RESPONSE);
    pkt.SetBodyData(nullptr, 0);
    pkt.CalcCRC();

    char* raw = pkt.GenPacket();
    int totalLen = pkt.GetLength() + 1;
    socket->SendData(raw, totalLen);

    addLog("SEND", "Cmd: TELEMETRY Pkt: " + std::to_string(pktCounter));

    // Robot may send ACK first, then telemetry data — try reading up to 2 times
    for (int attempt = 0; attempt < 2; attempt++) {
        char recvBuf[1024] = { 0 };
        int rlen = socket->GetData(recvBuf);

        if (rlen <= 0) {
            // Timeout on this read
            break;
        }

        PktDef resp(recvBuf);
        outPktCount = resp.GetPktCount();

        if (!resp.CheckCRC(recvBuf, rlen)) {
            ackBadCount++;
            outMessage = "CRC Error";
            addLog("RECV", "CRC ERROR Pkt: " + std::to_string(outPktCount));
            continue;
        }

        if (resp.IsTelemetry()) {
            // Got the telemetry data we wanted
            outTelemetry = resp.GetTelemetryData();
            outAck = true;
            outMessage = "Telemetry Received";
            ackOKCount++;
            addLog("RECV", "Telemetry data received Pkt: " + std::to_string(outPktCount));
            return true;
        }

        // Got an ACK packet — log it and try reading again for the telemetry body
        outAck = resp.GetAck();
        if (outAck) ackOKCount++; else ackBadCount++;
        addLog("RECV", std::string(outAck ? "ACK" : "NACK") + " (waiting for telemetry) Pkt: " + std::to_string(outPktCount));
    }

    ackBadCount++;
    outMessage = "Timeout - No telemetry received";
    addLog("RECV", "TIMEOUT - No telemetry for Pkt: " + std::to_string(pktCounter));
    return false;
}

bool RobotController::executeTransaction(PktDef& pkt, std::string& outMessage, bool& outAck, int& outPktCount, char* outData) {
    std::lock_guard<std::mutex> lock(transactionMutex);
    sentCount++;
    char* raw = pkt.GenPacket();
    int totalLen = pkt.GetLength() + 1;

    socket->SendData(raw, totalLen);

    char recvBuf[1024] = { 0 };
    int rlen = socket->GetData(recvBuf);

    if (rlen > 0) {
        PktDef resp(recvBuf);
        bool crcOK = resp.CheckCRC(recvBuf, rlen);
        outAck = resp.GetAck();
        outPktCount = resp.GetPktCount();

        if (crcOK) {
            if (outData) memcpy(outData, recvBuf, rlen);
            
            // If it's a telemetry packet, use specialized message, else use ACK/NACK
            if (resp.IsTelemetry()) {
                outMessage = "Telemetry Received";
                outAck = true;
            } else {
                outMessage = outAck ? "ACK Received" : "NACK Received";
            }

            addLog("RECV", std::string(outAck ? "ACK" : "NACK") + " Received for Pkt: " + std::to_string(outPktCount));
            if (outAck) ackOKCount++; else ackBadCount++;
        } else {
            ackBadCount++;
            outMessage = "CRC Error";
            addLog("RECV", "CRC ERROR for Pkt: " + std::to_string(outPktCount));
        }
        return true;
    } else {
        ackBadCount++;
        outMessage = "Timeout - No response";
        addLog("RECV", "TIMEOUT - No response for Pkt: " + std::to_string(pktCounter));
        return false;
    }
}

void RobotController::addLog(const std::string& direction, const std::string& content) {
    std::lock_guard<std::mutex> lock(logMutex);
    LogEntry entry;
    entry.timestamp = getTimestamp();
    entry.direction = direction;
    entry.content = content;
    
    // Keep internal log size manageable (last 50 logs)
    if (logs.size() > 50) {
        logs.erase(logs.begin());
    }
    logs.push_back(entry);
}

std::vector<LogEntry> RobotController::getLogs() {
    std::lock_guard<std::mutex> lock(logMutex);
    return logs;
}

std::string RobotController::getTimestamp() {
    time_t t = time(nullptr);
    struct tm tm;
    #ifdef _WIN32
    localtime_s(&tm, &t);
    #else
    localtime_r(&t, &tm);
    #endif
    char buf[20];
    strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
    return std::string(buf);
}

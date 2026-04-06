#include "RobotController.h"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>

RobotController::RobotController() : ipAddr(""), port(0), protocol("udp"), pktCounter(0), socket(nullptr) {}

RobotController::~RobotController() {
    if (socket) {
        delete socket;
        socket = nullptr;
    }
}

bool RobotController::configure(const std::string& ip, int port, const std::string& protocol) {
    this->ipAddr = ip;
    this->port = port;
    this->protocol = (protocol == "tcp") ? "tcp" : "udp";

    if (socket) {
        delete socket;
    }

    ConnectionType connType = (this->protocol == "tcp") ? TCP : UDP;
    socket = new MySocket(CLIENT, this->ipAddr, this->port, connType, 1024);

    if (connType == TCP) {
        socket->ConnectTCP();
    } else {
        socket->ConnectUDP();
    }

    addLog("CONFIG", "IP: " + ip + " Port: " + std::to_string(port) + " Protocol: " + this->protocol);
    return socket->IsConnectionSocketValid();
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

    PktDef pkt;
    pkt.SetPktCount(++pktCounter);
    pkt.SetCmd(RESPONSE);
    pkt.SetBodyData(nullptr, 0);

    addLog("SEND", "Cmd: TELEMETRY Pkt: " + std::to_string(pktCounter));

    bool success = executeTransaction(pkt, outMessage, outAck, outPktCount);

    if (success) {
        // Wait briefly for response data
        char recvBuf[1024] = { 0 };
        int rlen = socket->GetData(recvBuf);
        if (rlen > 0) {
            PktDef resp(recvBuf);
            if (resp.IsTelemetry()) {
                outTelemetry = resp.GetTelemetryData();
                addLog("RECV", "Telemetry data received for Pkt: " + std::to_string(outPktCount));
                return true;
            }
        }
    }
    return false;
}

bool RobotController::executeTransaction(PktDef& pkt, std::string& outMessage, bool& outAck, int& outPktCount) {
    char* raw = pkt.GenPacket();
    int totalLen = pkt.GetLength() + 1;

    socket->SendData(raw, totalLen);

    // Give simulator or robot time to respond
    // In a real REST server this might need to be shorter or non-blocking,
    // but for the simulator demo it's fine.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    char recvBuf[1024] = { 0 };
    int rlen = socket->GetData(recvBuf);

    if (rlen > 0) {
        PktDef resp(recvBuf);
        bool crcOK = resp.CheckCRC(recvBuf, rlen);
        outAck = resp.GetAck();
        outPktCount = resp.GetPktCount();

        if (crcOK) {
            outMessage = outAck ? "ACK Received" : "NACK Received";
            addLog("RECV", std::string(outAck ? "ACK" : "NACK") + " Received for Pkt: " + std::to_string(outPktCount));
        } else {
            outMessage = "CRC Error";
            addLog("RECV", "CRC ERROR for Pkt: " + std::to_string(outPktCount));
        }
        return true;
    } else {
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

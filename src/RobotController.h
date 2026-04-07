#pragma once
#include "Packet.h"
#include "Socket.h"
#include <string>
#include <vector>
#include <mutex>

struct LogEntry {
    std::string timestamp;
    std::string direction; // "SEND" or "RECV"
    std::string content;
};

class RobotController {
public:
    RobotController();
    ~RobotController();

    bool configure(const std::string& ip, int port, const std::string& protocol);
    
    // Command methods
    bool sendDrive(int direction, int duration, int power, std::string& outMessage, bool& outAck, int& outPktCount);
    bool sendTurn(int direction, int duration, std::string& outMessage, bool& outAck, int& outPktCount);
    bool sendSleep(std::string& outMessage, bool& outAck, int& outPktCount);
    bool requestTelemetry(std::string& outMessage, bool& outAck, int& outPktCount, TelemetryBody& outTelemetry);

    // Logging
    std::vector<LogEntry> getLogs();
    void addLog(const std::string& direction, const std::string& content);

    // Getters for state
    std::string getIP() const { return ipAddr; }
    int getPort() const { return port; }
    std::string getProtocol() const { return protocol; }
    int getSentCount() const { return sentCount; }
    int getAckOKCount() const { return ackOKCount; }
    int getAckBadCount() const { return ackBadCount; }
    int getPktCount() const { return pktCounter; }

private:
    bool executeTransaction(PktDef& pkt, std::string& outMessage, bool& outAck, int& outPktCount, char* outData = nullptr);
    std::string getTimestamp();

    std::string ipAddr;
    int port;
    std::string protocol;
    int pktCounter;
    int sentCount;
    int ackOKCount;
    int ackBadCount;

    MySocket* socket;
    std::vector<LogEntry> logs;
    std::mutex logMutex;
    std::mutex transactionMutex;
};

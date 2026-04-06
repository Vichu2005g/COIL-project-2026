#pragma once
#include <memory>
#include <iostream>
#include <fstream>
#include <cstring>

#pragma pack(push, 1)

const int EmptyPktSize = 5;
const int FORWARD = 1;
const int BACKWARD = 2;
const int RIGHT = 3;
const int LEFT = 4;
const int HEADERSIZE = 4;

enum CmdType { DRIVE, SLEEP, RESPONSE };

struct Header {
    unsigned short int PktCount;
    unsigned char Drive : 1;
    unsigned char Status : 1;
    unsigned char Sleep : 1;
    unsigned char Ack : 1;
    unsigned char Padding : 4;
    unsigned char Length;
};

struct DriveBody {
    unsigned char Direction;
    unsigned char Duration;
    unsigned char Power;
};

struct TurnBody {
    unsigned char Direction;
    unsigned short int Duration : 16;
};

struct TelemetryBody {
    unsigned short int LastPktCounter;
    unsigned short int CurrentGrade;
    unsigned short int HitCount;
    unsigned short int Heading;
    unsigned char LastCmd;
    unsigned char LastCmdValue;
    unsigned char LastCmdPower;
};

class PktDef
{
    struct CmdPacket {
        Header head;
        char* Data;
        char CRC;
    } packet;

    char* RawBuffer;

public:
    PktDef();
    PktDef(char* src);
    ~PktDef(); // Added destructor for cleanup

    void SetCmd(CmdType cmd);
    void SetBodyData(char* src, int size);
    void SetAck(bool value);
    void SetPktCount(int value);

    CmdType GetCmd();
    bool GetAck();
    int GetLength();
    char* GetBodyData();
    int GetPktCount();

    bool CheckCRC(char* buf, int len);
    void CalcCRC();
    char* GenPacket();

    bool IsTelemetry();
    bool IsAck();
    bool IsNack();
    TelemetryBody GetTelemetryData();
    void Display();
};

#pragma pack(pop)

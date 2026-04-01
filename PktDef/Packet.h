#pragma once
#include <memory>
#include <iostream>
#include <cstring>
#include <fstream>

const int EmptyPktSize = 6;					// Number of data bytes in a packet with no data field

// --- Constants & Enumerations ---
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
    unsigned short int Duration;
};

class PktDef
{
    struct CmdPacket {
        Header head{};
        char* Data = nullptr;
        char CRC = 0;
    } packet{};

    char* RawBuffer = nullptr;

public:
    // Default Constructor - Safe State
    PktDef() : packet{}, RawBuffer(nullptr) {}

    /*******************************************************************************
    STUDENTS:  You are responsible for writing the logic for the following functions
               as per the specifications provided
    *******************************************************************************/

    ~PktDef()
    {
        delete[] packet.Data;
        packet.Data = nullptr;

        delete[] RawBuffer;
        RawBuffer = nullptr;
    }

    // Anh Dung Phan
    PktDef(char* src) : packet{}, RawBuffer(nullptr)
    {
        if (src == nullptr) return;

        // Extract header first
        memcpy(&packet.head, src, sizeof(Header));

        // Validate length before using it
        if (packet.head.Length < sizeof(Header)) {
            packet.head = Header{};
            packet.CRC = 0;
            return;
        }

        int bodySize = static_cast<int>(packet.head.Length) - static_cast<int>(sizeof(Header));

        if (bodySize > 0) {
            packet.Data = new char[bodySize];
            memcpy(packet.Data, src + sizeof(Header), bodySize);
        }

        // Extract CRC from the tail
        memcpy(&packet.CRC, src + sizeof(Header) + bodySize, 1);

        // Store the full raw packet including CRC
        RawBuffer = new char[packet.head.Length + 1];
        memcpy(RawBuffer, src, packet.head.Length + 1);
    }

    // Anh Dung Phan
    void SetCmd(CmdType cmd) {
        packet.head.Drive = 0;
        packet.head.Status = 0;
        packet.head.Sleep = 0;

        switch (cmd) {
        case DRIVE: packet.head.Drive = 1; break;
        case SLEEP: packet.head.Sleep = 1; break;
        case RESPONSE: packet.head.Status = 1; break;
        }
    }

    // Jason Little
    void SetBodyData(char* src, int size) {
        if (packet.Data) {
            delete[] packet.Data;
            packet.Data = nullptr;
        }

        if (src != nullptr && size > 0) {
            packet.Data = new char[size];
            memcpy(packet.Data, src, size);
        }

        packet.head.Length = static_cast<unsigned char>(sizeof(Header) + size);
    }

    // Jason Little
    void SetPktCount(int value) {
        packet.head.PktCount = static_cast<unsigned short>(value);
    }

    // Jason Little
    CmdType GetCmd() {
        if (packet.head.Drive == 1)
            return DRIVE;

        if (packet.head.Sleep == 1)
            return SLEEP;

        if (packet.head.Status == 1)
            return RESPONSE;

        // Should never happen if protocol is valid
        return RESPONSE;
    }

    // Amna
    bool GetAck()
    {
        return (packet.head.Ack) != 0;
    }

    // Amna
    int GetLength() {
        return packet.head.Length;
    }

    // Amna
    char* GetBodyData() {
        return packet.Data;
    }

    // Amna
    int GetPktCount() {
        return packet.head.PktCount;
    }

    // Vishwaanth
    bool CheckCRC(char* buf, int len) {
        if (buf == nullptr || len < sizeof(Header) + 1)
            return false;  // Min header + CRC

        unsigned char calc_crc = 0;

        for (int i = 0; i < len - 1; i++) {
            unsigned char byte = (unsigned char)buf[i];

            while (byte) {
                calc_crc += (byte & 1);
                byte >>= 1;
            }
        }

        calc_crc %= 256;

        unsigned char received_crc = (unsigned char)buf[len - 1];

        return (calc_crc == received_crc);
    }

    // Vishwaanth
    void CalcCRC() {
        unsigned char crc = 0;

        // Clamp empty-body packets to zero body length
        int body_len = (static_cast<int>(packet.head.Length) > static_cast<int>(sizeof(Header)))
            ? static_cast<int>(packet.head.Length) - static_cast<int>(sizeof(Header))
            : 0;

        int total_no_crc = sizeof(Header) + body_len;

        char* temp_buf = new char[total_no_crc];

        // Copy header
        memcpy(temp_buf, &packet.head, sizeof(Header));

        // Copy body if exists
        if (packet.Data && body_len > 0) {
            memcpy(temp_buf + sizeof(Header), packet.Data, body_len);
        }

        // Count 1-bits
        for (int i = 0; i < total_no_crc; i++) {
            unsigned char byte = (unsigned char)temp_buf[i];

            int bitCount = 0;
            while (byte) {
                bitCount += byte & 1;
                byte >>= 1;
            }

            crc += bitCount;
        }

        crc %= 256;

        delete[] temp_buf;

        packet.CRC = crc;
    }

    // Vishwaanth
    char* GenPacket() {
        if (RawBuffer) {
            delete[] RawBuffer;
            RawBuffer = nullptr;
        }

        // Ensure packet length is at least large enough to hold the header
        if (packet.head.Length < sizeof(Header)) {
            packet.head.Length = sizeof(Header);
        }

        CalcCRC();  // Ensure fresh CRC

        int body_len = (static_cast<int>(packet.head.Length) > static_cast<int>(sizeof(Header)))
            ? static_cast<int>(packet.head.Length) - static_cast<int>(sizeof(Header))
            : 0;

        int total_size = packet.head.Length + 1;  // + CRC

        RawBuffer = new char[total_size];

        // Copy header
        memcpy(RawBuffer, &packet.head, sizeof(Header));

        // Copy body
        if (packet.Data && body_len > 0) {
            memcpy(RawBuffer + sizeof(Header), packet.Data, body_len);
        }

        // Copy CRC
        memcpy(RawBuffer + sizeof(Header) + body_len, &packet.CRC, 1);

        return RawBuffer;
    }
};
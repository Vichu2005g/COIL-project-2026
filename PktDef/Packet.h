#pragma once
#include <memory>
#include <iostream>
#include <fstream>

#pragma pack(push, 1)

const int EmptyPktSize = 5;					//Number of data bytes in a packet with no data field

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
    // Default Constructor - Safe State
    PktDef() : RawBuffer(nullptr) {
        memset(&packet.head, 0, sizeof(Header));
        packet.Data = nullptr;
        packet.CRC = 0;
    };

    /*******************************************************************************
    STUDENTS:  You are responsible for writing the logic for the following functions
               as per the specifications provided
    *******************************************************************************/

    // Anh Dung Phan
    PktDef(char* src)
    {
        if (src == nullptr) return;
        packet.Data = nullptr;
        delete RawBuffer;

        // Extract header
        memcpy(&packet.head, src, sizeof(Header));

        // Calculate Body Size based on total length minus Header and CRC
        // The spec states "Length" is the total number of bytes in the packet.
        int bodySize = packet.head.Length - sizeof(Header); // Removed - 1 because CRC is not included in Length as per spec

        if (bodySize > 0) {
            packet.Data = new char[bodySize];
            memcpy(packet.Data, src + sizeof(Header), bodySize);
        }

        // Extract CRC from the tail
        memcpy(&packet.CRC, src + sizeof(Header) + bodySize, 1);

        // Store a copy in RawBuffer
        RawBuffer = new char[packet.head.Length];
        memcpy(RawBuffer, src, packet.head.Length);
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
        if (packet.Data)
            delete[] packet.Data;

        packet.Data = new char[size];
        memcpy(packet.Data, src, size);

        packet.head.Length = sizeof(Header) + size + 1; // Set total packet length: header + body
    }

    // Method to set the Acknowledgement flag 
    void SetAck(bool value) {
        packet.head.Ack = value ? 1 : 0;
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
        if (buf == nullptr || len < sizeof(Header) + 1)  // Replaced HEADERSIZE
            return false;  // Min header + CRC

        unsigned char calc_crc = 0;

        for (int i = 0; i < len - 1; i++) {  // Exclude last byte (CRC)
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

        int body_len = packet.head.Length > sizeof(Header) ? packet.head.Length - sizeof(Header) : 0; // Replaced HEADERSIZE // Fixed negative body_len for empty-body packets by clamping it to 0

        int total_no_crc = sizeof(Header) + body_len;       // Replaced HEADERSIZE

        char* temp_buf = new char[total_no_crc];

        // Copy header
        memcpy(temp_buf, &packet.head, sizeof(Header));    // Replaced HEADERSIZE

        // Copy body if exists
        if (packet.Data && body_len > 0) {
            memcpy(temp_buf + sizeof(Header), packet.Data, body_len);  // Replaced HEADERSIZE
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

        CalcCRC();  // Ensure fresh CRC

        int body_len = packet.head.Length > sizeof(Header) ? packet.head.Length - sizeof(Header) : 0; // Replaced HEADERSIZE // Fixed negative body_len for empty-body packets by clamping it to 0

        int total_size = packet.head.Length + 1;  // + CRC

        RawBuffer = new char[total_size];

        // Copy header
        memcpy(RawBuffer, &packet.head, sizeof(Header));  // Replaced HEADERSIZE

        // Copy body
        if (packet.Data && body_len > 0) {
            memcpy(RawBuffer + sizeof(Header), packet.Data, body_len);  // Replaced HEADERSIZE
        }

        // Copy CRC
        memcpy(RawBuffer + sizeof(Header) + body_len, &packet.CRC, 1);  // Replaced HEADERSIZE

        return RawBuffer;  // Caller must NOT delete (or use smart pointer)
    }


    // Checks if the packet is a telemetry response 
    bool IsTelemetry() {
        return (packet.head.Status == 1);
    }

    // Checks if the packet is an Acknowledgement (ACK)
    bool IsAck() {
        return packet.head.Ack;
    }

    // Checks if the packet is a Negative Acknowledgement (NACK)
    bool IsNack() {
        return (packet.head.Ack == 0);
    }

    // Transforms raw body data into a TelemetryBody struct
    TelemetryBody GetTelemetryData() {
        TelemetryBody telemetry;

        // Zero out the struct initially to ensure it is in a safe, empty state
        memset(&telemetry, 0, sizeof(TelemetryBody));

        // 1. Verify this is actually a telemetry packet and that Data exists
        if (IsTelemetry() && packet.Data != nullptr) {

            // 2. Calculate the actual size of the body payload
            int bodySize = packet.head.Length - sizeof(Header);

            // 3. Ensure the payload is at least as large as our TelemetryBody struct
            // to prevent reading out of bounds during memcpy
            if (bodySize >= sizeof(TelemetryBody)) {

                // 4. Safely copy the raw bytes into our struct
                memcpy(&telemetry, packet.Data, sizeof(TelemetryBody));
            }
        }

        // Return the struct by value (populated if valid, zeroes if invalid)
        return telemetry;
    }

    // Utility function to print the exact contents of the packet
    void Display()
    {
        std::cout << "\n=== [ PktDef Debug Display ] ===\n";

        // --- 1. HEADER ---
        std::cout << "HEADER:\n";
        std::cout << "  PktCount : " << packet.head.PktCount << "\n";
        std::cout << "  Flags    : ";
        if (packet.head.Drive) std::cout << "[DRIVE] ";
        if (packet.head.Status) std::cout << "[RESPONSE] ";
        if (packet.head.Sleep) std::cout << "[SLEEP] ";
        if (packet.head.Ack) std::cout << "[ACK] ";
        std::cout << "\n";
        std::cout << "  Length   : " << (int)packet.head.Length << " bytes (Header + Body + CRC)\n";

        // --- 2. BODY ---
        std::cout << "BODY:\n";

        // Correctly calculate body size without the CRC byte
        int bodySize = packet.head.Length > sizeof(Header) ? packet.head.Length - sizeof(Header) - 1 : 0;

        if (packet.Data == nullptr || bodySize <= 0)
        {
            std::cout << "  (Empty Payload)\n";
        }
        else if (packet.head.Sleep)
        {
            if (bodySize > 0) {
                std::string msg(packet.Data, bodySize);
                std::cout << (packet.head.Ack ? "  [ACK Message]: " : "  [NACK Error Message]: ") << msg << "\n";
            }
            else {
                std::cout << "  (Sleep Command - Should be empty)\n";
            }
        }
        else if (packet.head.Status)
        {
            if (bodySize >= sizeof(TelemetryBody) && packet.head.Ack)
            {
                TelemetryBody t = GetTelemetryData();
                std::cout << "  [Telemetry Data]\n";
                std::cout << "    Last Pkt : " << t.LastPktCounter << "\n";
                std::cout << "    Hit Count: " << t.HitCount << "\n";
                std::cout << "    Grade    : " << t.CurrentGrade << "\n";
            }
            else
            {
                std::string msg(packet.Data, bodySize);
                std::cout << (packet.head.Ack ? "  [ACK Message]: " : "  [NACK Error Message]: ") << msg << "\n";
            }
        }
        else if (packet.head.Drive)
        {
            unsigned char firstByte = packet.Data[0];

            if ((firstByte == FORWARD || firstByte == BACKWARD) && bodySize == sizeof(DriveBody))
            {
                DriveBody* db = reinterpret_cast<DriveBody*>(packet.Data);
                std::cout << "  [DriveBody Command]\n";
                std::cout << "    Direction: " << (int)db->Direction << (firstByte == FORWARD ? " (FWD)" : " (BWD)") << "\n";
                std::cout << "    Duration : " << (int)db->Duration << " seconds\n";
                std::cout << "    Power    : " << (int)db->Power << "%\n";
            }
            else if ((firstByte == RIGHT || firstByte == LEFT) && bodySize == sizeof(TurnBody))
            {
                TurnBody* tb = reinterpret_cast<TurnBody*>(packet.Data);
                std::cout << "  [TurnBody Command]\n";
                std::cout << "    Direction: " << (int)tb->Direction << (firstByte == RIGHT ? " (RIGHT)" : " (LEFT)") << "\n";
                std::cout << "    Duration : " << tb->Duration << " seconds\n";
            }
            else
            {
                std::string msg(packet.Data, bodySize);
                if (packet.head.Ack) {
                    std::cout << "  [ACK Message]: " << msg << "\n";
                }
                else {
                    std::cout << "  [NACK Error Message]: " << msg << "\n";
                }
            }
        }

        // --- 3. TRAILER ---
        std::cout << "TRAILER:\n";
        std::cout << "  CRC      : " << (int)(unsigned char)packet.CRC << "\n";

        // --- 4. RAW BINARY ---
        std::cout << "\nRAW BINARY (Header | Body | CRC):\n  ";

        unsigned char* headPtr = reinterpret_cast<unsigned char*>(&packet.head);
        for (int i = 0; i < sizeof(Header); i++) {
            for (int b = 7; b >= 0; b--) {
                std::cout << ((headPtr[i] >> b) & 1);
            }
            std::cout << " ";
        }

        if (packet.Data != nullptr && bodySize > 0) {
            std::cout << "| ";
            unsigned char* bodyPtr = reinterpret_cast<unsigned char*>(packet.Data);
            for (int i = 0; i < bodySize; i++) {
                for (int b = 7; b >= 0; b--) {
                    std::cout << ((bodyPtr[i] >> b) & 1);
                }
                std::cout << " ";
            }
        }

        std::cout << "| ";
        for (int b = 7; b >= 0; b--) {
            std::cout << ((packet.CRC >> b) & 1);
        }

        std::cout << "\n================================\n";
    }
};

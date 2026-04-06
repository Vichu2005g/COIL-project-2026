#include "Packet.h"
#include <cstring>
#include <iomanip>

PktDef::PktDef() : RawBuffer(nullptr) {
    memset(&packet.head, 0, sizeof(Header));
    packet.Data = nullptr;
    packet.CRC = 0;
}

PktDef::PktDef(char* src) : RawBuffer(nullptr) {
    if (src == nullptr) return;
    packet.Data = nullptr;

    // Extract header
    memcpy(&packet.head, src, sizeof(Header));

    int bodySize = packet.head.Length - sizeof(Header);

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

PktDef::~PktDef() {
    if (packet.Data) {
        delete[] packet.Data;
        packet.Data = nullptr;
    }
    if (RawBuffer) {
        delete[] RawBuffer;
        RawBuffer = nullptr;
    }
}

void PktDef::SetCmd(CmdType cmd) {
    packet.head.Drive = 0;
    packet.head.Status = 0;
    packet.head.Sleep = 0;

    switch (cmd) {
    case DRIVE: packet.head.Drive = 1; break;
    case SLEEP: packet.head.Sleep = 1; break;
    case RESPONSE: packet.head.Status = 1; break;
    }
}

void PktDef::SetBodyData(char* src, int size) {
    if (packet.Data)
        delete[] packet.Data;

    if (size > 0 && src != nullptr) {
        packet.Data = new char[size];
        memcpy(packet.Data, src, size);
        packet.head.Length = sizeof(Header) + size + 1;
    } else {
        packet.Data = nullptr;
        packet.head.Length = sizeof(Header) + 1;
    }
}

void PktDef::SetAck(bool value) {
    packet.head.Ack = value ? 1 : 0;
}

void PktDef::SetPktCount(int value) {
    packet.head.PktCount = static_cast<unsigned short>(value);
}

CmdType PktDef::GetCmd() {
    if (packet.head.Drive == 1) return DRIVE;
    if (packet.head.Sleep == 1) return SLEEP;
    return RESPONSE;
}

bool PktDef::GetAck() {
    return (packet.head.Ack != 0);
}

int PktDef::GetLength() {
    return packet.head.Length;
}

char* PktDef::GetBodyData() {
    return packet.Data;
}

int PktDef::GetPktCount() {
    return packet.head.PktCount;
}

bool PktDef::CheckCRC(char* buf, int len) {
    if (buf == nullptr || len < sizeof(Header) + 1)
        return false;

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

void PktDef::CalcCRC() {
    unsigned char crc = 0;
    int body_len = packet.head.Length > sizeof(Header) ? packet.head.Length - sizeof(Header) : 0;
    int total_no_crc = sizeof(Header) + body_len;

    char* temp_buf = new char[total_no_crc];
    memcpy(temp_buf, &packet.head, sizeof(Header));
    if (packet.Data && body_len > 0) {
        memcpy(temp_buf + sizeof(Header), packet.Data, body_len);
    }

    for (int i = 0; i < total_no_crc; i++) {
        unsigned char byte = (unsigned char)temp_buf[i];
        while (byte) {
            crc += byte & 1;
            byte >>= 1;
        }
    }
    crc %= 256;
    delete[] temp_buf;
    packet.CRC = crc;
}

char* PktDef::GenPacket() {
    if (RawBuffer) {
        delete[] RawBuffer;
        RawBuffer = nullptr;
    }

    CalcCRC();

    int body_len = packet.head.Length > sizeof(Header) ? packet.head.Length - sizeof(Header) : 0;
    int total_size = packet.head.Length + 1;

    RawBuffer = new char[total_size];
    memcpy(RawBuffer, &packet.head, sizeof(Header));
    if (packet.Data && body_len > 0) {
        memcpy(RawBuffer + sizeof(Header), packet.Data, body_len);
    }
    memcpy(RawBuffer + sizeof(Header) + body_len, &packet.CRC, 1);

    return RawBuffer;
}

bool PktDef::IsTelemetry() {
    return (packet.head.Status == 1);
}

bool PktDef::IsAck() {
    return (packet.head.Ack == 1);
}

bool PktDef::IsNack() {
    return (packet.head.Ack == 0);
}

TelemetryBody PktDef::GetTelemetryData() {
    TelemetryBody telemetry;
    memset(&telemetry, 0, sizeof(TelemetryBody));
    if (IsTelemetry() && packet.Data != nullptr) {
        int bodySize = packet.head.Length - sizeof(Header);
        if (bodySize >= sizeof(TelemetryBody)) {
            memcpy(&telemetry, packet.Data, sizeof(TelemetryBody));
        }
    }
    return telemetry;
}

void PktDef::Display() {
    std::cout << "\n=== [ PktDef Debug Display ] ===\n";
    std::cout << "  PktCount : " << packet.head.PktCount << "\n";
    std::cout << "  Flags    : ";
    if (packet.head.Drive) std::cout << "[DRIVE] ";
    if (packet.head.Status) std::cout << "[RESPONSE] ";
    if (packet.head.Sleep) std::cout << "[SLEEP] ";
    if (packet.head.Ack) std::cout << "[ACK] ";
    std::cout << "\n";
    std::cout << "  Length   : " << (int)packet.head.Length << " bytes\n";
    std::cout << "  CRC      : " << (int)(unsigned char)packet.CRC << "\n";
    std::cout << "================================\n";
}

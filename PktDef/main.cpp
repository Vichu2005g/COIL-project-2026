// CSCN72050 - COIL Project Milestone 2
// Robot Command and Control Console (Fixed & Enhanced)

#include <iostream>
#include <string>
#include <iomanip>
#include <vector>
#include <ctime>
#include "Socket.h"
#include "Packet.h"

#pragma comment(lib, "ws2_32.lib")

#define CLR_RESET  "\033[0m"
#define CLR_GREEN  "\033[92m"
#define CLR_RED    "\033[91m"
#define CLR_YELLOW "\033[93m"
#define CLR_CYAN   "\033[96m"
#define CLR_WHITE  "\033[97m"
#define CLR_GRAY   "\033[90m"

// --- Global State ---
static int g_pktCount = 0;
static int g_sent = 0;
static int g_ackOK = 0;
static int g_ackBad = 0;

static std::string g_ip = "10.172.41.150";
static int g_udpPort = 29500;
static int g_tcpPort = 29000;
static ConnectionType g_proto = UDP;
static MySocket* g_sock = nullptr;

// --- Helpers ---
std::string Now() {
    time_t t = time(nullptr);
    struct tm tm; localtime_s(&tm, &t);
    char buf[9]; strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
    return buf;
}

void Cls() { system("cls"); }

std::string DirectionName(int d) {
    switch (d) {
    case FORWARD:  return "Forward";
    case BACKWARD: return "Backward";
    case LEFT:     return "Left";
    case RIGHT:    return "Right";
    default:       return "Unknown";
    }
}

// --- Connection Management ---
bool EnsureConnected() {
    if (g_sock != nullptr && g_sock->IsConnectionSocketValid()) return true;

    int currentPort = (g_proto == UDP) ? g_udpPort : g_tcpPort;
    g_sock = new MySocket(CLIENT, g_ip, currentPort, g_proto, 1024);

    std::cout << CLR_GRAY << "  Connecting to robot..." << CLR_RESET << std::endl;

    if (g_proto == TCP) {
        g_sock->ConnectTCP();
    }
    else {
        // FIX: SimTest.cpp forgot to call this, meaning the UDP socket was never actually created!
        g_sock->ConnectUDP();
    }
    return g_sock->IsConnectionSocketValid();
}

void Disconnect() {
    if (g_sock == nullptr) return;
    if (g_proto == TCP) g_sock->DisconnectTCP();
    if (g_proto == UDP) g_sock->DisconnectUDP();
    delete g_sock;
    g_sock = nullptr;
}

// --- UI Blocks ---
void PrintHeader() {
    std::cout << CLR_CYAN
        << "+--------------------------------------------------------------+\n"
        "|        COIL ROBOT COMMAND AND CONTROL CONSOLE                |\n"
        "|                CSCN72050  -  Milestone 2                     |\n"
        "+--------------------------------------------------------------+\n"
        << CLR_RESET;
}

void PrintStatus() {
    std::string proto = (g_proto == UDP) ? "UDP" : "TCP";
    int port = (g_proto == UDP) ? g_udpPort : g_tcpPort;
    std::string connState = (g_sock != nullptr && g_sock->IsConnectionSocketValid())
        ? (CLR_GREEN + std::string("CONNECTED"))
        : (CLR_RED + std::string("DISCONNECTED"));

    std::cout
        << CLR_WHITE << "+- Robot Status: " << connState
        << CLR_WHITE << " -----------------------------------------------+\n"
        << "|  Target   : " << CLR_YELLOW << std::left << std::setw(18) << g_ip
        << CLR_WHITE << "  Port : " << CLR_YELLOW << std::setw(6) << port
        << CLR_WHITE << "  Protocol : " << CLR_YELLOW << std::setw(6) << proto
        << CLR_WHITE << "       |\n"
        << "|  Sent     : " << CLR_GREEN << std::setw(4) << g_sent
        << CLR_WHITE << "  Accepted : " << CLR_GREEN << std::setw(4) << g_ackOK
        << CLR_WHITE << "  Rejected : " << CLR_RED << std::setw(4) << g_ackBad
        << CLR_WHITE << "  Packet # : " << CLR_CYAN << std::setw(5) << g_pktCount
        << CLR_WHITE << "  |\n"
        << "+---------------------------------------------------------------+\n"
        << CLR_RESET;
}

void PrintMenu() {
    std::cout << "\n" << CLR_WHITE << "  What would you like the robot to do?\n\n"
        << "  " << CLR_CYAN << "1" << CLR_WHITE << "  Drive Forward\n"
        << "  " << CLR_CYAN << "2" << CLR_WHITE << "  Drive Backward\n"
        << "  " << CLR_CYAN << "3" << CLR_WHITE << "  Turn Left\n"
        << "  " << CLR_CYAN << "4" << CLR_WHITE << "  Turn Right\n"
        << "  " << CLR_CYAN << "5" << CLR_WHITE << "  Put Robot to Sleep (resets robot)\n"
        << "  " << CLR_CYAN << "6" << CLR_WHITE << "  Request Robot Telemetry\n"
        << "  " << CLR_CYAN << "7" << CLR_WHITE << "  Change Protocol (TCP/UDP)\n"
        << "  " << CLR_CYAN << "0" << CLR_WHITE << "  Quit\n"
        << CLR_RESET;
}

// --- Core Transmission Logic ---
void SendAndReceive(PktDef& pkt, CmdType sentCmd) {
    if (!EnsureConnected()) {
        std::cout << CLR_RED << "\n  Failed to establish network socket.\n" << CLR_RESET;
        return;
    }

    char* raw = pkt.GenPacket();

    // FIX: GetLength() only returns Header+Body. We MUST add +1 for the CRC tail.
    int totalLen = pkt.GetLength() + 1;

    std::cout << "\n" << CLR_CYAN << "  +- Sending Packet ------------------------------------------\n" << CLR_RESET;
    pkt.Display(); // Uses your new display function to show exact bytes

    g_sent++;
    g_sock->SendData(raw, totalLen);

    // Wait briefly for simulator to process
    Sleep(500);

    char recvBuf[1024] = { 0 };
    int rlen = g_sock->GetData(recvBuf);

    std::cout << "\n" << CLR_CYAN << "  +- Robot Reply ---------------------------------------------\n" << CLR_RESET;

    if (rlen > 0) {
        PktDef resp(recvBuf);
        resp.Display();

        bool crcOK = resp.CheckCRC(recvBuf, rlen);
        bool ackOK = resp.GetAck();

        if (crcOK) {
            if (ackOK) {
                g_ackOK++;
                std::cout << CLR_GREEN << "  [SUCCESS] Robot ACCEPTED the command.\n" << CLR_RESET;
            }
            else {
                g_ackBad++;
                std::cout << CLR_RED << "  [FAILED] Robot REJECTED the command (NACK).\n" << CLR_RESET;
            }
        }
        else {
            g_ackBad++;
            std::cout << CLR_RED << "  [ERROR] Data corrupted. CRC Check Failed!\n" << CLR_RESET;
        }
    }
    else {
        g_ackBad++;
        std::cout << CLR_RED << "  [TIMEOUT] No reply received from the simulator.\n" << CLR_RESET;
    }
}

// --- Main Application ---
int main() {
    // Enable ANSI colours on Windows Console
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD m = 0; GetConsoleMode(h, &m);
    SetConsoleMode(h, m | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    int choice = -1;
    while (true) {
        Cls();
        PrintHeader();
        PrintStatus();
        PrintMenu();

        std::cout << "\n" << CLR_WHITE << "  Enter choice: " << CLR_YELLOW;
        if (!(std::cin >> choice)) {
            std::cin.clear(); std::cin.ignore(10000, '\n'); continue;
        }
        std::cout << CLR_RESET;

        if (choice == 0) break;

        if (choice == 7) {
            Disconnect();
            g_proto = (g_proto == UDP) ? TCP : UDP;
            std::cout << CLR_GREEN << "  Protocol switched to " << (g_proto == UDP ? "UDP" : "TCP") << ".\n" << CLR_RESET;
            Sleep(1000);
            continue;
        }

        PktDef pkt;
        pkt.SetPktCount(++g_pktCount);

        int duration = 0, power = 0;

        switch (choice) {
        case 1:
        case 2:
            std::cout << CLR_WHITE << "\n  How many seconds? (1-255): " << CLR_YELLOW;
            std::cin >> duration;
            std::cout << CLR_WHITE << "  Motor power (80-100%): " << CLR_YELLOW;
            std::cin >> power;

            pkt.SetCmd(DRIVE);
            {
                DriveBody b;
                b.Direction = (choice == 1) ? FORWARD : BACKWARD;
                b.Duration = (unsigned char)duration;
                b.Power = (unsigned char)power;
                pkt.SetBodyData(reinterpret_cast<char*>(&b), sizeof(DriveBody));
            }
            SendAndReceive(pkt, DRIVE);
            break;

        case 3:
        case 4:
            std::cout << CLR_WHITE << "\n  How many seconds? (1-255): " << CLR_YELLOW;
            std::cin >> duration;

            pkt.SetCmd(DRIVE);
            {
                TurnBody b;
                b.Direction = (choice == 3) ? LEFT : RIGHT;
                b.Duration = (unsigned short)duration;
                pkt.SetBodyData(reinterpret_cast<char*>(&b), sizeof(TurnBody));
            }
            SendAndReceive(pkt, DRIVE);
            break;

        case 5:
            std::cout << CLR_WHITE << "  This resets the robot. Continue? (y/n): " << CLR_YELLOW;
            char confirm; std::cin >> confirm;
            if (toupper(confirm) == 'Y') {
                pkt.SetCmd(SLEEP);
                pkt.SetBodyData(nullptr, 0);
                SendAndReceive(pkt, SLEEP);
            }
            else {
                g_pktCount--;
            }
            break;

        case 6:
            pkt.SetCmd(RESPONSE);
            pkt.SetBodyData(nullptr, 0);
            SendAndReceive(pkt, RESPONSE);
            break;

        default:
            std::cout << CLR_RED << "\n  Invalid selection.\n" << CLR_RESET;
            Sleep(1000);
            continue;
        }

        std::cout << "\n" << CLR_GRAY << "  Press Enter to continue..." << CLR_RESET;
        std::cin.ignore(); std::cin.get();
    }

    Disconnect();
    Cls(); PrintHeader();
    std::cout << "\n" << CLR_CYAN << "  Session Summary\n" << CLR_RESET
        << CLR_WHITE << "  Commands Sent     : " << CLR_YELLOW << g_sent << "\n"
        << CLR_WHITE << "  Commands Accepted : " << CLR_GREEN << g_ackOK << "\n"
        << CLR_WHITE << "  Commands Rejected : " << CLR_RED << g_ackBad << "\n\n"
        << CLR_GRAY << "  Connection closed. Goodbye.\n\n" << CLR_RESET;
    return 0;
}
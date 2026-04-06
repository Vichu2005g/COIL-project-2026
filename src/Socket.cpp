#include "Socket.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

MySocket::MySocket(SocketType type, std::string ip, unsigned int port, ConnectionType connType, unsigned int size)
{
    mySocket = type;
    IPAddr = ip;
    Port = port;
    connectionType = connType;
    bTCPConnect = false;

    if (size == 0)
        MaxSize = DEFAULT_SIZE;
    else
        MaxSize = size;

    Buffer = new char[MaxSize];
    memset(Buffer, 0, MaxSize);

    WelcomeSocket = INVALID_SOCKET;
    ConnectionSocket = INVALID_SOCKET;
    memset(&SvrAddr, 0, sizeof(SvrAddr));

#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        std::cout << "WSAStartup failed with error: " << result << std::endl;
    }
#endif

    SvrAddr.sin_family = AF_INET;
    SvrAddr.sin_port = htons(Port);
    inet_pton(AF_INET, IPAddr.c_str(), &SvrAddr.sin_addr);
}

MySocket::~MySocket()
{
    if (Buffer != nullptr)
    {
        delete[] Buffer;
        Buffer = nullptr;
    }

    if (ConnectionSocket != INVALID_SOCKET)
    {
        closesocket(ConnectionSocket);
        ConnectionSocket = INVALID_SOCKET;
    }

    if (WelcomeSocket != INVALID_SOCKET)
    {
        closesocket(WelcomeSocket);
        WelcomeSocket = INVALID_SOCKET;
    }

#ifdef _WIN32
    WSACleanup();
#endif
}

bool MySocket::IsConnectionSocketValid() {
    return ConnectionSocket != INVALID_SOCKET;
}

void MySocket::ConnectTCP()
{
    if (connectionType == UDP) return;
    if (bTCPConnect) return;

    if (mySocket == CLIENT)
    {
        ConnectionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (ConnectionSocket == INVALID_SOCKET) return;

        int result = connect(ConnectionSocket, (sockaddr*)&SvrAddr, sizeof(SvrAddr));
        if (result == SOCKET_ERROR)
        {
            closesocket(ConnectionSocket);
            ConnectionSocket = INVALID_SOCKET;
            return;
        }

        bTCPConnect = true;
    }
    else if (mySocket == SERVER)
    {
        WelcomeSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (WelcomeSocket == INVALID_SOCKET) return;

        if (bind(WelcomeSocket, (sockaddr*)&SvrAddr, sizeof(SvrAddr)) == SOCKET_ERROR)
        {
            closesocket(WelcomeSocket);
            WelcomeSocket = INVALID_SOCKET;
            return;
        }

        if (listen(WelcomeSocket, SOMAXCONN) == SOCKET_ERROR)
        {
            closesocket(WelcomeSocket);
            WelcomeSocket = INVALID_SOCKET;
            return;
        }

        ConnectionSocket = accept(WelcomeSocket, nullptr, nullptr);
        if (ConnectionSocket != INVALID_SOCKET)
        {
            bTCPConnect = true;
        }
    }
}

void MySocket::DisconnectTCP()
{
    if (connectionType != TCP) return;
    if (bTCPConnect)
    {
        shutdown(ConnectionSocket, SD_BOTH);
        closesocket(ConnectionSocket);
        bTCPConnect = false;
        ConnectionSocket = INVALID_SOCKET;
    }
}

void MySocket::ConnectUDP()
{
    if (connectionType != UDP) return;

    ConnectionSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ConnectionSocket == INVALID_SOCKET) return;

    if (mySocket == SERVER)
    {
        if (bind(ConnectionSocket, (struct sockaddr*)&SvrAddr, sizeof(SvrAddr)) == SOCKET_ERROR)
        {
            closesocket(ConnectionSocket);
            ConnectionSocket = INVALID_SOCKET;
            return;
        }
    }
}

void MySocket::DisconnectUDP()
{
    if (connectionType != UDP) return;
    if (ConnectionSocket != INVALID_SOCKET)
    {
        closesocket(ConnectionSocket);
        ConnectionSocket = INVALID_SOCKET;
    }
}

void MySocket::SendData(const char* data, int len)
{
    if (connectionType == TCP)
    {
        if (bTCPConnect)
            send(ConnectionSocket, data, len, 0);
    }
    else
    {
        sendto(ConnectionSocket, data, len, 0, (sockaddr*)&SvrAddr, sizeof(SvrAddr));
    }
}

int MySocket::GetData(char* dest)
{
    if (dest == nullptr) return -1;
    int bytesReceived = 0;
    memset(Buffer, 0, MaxSize);

    if (connectionType == TCP)
    {
        bytesReceived = recv(ConnectionSocket, Buffer, MaxSize, 0);
    }
    else
    {
        sockaddr_in senderAddr;
        socklen_t len = sizeof(struct sockaddr_in);
        bytesReceived = recvfrom(ConnectionSocket, Buffer, MaxSize, 0, (sockaddr*)&senderAddr, &len);
    }

    if (bytesReceived > 0)
        memcpy(dest, Buffer, bytesReceived);

    return bytesReceived;
}

std::string MySocket::GetIPAddr() { return IPAddr; }
void MySocket::SetIPAddr(std::string ip) { IPAddr = ip; inet_pton(AF_INET, IPAddr.c_str(), &SvrAddr.sin_addr); }
void MySocket::SetPort(int port) { Port = port; SvrAddr.sin_port = htons(Port); }
int MySocket::GetPort() { return Port; }
bool MySocket::IsConnected() const { return bTCPConnect; }
SocketType MySocket::GetType() { return mySocket; }
void MySocket::SetType(SocketType type) { mySocket = type; }
SOCKET MySocket::GetConnectionSocket() const { return ConnectionSocket; }
SOCKET MySocket::GetWelcomeSocket() const { return WelcomeSocket; }

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

void MySocket::SetTimeout(int ms) {
    if (ConnectionSocket == INVALID_SOCKET) return;
#ifdef _WIN32
    DWORD timeout = ms;
    setsockopt(ConnectionSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    setsockopt(ConnectionSocket, SOL_SOCKET, SO_RCVTIMEO, (const struct timeval*)&tv, sizeof(tv));
#endif
}

bool MySocket::IsConnectionSocketValid() {
    return ConnectionSocket != INVALID_SOCKET;
}

void MySocket::SetBlocking(bool blocking) {
    if (ConnectionSocket == INVALID_SOCKET) return;
#ifdef _WIN32
    unsigned long mode = blocking ? 0 : 1;
    ioctlsocket(ConnectionSocket, FIONBIO, &mode);
#else
    int flags = fcntl(ConnectionSocket, F_GETFL, 0);
    if (blocking) flags &= ~O_NONBLOCK;
    else flags |= O_NONBLOCK;
    fcntl(ConnectionSocket, F_SETFL, flags);
#endif
}

void MySocket::ConnectTCP()
{
    if (connectionType == UDP) return;
    if (bTCPConnect) return;

    if (mySocket == CLIENT)
    {
        ConnectionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (ConnectionSocket == INVALID_SOCKET) return;

        SetTimeout(1000);
        SetBlocking(false);

        int result = connect(ConnectionSocket, (sockaddr*)&SvrAddr, sizeof(SvrAddr));
        
        bool connected = false;
        if (result == 0) {
            connected = true;
        } else {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
#else
            int err = errno;
            if (err == EINPROGRESS) {
#endif
                fd_set write_fds;
                FD_ZERO(&write_fds);
                FD_SET(ConnectionSocket, &write_fds);
                
                struct timeval tv;
                tv.tv_sec = 3; // 3 second timeout for connection
                tv.tv_usec = 0;
                
                if (select((int)ConnectionSocket + 1, NULL, &write_fds, NULL, &tv) > 0) {
                    int optVal;
                    socklen_t optLen = sizeof(int);
                    getsockopt(ConnectionSocket, SOL_SOCKET, SO_ERROR, (char*)&optVal, &optLen);
                    if (optVal == 0) connected = true;
                }
            }
        }

        SetBlocking(true);

        if (!connected)
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

    // Set 1000ms timeout for robustness
    SetTimeout(1000);

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

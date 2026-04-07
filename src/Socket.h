#pragma once
#include <string>
#include <iostream>
#include <cstring>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <sys/time.h>
    #include <fcntl.h>
    typedef int SOCKET;
    #define INVALID_SOCKET  (SOCKET)(~0)
    #define SOCKET_ERROR            (-1)
    #define SD_BOTH SHUT_RDWR
    #define closesocket close
#endif

enum SocketType
{
    CLIENT,
    SERVER
};

enum ConnectionType
{
    TCP,
    UDP
};

const int DEFAULT_SIZE = 1024;

class MySocket
{
private:
    char* Buffer;
    SOCKET WelcomeSocket;
    SOCKET ConnectionSocket;
    sockaddr_in SvrAddr;

    SocketType mySocket;
    std::string IPAddr;
    int Port;
    ConnectionType connectionType;
    bool bTCPConnect;
    int MaxSize;

    void SetBlocking(bool blocking);

public:
    MySocket(SocketType type, std::string ip, unsigned int port, ConnectionType connType, unsigned int size = DEFAULT_SIZE);
    ~MySocket();

    bool IsConnectionSocketValid();
    void ConnectTCP();
    void DisconnectTCP();
    void ConnectUDP();
    void DisconnectUDP();
    void SendData(const char* data, int len);
    int GetData(char* dest);
    void SetTimeout(int ms);

    std::string GetIPAddr();
    void SetIPAddr(std::string ip);
    void SetPort(int port);
    int GetPort();
    bool IsConnected() const;
    SocketType GetType();
    void SetType(SocketType type);

    SOCKET GetConnectionSocket() const;
    SOCKET GetWelcomeSocket() const;
};

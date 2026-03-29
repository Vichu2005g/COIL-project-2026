#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

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

public:
    MySocket(SocketType type, std::string ip, unsigned int port, ConnectionType connType, unsigned int size) // implemented by 
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

        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);

        SvrAddr.sin_family = AF_INET;
        SvrAddr.sin_port = htons(Port);
        inet_pton(AF_INET, IPAddr.c_str(), &SvrAddr.sin_addr);

        // TODO:
        // create socket here
        // bind if server
        // listen if TCP server
    }

    ~MySocket()
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

        WSACleanup();
    } 

    void ConnectTCP()
    {
        // TODO:
        // if UDP, do nothing or error
        // if CLIENT TCP -> connect()
        // if SERVER TCP -> accept()
        // set bTCPConnect = true
	} // implemented by Jason Little
     
    void DisconnectTCP()
    {
        // TODO:
        // shutdown and close TCP socket
        // bTCPConnect = false
	} // implemented by Vishwaanth

    void ConnectUDP() {

	} // implemented by Anh Dung Phan

    void DisconnectUDP() {

	} // implemented by Ann Dung Phan

    void SendData(const char* data, int len)
    {
        // TODO:
        // TCP -> send()
        // UDP -> sendto()
	} // implemented by Ann Dung Phan

	int GetData(char* dest) // implemented by Amna
    {
        // TODO:
        // TCP -> recv()
        // UDP -> recvfrom()
        // copy Buffer to dest
        return 0;
    } 

    std::string GetIPAddr()
    {
        return IPAddr;
    }

    void SetIPAddr(std::string ip)
    {
        if (bTCPConnect)
        {
            std::cout << "Cannot change IP while connected.\n";
            return;
        }

        IPAddr = ip;
        inet_pton(AF_INET, IPAddr.c_str(), &SvrAddr.sin_addr);
    }

    void SetPort(int port)
    {
        if (bTCPConnect)
        {
            std::cout << "Cannot change port while connected.\n";
            return;
        }

        Port = port;
        SvrAddr.sin_port = htons(Port);
    }

    int GetPort()
    {
        return Port;
    }

    SocketType GetType()
    {
        return mySocket;
    }

    void SetType(SocketType type)
    {
        if (bTCPConnect || WelcomeSocket != INVALID_SOCKET)
        {
            std::cout << "Cannot change type while socket is active.\n";
            return;
        }

        mySocket = type;
    }
};
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
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);

        if (result != 0)
        {
            std::cout << "WSAStartup failed with error: " << result << std::endl;
        }

        SvrAddr.sin_family = AF_INET;
        SvrAddr.sin_port = htons(Port);
        inet_pton(AF_INET, IPAddr.c_str(), &SvrAddr.sin_addr);

        // TODO:
        // create socket here
        // bind if server
        // listen if TCP server

        if (mySocket == TCP)
            WelcomeSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        else
            WelcomeSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        // Bind if server
        if (connectionType == SERVER)
        {
            bind(WelcomeSocket, (sockaddr*)&SvrAddr, sizeof(SvrAddr));

            // Listen if TCP server
            if (mySocket == TCP)
            {
                listen(WelcomeSocket, SOMAXCONN);
            }
        }
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

    // Implemented by Jason Little
    void ConnectTCP()
    {
        // 1. Prevent UDP from using TCP connect logic
        if (connectionType == UDP)
        {
            std::cout << "ERROR: Cannot call ConnectTCP() on a UDP socket.\n";
            return;
        }

        // 2. Prevent reconnecting if already connected
        if (bTCPConnect)
        {
            std::cout << "TCP connection already established.\n";
            return;
        }


        // CLIENT TCP LOGIC
        if (mySocket == CLIENT)
        {
            ConnectionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (ConnectionSocket == INVALID_SOCKET)
            {
                std::cout << "ERROR: Failed to create TCP client socket.\n";
                return;
            }

            int result = connect(ConnectionSocket, (sockaddr*)&SvrAddr, sizeof(SvrAddr));
            if (result == SOCKET_ERROR)
            {
                std::cout << "ERROR: TCP client connect() failed.\n";
                closesocket(ConnectionSocket);
                ConnectionSocket = INVALID_SOCKET;
                return;
            }

            bTCPConnect = true;
            std::cout << "TCP CLIENT connected successfully.\n";
            return;
        }


        // SERVER TCP LOGIC
        if (mySocket == SERVER)
        {
            WelcomeSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (WelcomeSocket == INVALID_SOCKET)
            {
                std::cout << "ERROR: Failed to create TCP server socket.\n";
                return;
            }

            // Bind server socket
            if (bind(WelcomeSocket, (sockaddr*)&SvrAddr, sizeof(SvrAddr)) == SOCKET_ERROR)
            {
                std::cout << "ERROR: TCP server bind() failed.\n";
                closesocket(WelcomeSocket);
                WelcomeSocket = INVALID_SOCKET;
                return;
            }

            // Listen for incoming connections
            if (listen(WelcomeSocket, SOMAXCONN) == SOCKET_ERROR)
            {
                std::cout << "ERROR: TCP server listen() failed.\n";
                closesocket(WelcomeSocket);
                WelcomeSocket = INVALID_SOCKET;
                return;
            }

            std::cout << "TCP SERVER listening...\n";

            // Accept a client
            ConnectionSocket = accept(WelcomeSocket, nullptr, nullptr);
            if (ConnectionSocket == INVALID_SOCKET)
            {
                std::cout << "ERROR: TCP server accept() failed.\n";
                closesocket(WelcomeSocket);
                WelcomeSocket = INVALID_SOCKET;
                return;
            }

            bTCPConnect = true;
            std::cout << "TCP SERVER accepted a client.\n";
        }
    }
     
    void DisconnectTCP()
    {
        // TODO:
        // shutdown and close TCP socket
        // bTCPConnect = false

        // Prevent UDP from calling TCP disconnect
        if (mySocket != TCP)
            return;

        if (bTCPConnect)
        {
            shutdown(ConnectionSocket, SD_BOTH);
            closesocket(ConnectionSocket);

            bTCPConnect = false;
            ConnectionSocket = INVALID_SOCKET;
        }

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
        if (dest == nullptr)
            return -1;

        int bytesReceived = 0;

        // clearing internal buffer
        memset(Buffer, 0, MaxSize);

        if (connectionType == TCP)
        {
            // receive data from TCP connection
            bytesReceived = recv(ConnectionSocket, Buffer, MaxSize, 0);
        }
        else // UDP
        {
            int addrLen = sizeof(SvrAddr);
            bytesReceived = recvfrom(ConnectionSocket, Buffer, MaxSize, 0,
                                     (sockaddr*)&SvrAddr, &addrLen);
        }

        // If error or nothing received
        if (bytesReceived <= 0)
            return bytesReceived;

        // Copying data from internal buffer to destination
        memcpy(dest, Buffer, bytesReceived);

        return bytesReceived;
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

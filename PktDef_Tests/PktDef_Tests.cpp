#include "pch.h"
#include "CppUnitTest.h"
#include <thread>
#include <chrono>
#include "../PktDef/Socket.h"
#include "../PktDef/Packet.h"
#include "../PktDef/Socket.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Helper to initialize WSA for the raw listener sockets
void EnsureWSA()
{
	WSADATA wsaData{};
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	Assert::AreEqual(0, result, L"WSAStartup failed.");
}

// Helper to create a raw listener/receiver socket to validate MySocket's SendData
SOCKET CreateTestListener(ConnectionType connType, const std::string& ip, int port, DWORD timeoutMs = 1000)
{
	SOCKET listener = socket(AF_INET, (connType == TCP) ? SOCK_STREAM : SOCK_DGRAM, (connType == TCP) ? IPPROTO_TCP : IPPROTO_UDP);

	sockaddr_in serverAddr{};
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(port);
	inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

	bind(listener, (sockaddr*)&serverAddr, sizeof(serverAddr));

	// Only TCP needs to be placed into a listening state
	if (connType == TCP) {
		listen(listener, 1);
	}

	// Apply timeout so tests don't hang if data is never received
	setsockopt(listener, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));

	return listener;
}

namespace PktDefTests
{
	TEST_CLASS(PktDefTests)
	{
	public:

		TEST_METHOD(Test_DefaultConstructor)
		{
			// Arrange & Act
			PktDef pkt;

			// Assert
			Assert::AreEqual(0, pkt.GetPktCount());
			Assert::AreEqual(0, pkt.GetLength());
			Assert::IsFalse(pkt.GetAck());
		}

		TEST_METHOD(Test_SetGetPktCount)
		{
			// Arrange
			PktDef pkt;

			// Act
			pkt.SetPktCount(25);

			// Assert
			Assert::AreEqual(25, pkt.GetPktCount());
		}

		TEST_METHOD(Test_SetCmd_Drive)
		{
			// Arrange
			PktDef pkt;

			// Act
			pkt.SetCmd(DRIVE);

			// Assert
			Assert::AreEqual((int)DRIVE, (int)pkt.GetCmd());
		}

		TEST_METHOD(Test_SetGetBodyData)
		{
			// Arrange
			PktDef pkt;

			char data[] = { FORWARD, 5, 80 };

			// Act
			pkt.SetBodyData(data, 3);
			char* body = pkt.GetBodyData();

			// Assert
			Assert::AreEqual((int)data[0], (int)body[0]);
			Assert::AreEqual((int)data[1], (int)body[1]);
			Assert::AreEqual((int)data[2], (int)body[2]);
		}

		TEST_METHOD(Test_CRC_ValidPacket)
		{
			// Arrange
			PktDef pkt;

			pkt.SetPktCount(1);
			pkt.SetCmd(DRIVE);

			char data[] = { FORWARD, 5, 80 };
			pkt.SetBodyData(data, 3);

			// Act
			char* raw = pkt.GenPacket();

			bool result = pkt.CheckCRC(raw, pkt.GetLength() + 1); // +1 for CRC

			// Assert
			Assert::IsTrue(result);
		}

		TEST_METHOD(Test_CRC_InvalidPacket)
		{
			// Arrange
			PktDef pkt;
			pkt.SetPktCount(1);
			pkt.SetCmd(DRIVE);

			char data[] = { FORWARD, 5, 80 };
			pkt.SetBodyData(data, 3);

			char* raw = pkt.GenPacket();

			// Corrupt a byte to make CRC invalid
			raw[HEADERSIZE] ^= 0xFF;  // Flip bits in first body byte

			// Act
			bool result = pkt.CheckCRC(raw, pkt.GetLength() + 1);

			// Assert
			Assert::IsFalse(result);
		}

		TEST_METHOD(Test_SleepCommand_EmptyBody)
		{
			// Arrange
			PktDef pkt;
			pkt.SetCmd(SLEEP);
			pkt.SetPktCount(10);

			pkt.SetBodyData(nullptr, 0); // ensures Length = sizeof(Header)

			// Act
			char* raw = pkt.GenPacket();

			// Assert
			Assert::AreEqual((int)SLEEP, (int)pkt.GetCmd());
			Assert::AreEqual((int)sizeof(Header), pkt.GetLength());
			Assert::IsTrue(pkt.CheckCRC(raw, pkt.GetLength() + 1));
		}

		TEST_METHOD(Test_DriveCommand_MaxPower)
		{
			// Arrange
			PktDef pkt;
			pkt.SetCmd(DRIVE);

			char data[] = { FORWARD, 5, 100 };  // Max allowed power
			pkt.SetBodyData(data, 3);

			// Act
			char* raw = pkt.GenPacket();

			// Assert
			char* body = pkt.GetBodyData();
			Assert::AreEqual(100, (int)body[2]);
			Assert::IsTrue(pkt.CheckCRC(raw, pkt.GetLength() + 1));
		}

		TEST_METHOD(Test_AckFlag)
		{
			// Arrange
			PktDef pkt;
			pkt.SetCmd(RESPONSE);
			pkt.SetPktCount(5);

			// Manually set Ack
			pkt.GetAck(); // should be false initially
			pkt.GenPacket(); // generate to include CRC

			// Act
			pkt.SetCmd(RESPONSE); // just re-assert command
			pkt.GetAck();

			// Assert
			Assert::IsFalse(pkt.GetAck());  // Ack is not automatically set
		}

		TEST_METHOD(Test_Serialize_Deserialize)
		{
			// Arrange
			PktDef pkt1;

			pkt1.SetPktCount(7);
			pkt1.SetCmd(DRIVE);

			char data[] = { FORWARD, 10, 90 };
			pkt1.SetBodyData(data, 3);

			// Act
			char* raw = pkt1.GenPacket();

			PktDef pkt2(raw); // reconstruct from raw buffer

			// Assert
			Assert::AreEqual(pkt1.GetPktCount(), pkt2.GetPktCount());
			Assert::AreEqual((int)pkt1.GetCmd(), (int)pkt2.GetCmd());
			Assert::AreEqual(pkt1.GetLength(), pkt2.GetLength());

			char* body1 = pkt1.GetBodyData();
			char* body2 = pkt2.GetBodyData();

			for (int i = 0; i < 3; i++)
			{
				Assert::AreEqual(
					(int)(unsigned char)body1[i],
					(int)(unsigned char)body2[i]
				);

			}

			Assert::IsTrue(pkt2.CheckCRC(raw, pkt2.GetLength() + 1));
		}

	};

	TEST_CLASS(SocketTests)
	{
	public:

		TEST_METHOD(Test_TCP_Disconnect) 
		{
			// Arrange - Create a TCP Client (doesn't need to be officially connected to test flag reset via DisconnectTCP)
			MySocket client(CLIENT, "127.0.0.1", 5000, TCP, 1024);

			// We can't easily connect without a server, but we can test the Disconnect logic
			// assuming the internal state is handled correctly.

			// Act
			client.DisconnectTCP();

			// Assert
			Assert::IsFalse(client.IsConnected());
		} // By Vishwaanth

		TEST_METHOD(Test_Disconnect_NotConnected) // By Vishwaanth
		{
			// Arrange
			MySocket client(CLIENT, "127.0.0.1", 5001, TCP, 1024);

			// Act - Call disconnect on a socket that was never connected
			client.DisconnectTCP();

			// Assert
			Assert::IsFalse(client.IsConnected());
		} // By Vishwaanth

		TEST_METHOD(Test_Disconnect_UDP_Block) 
		{
			// Arrange - Create a UDP socket
			MySocket udpSocket(CLIENT, "127.0.0.1", 5002, UDP, 1024);

			// Act - Attempt to call DisconnectTCP on a UDP socket
			// This should be ignored by the logic we fixed
			udpSocket.DisconnectTCP();

			// Assert
			Assert::IsFalse(udpSocket.IsConnected());
		}  // By Vishwaanth

		TEST_METHOD(Test_Disconnect_ClosesSocket)
		{
			// Arrange
			MySocket client(CLIENT, "127.0.0.1", 5003, TCP, 1024);

			// Act
			client.DisconnectTCP();

			// Assert - Since we can't check private members directly, 
			// we verify that the public state reflects disconnection.
			Assert::IsFalse(client.IsConnected());
		} // By Vishwaanth

		TEST_METHOD(Test_Disconnect_FlagReset)
		{
			// Arrange
			MySocket client(CLIENT, "127.0.0.1", 5004, TCP, 1024);

			// Act
			client.DisconnectTCP();

			// Assert
			Assert::IsFalse(client.IsConnected());
		} // By Vishwaanth

		TEST_METHOD(Test_Disconnect_MultipleCalls)
		{
			// Arrange
			MySocket client(CLIENT, "127.0.0.1", 5005, TCP, 1024);

			// Act
			client.DisconnectTCP();
			client.DisconnectTCP();
			client.DisconnectTCP();
      // Assert - Multiple calls should be safe and not crash
			Assert::IsFalse(client.IsConnected());
		} // By Vishwaanth

		TEST_METHOD(Test_Disconnect_After_Send)
		{
			// Arrange
			MySocket client(CLIENT, "127.0.0.1", 5006, TCP, 1024);

			// Act
			// Even if SendData isn't fully implemented or fails because not connected,
			// Disconnect should still be able to clean up/reset state.
			client.SendData("test", 4);
			client.DisconnectTCP();

			// Assert
			Assert::IsFalse(client.IsConnected());
		} // By Vishwaanth

		TEST_METHOD(Test_ConnectUDP_Server_CreatesAndBindsSocket)
		{
			// Arrange: Create a UDP Server on port 8081
			MySocket sock(SERVER, "127.0.0.1", 8081, UDP, 1024);

			// Act
			sock.ConnectUDP();

			// Assert State
			Assert::IsTrue(sock.IsConnectionSocketValid(), L"ConnectionSocket should be valid after ConnectUDP for a server.");

			// Assert Values
			Assert::AreEqual(8081, sock.GetPort(), L"Port should match the initialized value.");
			Assert::AreEqual(std::string("127.0.0.1"), sock.GetIPAddr(), L"IP Address should match the initialized value.");
			Assert::AreEqual((int)SERVER, (int)sock.GetType(), L"SocketType should be SERVER.");
		}

		TEST_METHOD(Test_ConnectUDP_Client_CreatesSocket)
		{
			// Arrange: Create a UDP Client on port 8082
			MySocket sock(CLIENT, "127.0.0.1", 8082, UDP, 1024);

			// Act
			sock.ConnectUDP();

			// Assert State
			Assert::IsTrue(sock.IsConnectionSocketValid(), L"ConnectionSocket should be valid after ConnectUDP for a client.");

			// Assert Values
			Assert::AreEqual(8082, sock.GetPort(), L"Port should match the initialized value.");
			Assert::AreEqual(std::string("127.0.0.1"), sock.GetIPAddr(), L"IP Address should match the initialized value.");
			Assert::AreEqual((int)CLIENT, (int)sock.GetType(), L"SocketType should be CLIENT.");
		}

		TEST_METHOD(Test_ConnectUDP_OnTCP_FailsSafely)
		{
			// Arrange: Create a TCP Server (ConnectUDP should not work here)
			MySocket sock(SERVER, "127.0.0.1", 8083, TCP, 1024);

			// Act
			sock.ConnectUDP();

			// Assert State
			Assert::IsFalse(sock.IsConnectionSocketValid(), L"ConnectionSocket should remain invalid if ConnectUDP is called on a TCP socket.");

			// Assert Values remain untouched
			Assert::AreEqual(8083, sock.GetPort());
			Assert::AreEqual(std::string("127.0.0.1"), sock.GetIPAddr());
			Assert::AreEqual((int)SERVER, (int)sock.GetType());
		} // By Anh Dung Phan

		TEST_METHOD(Test_DisconnectUDP_ClosesSocket)
		{
			// Arrange: Create and connect a UDP socket
			MySocket sock(CLIENT, "127.0.0.1", 8084, UDP, 1024);
			sock.ConnectUDP();

			// Verify it's open first
			Assert::IsTrue(sock.IsConnectionSocketValid(), L"Setup failed: Socket should be open before disconnecting.");

			// Act
			sock.DisconnectUDP();

			// Assert State
			Assert::IsFalse(sock.IsConnectionSocketValid(), L"ConnectionSocket should be invalid after DisconnectUDP is called.");

			// Assert Values remain intact after disconnection
			Assert::AreEqual(8084, sock.GetPort());
			Assert::AreEqual(std::string("127.0.0.1"), sock.GetIPAddr());
			Assert::AreEqual((int)CLIENT, (int)sock.GetType());
		} // By Anh Dung Phan

		TEST_METHOD(Test_DisconnectUDP_OnTCP_FailsSafely)
		{
			// Arrange: Create a TCP socket
			MySocket sock(CLIENT, "127.0.0.1", 8085, TCP, 1024);

			// Act
			sock.DisconnectUDP();

			// Assert State
			Assert::IsFalse(sock.IsConnectionSocketValid());

			// Assert Values
			Assert::AreEqual(8085, sock.GetPort());
			Assert::AreEqual(std::string("127.0.0.1"), sock.GetIPAddr());
			Assert::AreEqual((int)CLIENT, (int)sock.GetType());
		} // By Anh Dung Phan

		TEST_METHOD(Test_GetIPAddr)
		{
			MySocket sock(CLIENT, "127.0.0.1", 8080, TCP, 1024);
			Assert::AreEqual(std::string("127.0.0.1"), sock.GetIPAddr());
		}

		TEST_METHOD(Test_SetIPAddr)
		{
			MySocket sock(CLIENT, "127.0.0.1", 8080, TCP, 1024);
			sock.SetIPAddr("192.168.1.10");
			Assert::AreEqual(std::string("192.168.1.10"), sock.GetIPAddr());
		}

		TEST_METHOD(Test_SetPort)
		{
			MySocket sock(CLIENT, "127.0.0.1", 8080, TCP, 1024);
			sock.SetPort(9090);
			Assert::AreEqual(9090, sock.GetPort());
		}

		TEST_METHOD(Test_GetData_NullBuffer)
		{
			MySocket sock(CLIENT, "127.0.0.1", 8080, TCP, 1024);
			int result = sock.GetData(nullptr);
			Assert::AreEqual(-1, result);
		}

		TEST_METHOD(Test_GetData_Empty)
		{
			// This test assumes no connection has been established yet,
			// so recv/recvfrom should fail or return <= 0.
			MySocket sock(CLIENT, "127.0.0.1", 8080, TCP, 1024);

			char buffer[1024] = { 0 };
			int result = sock.GetData(buffer);

			Assert::IsTrue(result <= 0);
		}

		TEST_METHOD(Test_GetData_TCP)
		{
			// Arrange
			MySocket server(SERVER, "127.0.0.1", 5050, TCP, 1024);
			MySocket client(CLIENT, "127.0.0.1", 5050, TCP, 1024);

			const char* msg = "Hello TCP";
			char recvBuffer[1024] = { 0 };

			// Act
			std::thread t([&]() { server.ConnectTCP(); });
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			client.ConnectTCP();
			t.join();

			client.SendData(msg, (int)strlen(msg) + 1);
			int bytes = server.GetData(recvBuffer);

			// Assert
			Assert::IsTrue(bytes > 0);
			Assert::AreEqual(std::string(msg), std::string(recvBuffer));

			client.DisconnectTCP();
			server.DisconnectTCP();
		}

		TEST_METHOD(Test_GetData_UDP)
		{
			// Arrange
			MySocket server(SERVER, "127.0.0.1", 6060, UDP, 1024);
			MySocket client(CLIENT, "127.0.0.1", 6060, UDP, 1024);

			const char* msg = "Hello UDP";
			char recvBuffer[1024] = { 0 };

			// Act
			server.ConnectUDP();
			client.ConnectUDP();

			client.SendData(msg, (int)strlen(msg) + 1);
			int bytes = server.GetData(recvBuffer);

			// Assert
			Assert::IsTrue(bytes > 0);
			Assert::AreEqual(std::string(msg), std::string(recvBuffer));

			// Cleanup
			server.DisconnectUDP();
			client.DisconnectUDP();
		}

		// Test_SendData_TCP
		TEST_METHOD(Test_SendData_TCP)
		{
			EnsureWSA();
			int testPort = 9017;
			std::string testIP = "127.0.0.1";

			// 1. Setup the raw listener
			SOCKET listener = CreateTestListener(TCP, testIP, testPort);

			// 2. Setup the MySocket Client and connect
			MySocket client(CLIENT, testIP, testPort, TCP, 1024);
			client.ConnectTCP();

			// 3. Accept the connection on our raw listener
			SOCKET acceptedSocket = accept(listener, nullptr, nullptr);
			Assert::AreNotEqual((int)INVALID_SOCKET, (int)acceptedSocket, L"Listener failed to accept TCP connection.");

			// Apply timeout to the accepted socket as well
			DWORD timeout = 1000;
			setsockopt(acceptedSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

			// 4. Act: Send data using MySocket
			char sendBuf[] = "TCP Payload Data";
			client.SendData(sendBuf, sizeof(sendBuf));

			// 5. Assert: Receive data on the raw socket and verify
			char recvBuf[1024] = { 0 };
			int bytesReceived = recv(acceptedSocket, recvBuf, sizeof(recvBuf), 0);

			Assert::IsTrue(bytesReceived > 0, L"Failed to receive TCP data.");
			Assert::AreEqual(std::string(sendBuf), std::string(recvBuf), L"Received TCP data does not match sent data.");

			// Cleanup
			closesocket(acceptedSocket);
			closesocket(listener);
			client.DisconnectTCP();
		} // By Anh Dung Phan

		TEST_METHOD(Test_TCP_Client_Connect)
		{
			MySocket server(SERVER, "127.0.0.1", 6000, TCP, 1024);
			MySocket client(CLIENT, "127.0.0.1", 6000, TCP, 1024);

			std::thread t([&]() { server.ConnectTCP(); });
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			client.ConnectTCP();
			t.join();

			Assert::IsTrue(client.IsConnected());
			Assert::IsTrue(server.IsConnected());
		}

		TEST_METHOD(Test_TCP_Server_Connect)
		{
			MySocket server(SERVER, "127.0.0.1", 6001, TCP, 1024);
			MySocket client(CLIENT, "127.0.0.1", 6001, TCP, 1024);

			std::thread t([&]() { server.ConnectTCP(); });
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			client.ConnectTCP();
			t.join();

			Assert::IsTrue(server.IsConnected());
		}

		TEST_METHOD(Test_TCP_Connect_Twice)
		{
			MySocket client(CLIENT, "127.0.0.1", 6002, TCP, 1024);

			client.ConnectTCP();
			bool first = client.IsConnected();

			client.ConnectTCP();
			bool second = client.IsConnected();

			Assert::AreEqual(first, second);
		}

		TEST_METHOD(Test_TCP_Connect_InvalidIP)
		{
			MySocket client(CLIENT, "256.256.256.256", 6003, TCP, 1024);

			client.ConnectTCP();

			Assert::IsFalse(client.IsConnected());
		}

		TEST_METHOD(Test_TCP_Connect_UDP_Block)
		{
			MySocket udpClient(CLIENT, "127.0.0.1", 6004, UDP, 1024);

			udpClient.ConnectTCP();

			Assert::IsFalse(udpClient.IsConnected());
		}

		TEST_METHOD(Test_TCP_Client_SocketCreation)
		{
			MySocket client(CLIENT, "127.0.0.1", 6005, TCP, 1024);

			client.ConnectTCP();

			Assert::IsTrue(client.GetConnectionSocket() != INVALID_SOCKET ||
				client.IsConnected() == false);
		}

		TEST_METHOD(Test_TCP_Server_Listen)
		{
			MySocket server(SERVER, "127.0.0.1", 6006, TCP, 1024);

			std::thread t([&]() { server.ConnectTCP(); });
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			Assert::IsTrue(server.GetWelcomeSocket() != INVALID_SOCKET);

			t.detach();
		}

		// Test_SendData_UDP
		TEST_METHOD(Test_SendData_UDP)
		{
			EnsureWSA();
			int testPort = 9018;
			std::string testIP = "127.0.0.1";

			// 1. Setup the raw receiver
			SOCKET receiver = CreateTestListener(UDP, testIP, testPort);
			Assert::AreNotEqual((int)INVALID_SOCKET, (int)receiver, L"Testing Server is not established properly!");

			// 2. Setup the MySocket UDP Client
			MySocket client(CLIENT, testIP, testPort, UDP, 1024);
			client.ConnectUDP();

			// 3. Act: Send data using MySocket
			char sendBuf[17] = "UDP Payload Data";
			client.SendData(sendBuf, sizeof(sendBuf));

			// 4. Assert: Receive data on the raw socket and verify
			char recvBuf[1024] = { 0 };
			int bytesReceived = recvfrom(receiver, recvBuf, sizeof(recvBuf), 0, nullptr, nullptr);

			Assert::IsTrue(bytesReceived > 0, L"Failed to receive UDP data.");
			Assert::AreEqual(std::string(sendBuf), std::string(recvBuf), L"Received UDP data does not match sent data.");

			// Cleanup
			closesocket(receiver);
			client.DisconnectUDP();
		} // By Anh Dung Phan

		// Test_SendData_Empty
		TEST_METHOD(Test_SendData_Empty)
		{
			EnsureWSA();
			int testPort = 9019;
			std::string testIP = "127.0.0.1";

			// 1. Setup the raw receiver
			SOCKET receiver = CreateTestListener(UDP, testIP, testPort);

			// 2. Setup the MySocket UDP Client
			MySocket client(CLIENT, testIP, testPort, UDP, 1024);
			client.ConnectUDP();

			// 3. Act: Send an empty payload (0 bytes)
			char emptyBuf[] = "";
			client.SendData(emptyBuf, 0);

			// 4. Assert: UDP allows 0-byte datagrams, so we should receive a packet with 0 bytes.
			char recvBuf[1024] = { 0 };
			int bytesReceived = recvfrom(receiver, recvBuf, sizeof(recvBuf), 0, nullptr, nullptr);

			Assert::AreEqual(0, bytesReceived, L"Should have received a 0-byte UDP datagram.");

			// Cleanup
			closesocket(receiver);
			client.DisconnectUDP();
		} // By Anh Dung Phan

		// Test_SendData_Large
		TEST_METHOD(Test_SendData_Large)
		{
			EnsureWSA();
			int testPort = 9020;
			std::string testIP = "127.0.0.1";

			// 1. Setup the raw receiver
			SOCKET receiver = CreateTestListener(UDP, testIP, testPort);

			// 2. Set up a custom large MySocket client
			int largeSize = 2048;
			MySocket client(CLIENT, testIP, testPort, UDP, largeSize);
			client.ConnectUDP();

			// 3. Create a payload larger than the standard DEFAULT_SIZE (1024)
			char* largeSendBuf = new char[largeSize];
			memset(largeSendBuf, 'A', largeSize);
			largeSendBuf[largeSize - 1] = '\0';

			// 4. Act
			client.SendData(largeSendBuf, largeSize);

			// 5. Assert
			char* largeRecvBuf = new char[largeSize];
			memset(largeRecvBuf, 0, largeSize);

			int bytesReceived = recvfrom(receiver, largeRecvBuf, largeSize, 0, nullptr, nullptr);

			Assert::AreEqual(largeSize, bytesReceived, L"Did not receive the full large payload.");
			Assert::AreEqual(std::string(largeSendBuf), std::string(largeRecvBuf), L"Large payloads do not match.");

			// Cleanup
			delete[] largeSendBuf;
			delete[] largeRecvBuf;
			closesocket(receiver);
			client.DisconnectUDP();
		} // By Anh Dung Phan

		// Test_SendData_InvalidSocket
		TEST_METHOD(Test_SendData_InvalidSocket)
		{
			// Note: This test does not require a listener since we expect it to safely fail internally.

			// Arrange
			MySocket sock(CLIENT, "127.0.0.1", 9021, TCP, 1024);
			char data[] = "Orphan Data";

			// Act
			sock.SendData(data, sizeof(data));

			// Assert
			Assert::IsTrue(true, L"SendData should safely ignore the transmission without crashing if bTCPConnect is false.");
		} // Anh Dung Phan
	};
}
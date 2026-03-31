#include "pch.h"
#include "CppUnitTest.h"
#include "../PktDef/Packet.h"
#include "../PktDef/Socket.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

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
	};
}

// test comment from vishwaanth
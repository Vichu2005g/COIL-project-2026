#include "pch.h"
#include "CppUnitTest.h"
#include "../PktDef/Packet.h"

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
}

// test comment from vishwaanth
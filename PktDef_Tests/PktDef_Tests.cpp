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
			pkt.SetBodyData(nullptr, 0);  // <-- FIX

			// Act
			pkt.GenPacket();

			// Assert
			Assert::IsFalse(pkt.GetAck());
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


		TEST_METHOD(Test_Serialize_Deserialize_WithAck)
		{
			// Arrange
			PktDef pkt1;
			pkt1.SetPktCount(12);
			pkt1.SetCmd(RESPONSE);
			pkt1.SetAck(true);
			pkt1.SetBodyData(nullptr, 0);   // <-- FIX

			// Act
			char* raw = pkt1.GenPacket();
			PktDef pkt2(raw);

			// Assert
			Assert::IsTrue(pkt2.GetAck());
			Assert::AreEqual((int)RESPONSE, (int)pkt2.GetCmd());
		}

		TEST_METHOD(Test_AckResponse)
		{
			// Arrange
			PktDef pkt;
			pkt.SetCmd(RESPONSE); // Sets Status = 1
			pkt.SetAck(true);     // Sets Ack = 1

			// Act & Assert
			Assert::IsTrue(pkt.IsAck(), L"Packet should be identified as an ACK.");
			Assert::IsFalse(pkt.IsNack(), L"Packet should not be identified as a NACK.");
		}

		TEST_METHOD(Test_NackResponse)
		{
			// Arrange
			PktDef pkt;
			pkt.SetCmd(RESPONSE); // Sets Status = 1
			pkt.SetAck(false);    // Sets Ack = 0

			// Act & Assert
			Assert::IsFalse(pkt.IsAck(), L"Packet should not be identified as an ACK.");
			Assert::IsTrue(pkt.IsNack(), L"Packet should be identified as a NACK.");
		}

		TEST_METHOD(Test_TelemetryResponse_ValidData)
		{
			// Arrange
			PktDef pkt;
			pkt.SetCmd(RESPONSE);
			pkt.SetAck(true);     // Sets Ack = 1

			// Create dummy telemetry data to act as our payload
			TelemetryBody expectedData;
			// Zero out first to avoid uninitialized padding bytes causing issues
			memset(&expectedData, 0, sizeof(TelemetryBody));
			expectedData.LastPktCounter = 42;
			expectedData.CurrentGrade = 95;
			expectedData.HitCount = 3;
			expectedData.Heading = 180;
			expectedData.LastCmd = DRIVE;
			expectedData.LastCmdValue = FORWARD;
			expectedData.LastCmdPower = 75;

			// Cast the struct to char* to simulate raw byte payload coming over a network
			pkt.SetBodyData(reinterpret_cast<char*>(&expectedData), sizeof(TelemetryBody));

			// Act
			TelemetryBody result = pkt.GetTelemetryData();

			// Assert
			Assert::IsTrue(pkt.IsTelemetry(), L"Packet must be identified as telemetry.");

			// Compare the parsed struct properties with our arranged data
			Assert::AreEqual((int)expectedData.LastPktCounter, (int)result.LastPktCounter);
			Assert::AreEqual((int)expectedData.CurrentGrade, (int)result.CurrentGrade);
			Assert::AreEqual((int)expectedData.HitCount, (int)result.HitCount);
			Assert::AreEqual((int)expectedData.Heading, (int)result.Heading);
			Assert::AreEqual((int)expectedData.LastCmd, (int)result.LastCmd);
			Assert::AreEqual((int)expectedData.LastCmdValue, (int)result.LastCmdValue);
			Assert::AreEqual((int)expectedData.LastCmdPower, (int)result.LastCmdPower);
		}

		TEST_METHOD(Test_TelemetryResponse_InvalidSize)
		{
			// Arrange
			PktDef pkt;
			pkt.SetCmd(RESPONSE);

			// Set body data that is smaller than sizeof(TelemetryBody)
			char smallPayload[] = { 1, 2, 3 };
			pkt.SetBodyData(smallPayload, 3);

			// Act
			TelemetryBody result = pkt.GetTelemetryData();

			// Assert
			// Because the body is too small, GetTelemetryData should return an empty/zeroed struct
			Assert::AreEqual(0, (int)result.LastPktCounter);
			Assert::AreEqual(0, (int)result.CurrentGrade);
			Assert::AreEqual(0, (int)result.Heading);
		}

		TEST_METHOD(Test_Destructor_Cleanup)
		{
			// Arrange & Act
			{
				PktDef pkt;

				pkt.SetCmd(DRIVE);

				char data[] = { FORWARD, 5, 80 };
				pkt.SetBodyData(data, 3);

				pkt.GenPacket();

			} // Destructor called here automatically

			// Assert
			Assert::IsTrue(true); // If no crash, destructor worked
		}


	};
}

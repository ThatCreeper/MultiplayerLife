#pragma once

void netRecievePackets();

struct TCPHeader {
	short sourcePort;
	short destinationPort;
	int sequenceNumber;
	int acknowledgementNumber;
	short lengthFlags;
	short winSize;
	short checkSum;
	short urgentPointer;
};
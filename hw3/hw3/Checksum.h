#pragma once
#define CRC_SIZE 256
class Checksum {
public:
	Checksum();
	DWORD CRC32(unsigned char* buf, size_t len);
	DWORD crc_table[CRC_SIZE];
};
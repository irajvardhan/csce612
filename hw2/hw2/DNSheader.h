#pragma once
#include "pch.h"

#pragma pack(push,1) // sets padding to 1 byte
class FixedDNSheader {
public:
	USHORT TXID;
	USHORT flags;
	USHORT n_questions;
	USHORT n_answers;
	USHORT n_authority;
	USHORT n_additional;
	
};

class QueryHeader {
public:
	USHORT q_type;
	USHORT q_class;

};
#pragma pack(pop) // restores to old packing
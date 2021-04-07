/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
#include "pch.h"

SharedParameters::SharedParameters() {
	base = 0;// todo check what value this should be
	MBacked = 0.0;
	nextSeq = 1;
	T = 0; // no packet timedout initially
	F = 0; // no fast retransmissions initially
	windowSize = 1;
	speed = 0.0;
	RTT = 0.0;
	devRTT = 0.0;
}
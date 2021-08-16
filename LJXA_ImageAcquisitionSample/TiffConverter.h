//Copyright (c) 2020 KEYENCE CORPORATION. All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <fstream>
using namespace std;

class TiffConverter {
public:
	static long Save(string savePath, unsigned short *heighImage, int lines, int width);
private:
	static void WriteTiffHeader(ostream *stream, unsigned int lines, unsigned int width);
	static void WriteTiffTag(ostream *stream, unsigned short kind, unsigned short dataType, unsigned int dataSize, unsigned int data);
};
#pragma once

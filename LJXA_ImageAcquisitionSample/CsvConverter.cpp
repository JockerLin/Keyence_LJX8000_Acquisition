//Copyright (c) 2020 KEYENCE CORPORATION. All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <fstream>
#include "CsvConverter.h"
using namespace std;

static int COLLECT_VALUE = 32768;
static double INVALID_VALUE = -999.9999;

long CsvConverter::Save(string savePath, unsigned short *image, int lines, int width, float z_pitch_um) {

	// Save the profile
	ofstream stream(savePath);
	if (!stream) return -1;
	unsigned short *ptr = (unsigned short*)&image[0];
	char buffer[20];

	for (int i = 0; i < lines * width; i++) {
		if (i != 0 && i % width == 0) stream << std::endl;
		double value = *ptr == 0 ? INVALID_VALUE : (*ptr - COLLECT_VALUE) * z_pitch_um / 1000;
		int length = sprintf_s(buffer, "%-.4f", value);
		stream.write((char*)buffer, length * sizeof(char));
		if (i < lines * width) stream.write(",", sizeof(char));
		ptr++;
	}

	stream.close();
	return 0;
}
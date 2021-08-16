//Copyright (c) 2020 KEYENCE CORPORATION. All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string>
using namespace std;

class CsvConverter {
public:
	static long Save(string savePath, unsigned short *heighImage, int lines, int width, float z_pitch_um);
};

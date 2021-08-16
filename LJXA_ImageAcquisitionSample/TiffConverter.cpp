//Copyright (c) 2020 KEYENCE CORPORATION. All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <fstream>
#include "TiffConverter.h"
using namespace std;

long TiffConverter::Save(string savePath, unsigned short *image, int lines, int width) {

	// Save the profile
	ofstream stream(savePath, ios::binary);
	if (!stream) return -1;

	char *ptr = (char*)&image[0];

	WriteTiffHeader(&stream, (unsigned int)lines, (unsigned int)width);
	stream.write(ptr, lines * width * 2);
	stream.close();
	return 0;
}

void TiffConverter::WriteTiffHeader(ostream *stream, unsigned int lines, unsigned int width) {
	// <header(8)> + <tag count(2)> + <tag(12)>*11 + <next IFD(4)> + <resolution unit(8)>*2
	const unsigned int stripOffset = 162;

	// Header (little endian)
	char header[8] = { 0x49, 0x49, 0x2A, 0x00, 0x08, 0x00, 0x00, 0x00 };
	stream->write(header, 8);

	// Tag count
	char tagCount[2] = { 0x0B, 0x00 };
	stream->write(tagCount, 2);

	// Image Width
	WriteTiffTag(stream, 0x0100, 3, 1, width);

	// Image Length
	WriteTiffTag(stream, 0x0101, 3, 1, lines);

	// Bits per sample
	WriteTiffTag(stream, 0x0102, 3, 1, 16);

	// Compression (no compression)
	WriteTiffTag(stream, 0x0103, 3, 1, 1);

	// Photometric interpretation (white mode & monochrome)
	WriteTiffTag(stream, 0x0106, 3, 1, 1);

	// Strip offsets
	WriteTiffTag(stream, 0x0111, 3, 1, stripOffset);

	// Rows per strip
	WriteTiffTag(stream, 0x0116, 3, 1, lines);

	// strip byte counts
	WriteTiffTag(stream, 0x0117, 4, 1, width * lines * (unsigned int)2);

	// X resolusion address
	WriteTiffTag(stream, 0x011A, 5, 1, stripOffset - 16);

	// Y resolusion address
	WriteTiffTag(stream, 0x011B, 5, 1, stripOffset - 8);

	// Resolusion unit (inch)
	WriteTiffTag(stream, 0x0128, 3, 1, 2);

	// Next IFD
	char nextIfd[8] = { 0x00,0x00,0x00, 0x00 };
	stream->write(nextIfd, 4);

	// X resolusion and Y resolusion
	char res1[8] = { 0x60,0x00,0x00, 0x00 };
	char res2[8] = { 0x01,0x00,0x00, 0x00 };
	stream->write(res1, 4);
	stream->write(res2, 4);
	stream->write(res1, 4);
	stream->write(res2, 4);
}

void TiffConverter::WriteTiffTag(ostream *stream, unsigned short kind, unsigned short dataType, unsigned int dataSize, unsigned int data) {
	char list[12] = { 0 };
	char* p = &list[0];
	memcpy(p, (unsigned short*)&kind, sizeof(unsigned short));
	memcpy(p + 2, (unsigned short*)&dataType, sizeof(unsigned short));
	memcpy(p + 4, (unsigned int*)&dataSize, sizeof(unsigned int));
	memcpy(p + 8, (unsigned int*)&data, sizeof(unsigned int));
	stream->write(list, 12);
}
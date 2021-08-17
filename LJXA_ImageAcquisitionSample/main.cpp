//Copyright (c) 2020 KEYENCE CORPORATION. All rights reserved.

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <winsock2.h>
#include <conio.h>

#include "LJX8_IF.h"
#include "LJX8_ErrorCode.h"
#include "LJXA_ACQ.h"

#include "CsvConverter.h"
#include "TiffConverter.h"

#pragma comment(lib, "winmm.lib")

using namespace std;

void WaitExit()
{
	printf("\nPress any key to exit the program...\n");
	_getch();
}

int main() {
	//=====================================================================
	// Acquire LJXA image
	//=====================================================================
	// Basically you can acquire LJ-XA image in just 3 steps.
	//
	//  Step1. Open device
	//  Step2. Acquire
	//  Step3. Close device

	unsigned short *heightImage;		// Height image
	unsigned short *luminanceImage;		// Luminance image

	//-----------------------------------------------------------------
	// CHANGE THIS BLOCK TO MATCH YOUR SENSOR SETTINGS (FROM HERE)
	//-----------------------------------------------------------------
	int deviceId = 0;					// 设备数 Set "0" if you use only 1 head.
	int xImageSize = 3200;				// 图像宽 Number of X points.
	int yImageSize = 1000;				// 图像高 Number of Y lines.
	float y_pitch_um = 20.0;			// Y数据间距 Data pitch of Y data. (e.g. your encoder setting)
	int	timeout_ms = 5000;				// 采集timeout设定 Timeout value for the acquiring image (in milisecond).
	int use_external_batchStart = 0;	// Set "1" if you controll the batch start timing externally. (e.g. terminal input)
	string outputFilePath1 = "sample_height.csv";
	string outputFilePath2 = "sample_height.tif";
	string outputFilePath3 = "sample_luminance.tif";

	LJX8IF_ETHERNET_CONFIG EthernetConfig =
	{
		{ 192, 168, 0, 1 },				// IP address
		24691							// Port number
	};

	int HighSpeedPortNo = 24692;		// Port number for high-speed communication
	//-----------------------------------------------------------------
	// CHANGE THIS BLOCK TO MATCH YOUR SENSOR SETTINGS (TO HERE)
	//-----------------------------------------------------------------

	// 申请内存 Allocate user memory
	heightImage = (unsigned short*)malloc(sizeof(unsigned short) * xImageSize * yImageSize);
	luminanceImage = (unsigned short*)malloc(sizeof(unsigned short) * xImageSize * yImageSize);
	if (heightImage == NULL || luminanceImage == NULL) {
		printf("Failed to allocate memory.\n");

		WaitExit();
		return (1);
	}

	// 采集参数设定 Prepare setting parameter
	LJXA_ACQ_SETPARAM setParam;
	{
		setParam.y_linenum = yImageSize;
		setParam.y_pitch_um = y_pitch_um;
		setParam.timeout_ms = timeout_ms;
		setParam.use_external_batchStart = use_external_batchStart;
	}

	// Variable to store information of the acquired image
	// 用于存储获取的图像信息的变量
	LJXA_ACQ_GETPARAM getParam;

	//------------------------------------------------------------
	// Step1. Open device
	// 打开设备
	//------------------------------------------------------------
	int errCode = LJXA_ACQ_OpenDevice(deviceId, &EthernetConfig, HighSpeedPortNo);

	if (errCode != LJX8IF_RC_OK) {
		printf("Failed to open device.\n");
		//Free user memory
		free(heightImage);
		free(luminanceImage);

		WaitExit();
		return (1);
	}

	//------------------------------------------------------------
	// Step2. Acquire image
	// 采集图像
	//------------------------------------------------------------
	// There are two methods you can use.
	//
	// (1) Synchronous method 同步方法
	//	"Acquire" function does not return unless the acquisition is completed or a timeout occurs. 
	//	This is an easy method because you only call one function.
	//  But it blocks execution of other code during acquisition.
	//
	// (2) Asynchronous methods 异步方法
	// "Start" the acquisition first, and "Acquire" later.
	// "Acquire" function returns even if the acquisition is not completed.
	// It doesn't block other code.


#if 1	// Synchronous acquisition 同步采集方法
	errCode = LJXA_ACQ_Acquire(deviceId, heightImage, luminanceImage, &setParam, &getParam);

	if (errCode != LJX8IF_RC_OK) {
		printf("Failed to acquire image.\n");
		//Free user memory
		free(heightImage);
		free(luminanceImage);

		WaitExit();
		return (1);
	}
#else	// Asynchronous acquisition 异步采集方法
	// Start asynchronous acquire
	errCode = LJXA_ACQ_StartAsync(deviceId, &setParam);
	if (errCode != LJX8IF_RC_OK) {
		printf("Failed to acquire image.\n");
		//Free user memory
		free(heightImage);
		free(luminanceImage);

		WaitExit();
		return (1);
	}

	// Acquire. Polling to confirm complete.
	// Or wait until a timeout occurs.
	printf(" [acquring image...(waiting in usercode)]\n");
	DWORD start = timeGetTime();
	while (true)
	{
		DWORD ts = timeGetTime() - start;
		if (timeout_ms < ts) {
			break;
		}
		errCode = LJXA_ACQ_AcquireAsync(deviceId, heightImage, luminanceImage, &setParam, &getParam);
		if (errCode == LJX8IF_RC_OK)break;
	}

	if (errCode != LJX8IF_RC_OK) {
		printf("Failed to acquire image (timeout).\n");
		//Free user memory
		free(heightImage);
		free(luminanceImage);

		WaitExit();
		return (1);
	}

#endif

	//------------------------------------------------------------
	// Step3. Close device
	//------------------------------------------------------------
	LJXA_ACQ_CloseDevice(deviceId);

	// Information of the acquired image
	printf("----------------------------------------\n");
	printf(" Luminance output      : %d\n", getParam.luminance_enabled);
	printf(" Number of X points    : %d\n", getParam.x_pointnum);
	printf(" Number of Y lines     : %d\n", getParam.y_linenum_acquired);
	printf(" X pitch in micrometer : %-.1f\n", getParam.x_pitch_um);
	printf(" Y pitch in micrometer : %-.1f\n", getParam.y_pitch_um);
	printf(" Z pitch in micrometer : %-.1f\n", getParam.z_pitch_um);
	printf("----------------------------------------\n");

	// Output csv and tiff
	// 保存数据
	try
	{
		long rc = CsvConverter::Save(outputFilePath1, heightImage, yImageSize, xImageSize, getParam.z_pitch_um);

		if (rc == 0)
		{
			rc = TiffConverter::Save(outputFilePath2, heightImage, yImageSize, xImageSize);
		}

		if (rc == 0 && getParam.luminance_enabled) {
			rc = TiffConverter::Save(outputFilePath3, luminanceImage, yImageSize, xImageSize);
		}

		printf(rc == 0 ? "File saved successfully.\n" : "Failed to save file.\n");
	}
	catch (exception e)
	{
		printf("Failed to save file.\n");
	}

	//Free user memory
	free(heightImage);
	free(luminanceImage);

	WaitExit();
	return (0);
}

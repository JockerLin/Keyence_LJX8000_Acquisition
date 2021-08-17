//Copyright (c) 2020 KEYENCE CORPORATION. All rights reserved.

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <stdlib.h>
#include <mutex>

#include "LJX8_IF.h"
#include "LJX8_ErrorCode.h"

#include "LJXA_ACQ.h"

#pragma comment(lib, "winmm.lib")

using namespace std;

// Static variable
static LJX8IF_ETHERNET_CONFIG _ethernetConfig[MAX_LJXA_DEVICENUM];
static int _highSpeedPortNo[MAX_LJXA_DEVICENUM];
static int _imageAvailable[MAX_LJXA_DEVICENUM];
static int _lastImageSizeHeight[MAX_LJXA_DEVICENUM];
static LJXA_ACQ_GETPARAM _getParam[MAX_LJXA_DEVICENUM];
static unsigned short *_heightBuf[MAX_LJXA_DEVICENUM];
static unsigned short *_luminanceBuf[MAX_LJXA_DEVICENUM];

// Function prototype
void myCallbackFunc(LJX8IF_PROFILE_HEADER* pProfileHeaderArray, WORD* pHeightProfileArray, WORD* pLuminanceProfileArray, DWORD dwLuminanceEnable, DWORD dwProfileDataCount, DWORD dwCount, DWORD dwNotify, DWORD dwUser);

extern "C"
{
	LJXA_ACQ_API int LJXA_ACQ_OpenDevice(int lDeviceId, LJX8IF_ETHERNET_CONFIG *EthernetConfig, int HighSpeedPortNo) {
		int errCode = LJX8IF_EthernetOpen(lDeviceId, EthernetConfig);

		_ethernetConfig[lDeviceId] = *EthernetConfig;
		_highSpeedPortNo[lDeviceId] = HighSpeedPortNo;
		printf("[@(LJXA_ACQ_OpenDevice) Open device](0x%x)\n", errCode);

		return errCode;
	}

	LJXA_ACQ_API void LJXA_ACQ_CloseDevice(int lDeviceId) {
		LJX8IF_FinalizeHighSpeedDataCommunication(lDeviceId);
		LJX8IF_CommunicationClose(lDeviceId);
		printf("[@(LJXA_ACQ_CloseDevice) Close device]\n");
	}

	/**
	同步采集方法
	@param	lDeviceId		设备ID
	@param	heightImage		高度图指针
	@param	luminanceImage	亮度图指针
	@param	setParam		设置采集参数
	@param	getParam		采集完参数
	@return	Return code
	*/
	LJXA_ACQ_API int LJXA_ACQ_Acquire(int lDeviceId, unsigned short *heightImage, unsigned short *luminanceImage, LJXA_ACQ_SETPARAM *setParam, LJXA_ACQ_GETPARAM *getParam) {
		int errCode;

		int yDataNum = setParam->y_linenum;
		int timeout_ms = setParam->timeout_ms;
		int use_external_batchStart = setParam->use_external_batchStart;
		unsigned short zUnit = 0;

		//Allocate memory
		_heightBuf[lDeviceId] = (unsigned short*)malloc(yDataNum * MAX_LJXA_XDATANUM * 2);
		if (_heightBuf[lDeviceId] == NULL) {
			return LJX8IF_RC_ERR_NOMEMORY;
		}

		_luminanceBuf[lDeviceId] = (unsigned short*)malloc(yDataNum * MAX_LJXA_XDATANUM * 2);
		if (_luminanceBuf[lDeviceId] == NULL) {
			return LJX8IF_RC_ERR_NOMEMORY;
		}

		// Initialize
		// 高速通讯预处理 注册采集回调=>myCallbackFunc
		// yDataNum 调用回调的次数？
		errCode = LJX8IF_InitializeHighSpeedDataCommunicationSimpleArray(lDeviceId, &_ethernetConfig[lDeviceId], _highSpeedPortNo[lDeviceId], &myCallbackFunc, yDataNum, lDeviceId);
		printf("[@(LJXA_ACQ_Acquire) Initialize HighSpeed](0x%x)\n", errCode);

		// PreStart
		LJX8IF_HIGH_SPEED_PRE_START_REQ startReq;
		startReq.bySendPosition = 2;
		LJX8IF_PROFILE_INFO profileInfo;

		// 准备高速通讯
		errCode = LJX8IF_PreStartHighSpeedDataCommunication(lDeviceId, &startReq, &profileInfo);
		printf("[@(LJXA_ACQ_Acquire) PreStart](0x%x)\n", errCode);

		// todo 获取simpleArray转换因子 ？？
		errCode = LJX8IF_GetZUnitSimpleArray(lDeviceId, &zUnit);
		if (errCode != 0 || zUnit == 0)
		{
			printf("Failed to acquire zunit.\n");

			//Free memory
			if (_heightBuf[lDeviceId] != NULL) {
				free(_heightBuf[lDeviceId]);
			}

			if (_luminanceBuf[lDeviceId] != NULL) {
				free(_luminanceBuf[lDeviceId]);
			}
			return errCode;
		}

		//Start HighSpeed
		_imageAvailable[lDeviceId] = 0;
		_lastImageSizeHeight[lDeviceId] = 0;

		errCode = LJX8IF_StartHighSpeedDataCommunication(lDeviceId);
		printf("[@(LJXA_ACQ_Acquire) Start HighSpeed](0x%x)\n", errCode);

		//StartMeasure(Batch Start)
		if (use_external_batchStart > 0) {
		}
		else {
			// 开始批处理
			errCode = LJX8IF_StartMeasure(lDeviceId);
			printf("[@(LJXA_ACQ_Acquire) Measure Start(Batch Start)](0x%x)\n", errCode);
		}

		// Acquire. Polling to confirm complete.
		// Or wait until a timeout occurs.
		// 后台采集，采集完进回调函数改标志位
		printf(" [@(LJXA_ACQ_Acquire) acquring image...]\n");
		DWORD start = timeGetTime();
		while (true)
		{
			DWORD ts = timeGetTime() - start;
			if ((DWORD)timeout_ms < ts) {
				break;
			}
			if (_imageAvailable[lDeviceId]) break;
		}

		// timeout 采集超时
		if (_imageAvailable[lDeviceId] != 1) {
			printf(" [@(LJXA_ACQ_Acquire) timeout]\n");

			//Stop HighSpeed
			errCode = LJX8IF_StopHighSpeedDataCommunication(lDeviceId);
			printf("[@(LJXA_ACQ_Acquire) Stop HighSpeed](0x%x)\n", errCode);

			//Free memory
			if (_heightBuf[lDeviceId] != NULL) {
				free(_heightBuf[lDeviceId]);
			}

			if (_luminanceBuf[lDeviceId] != NULL) {
				free(_luminanceBuf[lDeviceId]);
			}
			return LJX8IF_RC_ERR_TIMEOUT;
		}
		printf(" [@(LJXA_ACQ_Acquire) done]\n");

		// 采集完毕，停止高速通讯 Stop HighSpeed
		errCode = LJX8IF_StopHighSpeedDataCommunication(lDeviceId);
		printf("[@(LJXA_ACQ_Acquire) Stop HighSpeed](0x%x)\n", errCode);

		//---------------------------------------------------------------------
		//  Organaize parameters related to acquired image 
		//---------------------------------------------------------------------

		_getParam[lDeviceId].luminance_enabled = profileInfo.byLuminanceOutput;
		_getParam[lDeviceId].x_pointnum = profileInfo.wProfileDataCount;
		_getParam[lDeviceId].y_linenum_acquired = _lastImageSizeHeight[lDeviceId];
		// todo 两个数据为什么/100
		_getParam[lDeviceId].x_pitch_um = profileInfo.lXPitch / 100.0f;
		_getParam[lDeviceId].y_pitch_um = setParam->y_pitch_um;
		_getParam[lDeviceId].z_pitch_um = zUnit / 100.0f;

		*getParam = _getParam[lDeviceId];

		//---------------------------------------------------------------------
		//  Copy internal buffer to user buffer
		// 拷贝内部数据
		//---------------------------------------------------------------------
		int xDataNum = _getParam[lDeviceId].x_pointnum;

		unsigned short *dwHeightBuf = (unsigned short*)&_heightBuf[lDeviceId][0];
		memcpy(heightImage, dwHeightBuf, xDataNum * yDataNum * 2);

		if (_getParam[lDeviceId].luminance_enabled > 0) {
			unsigned short *dwLuminanceBuf = (unsigned short*)&_luminanceBuf[lDeviceId][0];
			memcpy(luminanceImage, dwLuminanceBuf, xDataNum * yDataNum * 2);
		}

		//Free memory
		if (_heightBuf[lDeviceId] != NULL) {
			free(_heightBuf[lDeviceId]);
		}

		if (_luminanceBuf[lDeviceId] != NULL) {
			free(_luminanceBuf[lDeviceId]);
		}

		return LJX8IF_RC_OK;
	}

	/**
	异步采集开始
	@param	lDeviceId		设备ID
	@param	setParam		设置采集参数
	@return	Return code
	*/
	LJXA_ACQ_API int LJXA_ACQ_StartAsync(int lDeviceId, LJXA_ACQ_SETPARAM *setParam) {
		int errCode;
		unsigned short zUnit = 0;

		int yDataNum = setParam->y_linenum;
		int use_external_batchStart = setParam->use_external_batchStart;

		//Allocate memory
		if (_heightBuf[lDeviceId] != NULL) {
			free(_heightBuf[lDeviceId]);
		}

		if (_luminanceBuf[lDeviceId] != NULL) {
			free(_luminanceBuf[lDeviceId]);
		}

		_heightBuf[lDeviceId] = (unsigned short*)malloc(yDataNum * MAX_LJXA_XDATANUM * 2);
		_luminanceBuf[lDeviceId] = (unsigned short*)malloc(yDataNum * MAX_LJXA_XDATANUM * 2);

		if (_heightBuf[lDeviceId] == NULL) {
			return LJX8IF_RC_ERR_NOMEMORY;
		}

		if (_luminanceBuf[lDeviceId] == NULL) {
			return LJX8IF_RC_ERR_NOMEMORY;
		}

		//Initialize
		errCode = LJX8IF_InitializeHighSpeedDataCommunicationSimpleArray(lDeviceId, &_ethernetConfig[lDeviceId], _highSpeedPortNo[lDeviceId], &myCallbackFunc, yDataNum, lDeviceId);
		printf("[@(LJXA_ACQ_StartAsync) Initialize HighSpeed](0x%x)\n", errCode);

		//PreStart
		LJX8IF_HIGH_SPEED_PRE_START_REQ startReq;
		startReq.bySendPosition = 2;
		LJX8IF_PROFILE_INFO profileInfo;

		errCode = LJX8IF_PreStartHighSpeedDataCommunication(lDeviceId, &startReq, &profileInfo);
		printf("[@(LJXA_ACQ_StartAsync) PreStart](0x%x)\n", errCode);

		//zUnit 
		errCode = LJX8IF_GetZUnitSimpleArray(lDeviceId, &zUnit);
		if (errCode != 0 || zUnit == 0)
		{
			printf("Failed to acquire zunit.\n");

			//Free memory
			if (_heightBuf[lDeviceId] != NULL) {
				free(_heightBuf[lDeviceId]);
			}

			if (_luminanceBuf[lDeviceId] != NULL) {
				free(_luminanceBuf[lDeviceId]);
			}
			return errCode;
		}

		//Start HighSpeed
		_imageAvailable[lDeviceId] = 0;
		_lastImageSizeHeight[lDeviceId] = 0;

		errCode = LJX8IF_StartHighSpeedDataCommunication(lDeviceId);
		printf("[@(LJXA_ACQ_StartAsync) Start HighSpeed](0x%x)\n", errCode);

		//StartMeasure(Batch Start)
		if (use_external_batchStart > 0) {
		}
		else {
			errCode = LJX8IF_StartMeasure(lDeviceId);
			printf("[@(LJXA_ACQ_StartAsync) Measure Start(Batch Start)](0x%x)\n", errCode);
		}

		//---------------------------------------------------------------------
		//  Organaize parameters related to acquired image 
		//---------------------------------------------------------------------

		_getParam[lDeviceId].luminance_enabled = profileInfo.byLuminanceOutput;
		_getParam[lDeviceId].x_pointnum = profileInfo.wProfileDataCount;

		//_getParam[lDeviceId].y_linenum_acquired : This parameter is unkown at this stage. Set later in "LJXA_ACQ_AcquireAsync" function.

		_getParam[lDeviceId].x_pitch_um = profileInfo.lXPitch / 100.0f;
		_getParam[lDeviceId].y_pitch_um = setParam->y_pitch_um;
		_getParam[lDeviceId].z_pitch_um = zUnit / 100.0f;

		return LJX8IF_RC_OK;
	}

	LJXA_ACQ_API int LJXA_ACQ_AcquireAsync(int lDeviceId, unsigned short *heightImage, unsigned short *luminanceImage, LJXA_ACQ_SETPARAM *setParam, LJXA_ACQ_GETPARAM *getParam) {
		int errCode;

		int yDataNum = setParam->y_linenum;

		//Allocated memory?
		if (_heightBuf[lDeviceId] == NULL || _luminanceBuf[lDeviceId] == NULL) {
			return LJX8IF_RC_ERR_NOMEMORY;
		}

		if (_imageAvailable[lDeviceId] != 1) {
			return LJX8IF_RC_ERR_TIMEOUT;
		}
		printf(" [@(LJXA_ACQ_AcquireAsync) done]\n");

		//Stop HighSpeed
		errCode = LJX8IF_StopHighSpeedDataCommunication(lDeviceId);
		printf("[@(LJXA_ACQ_AcquireAsync) Stop HighSpeed](0x%x)\n", errCode);

		//---------------------------------------------------------------------
		//  Organaize parameters related to acquired image 
		//---------------------------------------------------------------------
		//The rest parameters are preset in "LJXA_ACQ_StartAsync" function.

		_getParam[lDeviceId].y_linenum_acquired = _lastImageSizeHeight[lDeviceId];
		*getParam = _getParam[lDeviceId];

		//---------------------------------------------------------------------
		//  Copy internal buffer to user buffer
		//---------------------------------------------------------------------
		int xDataNum = _getParam[lDeviceId].x_pointnum;

		unsigned short *dwHeightBuf = (unsigned short*)&_heightBuf[lDeviceId][0];
		memcpy(heightImage, dwHeightBuf, xDataNum*yDataNum * 2);

		if (_getParam[lDeviceId].luminance_enabled > 0) {
			unsigned short *dwLuminanceBuf = (unsigned short*)&_luminanceBuf[lDeviceId][0];
			memcpy(luminanceImage, dwLuminanceBuf, xDataNum * yDataNum * 2);
		}

		//Free memory
		if (_heightBuf[lDeviceId] != NULL) {
			free(_heightBuf[lDeviceId]);
		}

		if (_luminanceBuf[lDeviceId] != NULL) {
			free(_luminanceBuf[lDeviceId]);
		}
		return LJX8IF_RC_OK;
	}
}

mutex mtx;

void myCallbackFunc(LJX8IF_PROFILE_HEADER * pProfileHeaderArray, WORD * pHeightProfileArray, WORD * pLuminanceProfileArray, DWORD dwLuminanceEnable, DWORD dwProfileDataCount, DWORD dwCount, DWORD dwNotify, DWORD dwUser)
{
	if ((dwNotify != 0) || (dwNotify & 0x10000) != 0) return;
	if (dwCount == 0) return;
	if (_imageAvailable[dwUser] == 1) return;

	lock_guard<mutex> lock(mtx);

	memcpy(&_heightBuf[dwUser][0], pHeightProfileArray, dwProfileDataCount * dwCount * 2);

	if (dwLuminanceEnable == 1)
	{
		memcpy(&_luminanceBuf[dwUser][0], pLuminanceProfileArray, dwProfileDataCount * dwCount * 2);
	}

	_imageAvailable[dwUser] = 1;
	_lastImageSizeHeight[dwUser] = dwCount;
}

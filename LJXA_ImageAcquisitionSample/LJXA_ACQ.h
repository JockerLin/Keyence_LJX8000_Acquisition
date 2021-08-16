//Copyright (c) 2020 KEYENCE CORPORATION. All rights reserved.


#ifndef _LJXA_ACQ_H
#define _LJXA_ACQ_H

typedef struct {
	int 	y_linenum;
	float	y_pitch_um;
	int		timeout_ms;
	int		use_external_batchStart;
} LJXA_ACQ_SETPARAM;

typedef struct {
	int		luminance_enabled;
	int		x_pointnum;
	int		y_linenum_acquired;
	float	x_pitch_um;
	float	y_pitch_um;
	float	z_pitch_um;
} LJXA_ACQ_GETPARAM;

const int MAX_LJXA_DEVICENUM = 6;
const int MAX_LJXA_XDATANUM = 3200;
const unsigned int BUFFER_FULL_COUNT = 30000;

#ifdef LJXA_ACQ_API_EXPORT
#define LJXA_ACQ_API __declspec(dllexport)
#else
#define LJXA_ACQ_API __declspec(dllimport)
#endif

extern "C"
{
	LJXA_ACQ_API int LJXA_ACQ_OpenDevice(int lDeviceId, LJX8IF_ETHERNET_CONFIG *EthernetConfig, int HighSpeedPortNo);
	LJXA_ACQ_API void LJXA_ACQ_CloseDevice(int lDeviceId);

	//Blocking I/F
	LJXA_ACQ_API int LJXA_ACQ_Acquire(int lDeviceId, unsigned short *heightImage, unsigned short *luminanceImage, LJXA_ACQ_SETPARAM *setParam, LJXA_ACQ_GETPARAM *getParam);

	//Non-blocking I/F
	LJXA_ACQ_API int LJXA_ACQ_StartAsync(int lDeviceId, LJXA_ACQ_SETPARAM *setParam);
	LJXA_ACQ_API int LJXA_ACQ_AcquireAsync(int lDeviceId, unsigned short *heightImage, unsigned short *luminanceImage, LJXA_ACQ_SETPARAM *setParam, LJXA_ACQ_GETPARAM *getParam);
}

#endif /* _LJXA_ACQ_H */
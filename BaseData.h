#ifndef _BASE_H_
#define _BASE_H_

#include "stdio.h"
#include "stdlib.h"
#include "memory.h"
#include <time.h>

//#include "../include/devfile.h"
//#include "../include/filesystem.h"

#define TD_DRV_VIDEO_STARDARD_NTSC 1
#define TD_DRV_VIDEO_STARDARD_PAL 0

#define TD_DRV_VIDEO_SIZE_960P  9
#define TD_DRV_VIDEO_SIZE_576P  8
#define TD_DRV_VIDEO_SIZE_480P 7
#define TD_DRV_VIDEO_SIZE_1080P 6
#define TD_DRV_VIDEO_SIZE_720P 5
#define TD_DRV_VIDEO_SIZE_960 4
#define TD_DRV_VIDEO_SIZE_CIF 2
#define TD_DRV_VIDEO_SIZE_HD1 1
#define TD_DRV_VIDEO_SIZE_D1 0



#define CAP_WIDTH 720
#define CAP_HEIGHT_PAL 576
#define CAP_HEIGHT_NTSC 480

#define CAP_WIDTH_CIF 352
#define CAP_HEIGHT_CIF_PAL 288
#define CAP_HEIGHT_CIF_NTSC 240

#define CAP_WIDTH_HD1 720
#define CAP_HEIGHT_HD1_PAL 288
#define CAP_HEIGHT_HD1_NTSC 240


#define IMG_720P_WIDTH		(1280)
#define IMG_720P_HEIGHT	(720)

#define IMG_1080P_WIDTH	(1920)
#define IMG_1080P_HEIGHT	(1072)

#define IMG_960_WIDTH		(960)
#define IMG_NTSC_960_HEIGHT	(480)
#define IMG_PAL_960_HEIGHT	(576)

#define IMG_480P_WIDTH	(640)
#define IMG_480P_HEIGHT	(480)

#define IMG_480P_DEC_WIDTH	 (704)
#define IMG_480P_DEC_HEIGHT (576)



#define VIDEO_FRAME 1
#define AUDIO_FRAME 2
#define DATA_FRAME  3

#define AUDIO_MAX_BUF_SIZE (8000)
#define AUDIO_MAX_SEND_NUM (16)

#define ERR_NET   -1000
#define ERR_NET_INIT1  ERR_NET-1
#define ERR_NET_ALREADY_WORK	ERR_NET-2
#define ERR_NET_NOT_INIT		ERR_NET-3
#define ERR_NET_BUILD			ERR_NET-4
#define ERR_NET_LISTEN			ERR_NET-5
#define ERR_NET_SET_ACCEPT_SOCKET	ERR_NET-6
#define ERR_NET_DATE			ERR_NET-7
#define ERR_MAX_USER			ERR_NET-8
#define ERR_ALREADY_MOST_USER ERR_NET-9
#define ERR_DVR_IS_NOT_ALIVE  ERR_NET-10
#define ERR_CREATE_HOLE  ERR_NET-11
#define ERR_CONNECT_DVR_HOLE  ERR_NET-12

#define NET_DVR_NET_COMMAND (1001)
#define NET_DVR_NET_FRAME (1002)
#define NET_DVR_NET_SELF_DATA (1003)


#define SVR_LISTEN_PORT		10088
#define SVR_CONNECT_PORT	10116
#define SVR_TO_DVR_PORT	10126
#define DVR_TO_PC_PORT  10138
#define BASE_PORT		10200
#define MAX_P2P_CLIENT 2


enum
{
	COMMAND_GET_FILELIST,
	COMMAND_OPEN_NET_PLAY_FILE,
	COMMAND_GET_FRAME,
	COMMAND_SET_TIMEPOS,
	COMMAND_STOP_NET_PLAY_FILE,
	PLAY_STOP,
	PLAY_PAUSE,
	NET_FILE_PLAYING,
	NET_FILE_OPEN_ERROR,
	NET_FILE_OPEN_SUCCESS,
	USER_TOO_MUCH,
	GET_LOCAL_VIEW,
	STOP_LOCAL_VIEW,
	GET_NET_VIEW,
	STOP_NET_CONNECT,
	SET_NET_PARAMETER,
	NET_KEYBOARD,
	OTHER_INFO,
	SEND_USER_NAME_PASS,
	CREATE_NET_KEY_FRAME,
	GET_SYSTEM_FILE_FROM_PC,
	EXECUTE_SHELL_FILE,
	TIME_FILE_PLAY,
	COMMAND_SEND_MAC_PARA,
	COMMAND_SET_MAC_PARA,
	COMMAND_SET_MAC_KEY,
	COMMAND_SEND_AUDIO_DATA,
	COMMAND_SEND_GUI_MSG,
	COMMAND_SET_GUI_MSG,
	NET_CAM_BIND_ENC,
	NET_CAM_SEND_DATA,
	NET_CAM_STOP_SEND,
	NET_CAM_ENC_SET,
	NET_CAM_FILE_OPEN,
	NET_CAM_FILE_WRITE,
	NET_CAM_FILE_CLOSE,
	NET_CAM_SEND_INTERNATIONAL_DATA,
	NET_CAM_GET_MAC,
	COMMAND_SET_AUDIO_SOCKET,
	COMMAND_AUDIO_SET,
	NET_CAM_GET_FILE_OPEN,
	NET_CAM_GET_FILE_READ,
	NET_CAM_GET_FILE_CLOSE,
	NET_CAM_WRITE_DVR_SVR,
	NET_CAM_NET_MODE,
	NET_CAM_WIFI_ENABLE,
	NET_CAM_GET_CAM_TYPE
};


typedef struct _NET_COMMAND_
{
	int command;
	int length;
	int page;
	int play_fileId;
	time_t searchDate;	
}NET_COMMAND;

typedef struct _BUF_FRAME_HEADER_
{
	unsigned long type;
	unsigned long iLength;
	unsigned long iFrameType;
	unsigned long ulTimeSec;
	unsigned long ulTimeUsec;
	unsigned long standard;
	unsigned long size;
	unsigned long index;
	unsigned long frame_num;
}BUF_FRAMEHEADER;


typedef struct _ENCODE_ITEM
{
	int encode_fd;
	int ichannel;    //通道号
	int mode;        // 制式 N /P
	int size;		  // cif  d1 hd1
	int width;
	int height;
	unsigned char * mmp_addr; // 映射地址
//	H264RateControl     ratec;
	int iframe_num;
	int enc_quality;
	int favc_quant;
	int bitstream;
	int framerate;
	int create_key_frame;  // 产生关键帧 
	int is_key_frame;
	int cut_pic;
	int cut_x;
	int cut_y;
	int u32MaxQuant;
	int u32MinQuant;
	int u32IPInterval;
	int enc_frame_num;
	int gop;
	int VeGroup; //视频组
	int is_open_encode;
	int is_open_decode;
	int is_open_motiondetect;
	int is_start_motiondetect;
	int is_net_enc; //是不是网络引擎
	int decode_is_use_stream;
	unsigned short * motion_buf;
}ENCODE_ITEM;



#endif //_BASE_H_



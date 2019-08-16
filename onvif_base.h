#ifndef _ONVIF_BASE_
#define _ONVIF_BASE_


typedef struct _RTSP_URL_ARRAY_
{
	char url1[100];
	char url2[100];
	char username[20];
	char password[20];
}RTSP_URL_ARRAY;

#define IPCAM_MAX_RTSP_NUM (4)

#define ONVIF_NET_MAX_BUF  (0x100000/4)

typedef struct _IPCAM_INFO_
{
	char ip[100];
	char username[20];
	char password[20];
	char token[IPCAM_MAX_RTSP_NUM][100];
	char ptz_token[IPCAM_MAX_RTSP_NUM][100];
	char url[IPCAM_MAX_RTSP_NUM][100];	
	char Analytics_XAddr[100];	
	char Device_XAddr[100];	
	char Events_XAddr[100];	
	char Imaging_XAddr[100];	
	char Media_XAddr[100];	
	char PTZ_XAddr[100];	
	int gop[IPCAM_MAX_RTSP_NUM];
	int bitstream[IPCAM_MAX_RTSP_NUM];
	int framerate[IPCAM_MAX_RTSP_NUM];
	int height[IPCAM_MAX_RTSP_NUM];
	int width[IPCAM_MAX_RTSP_NUM];
	int code[IPCAM_MAX_RTSP_NUM];
	int getinfo;
	int have_rtsp_num;
	int port;
	int not_need_pass;
	int cam_no;
	int support_motion_detect;
	int recognize_enable_motion_detect_ipcam;
	int motine_CreatePullPointSubscription_ok;
}IPCAM_INFO;

typedef struct  _ONVIF_TIME_
{
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
}ONVIF_TIME;



enum
{
	ONVIF_BASE = 20000,
	GET_CHAN_NUM,
	SET_CHAN_URL,
	STOP_CHAN_RTSP,
	SET_IPCAM_INFO,
	SET_IPCAM_ENC_INFO,
	SET_IPCAM_TIME,
	PTZ_MOVE,
	PTZ_STOP,
	PTZ_SET_PRESET,
	PTZ_REMOVE_PRESET,
	PTZ_GOTO_PRESET,
	SET_IPCAM_IMAGE_PARAMETER,
	MOTION_ALARM,
	SCAN_ONVIF_IPCAM
};

#define ONVIF_SOAP_ERR (-2000)
#define  ONVIF_SOAP_ERR_NOT_NEED_PASSWORD  (ONVIF_SOAP_ERR-1)

#define HOSTNAMELENGTH 80
#define NUM_LOG_PER_PAGE	20
#define SIZE_ALLOC      100
#define ALL  0
#define ANALYTICS 1
#define DEVICE 2
#define EVENTS 3
#define IMAGING 4
#define MEDIA 5
#define PANTILTZOOM 6
#define TRUE 1
#define FALSE 0
#define NUM_IN_CONEC 0
#define NUM_RELAY_CONEC 0
#define MAJOR 1
#define MINOR 0
#define DISCOVERABLE 0
#define NON-DISCOVERABLE 1
#define HTTP 0
#define HTTPS 1
#define RTSP 2
#define IPV4_NTP 0
#define IPV6_NTP 1
#define DNS_NTP 2
#define DATE_MANUAL 0
#define DATE_NTP 1
#define DEFAULT_VALUE 1
#define DEFAULT_SIZE 1
#define NUM_PROFILE 4
#define ANALYTICS_ENABLE 1
#define PTZ_ENABLE 1
#define METADATA_ENABLE 1
#define EXIST 1
#define NOT_EXIST 0
#define SUCCESS 0
#define DEFAULT_ENCODER_PROF {0,"", "", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "", "", 0, 0, 0}
#define DEFAULT_SOURCE_PROF {0,"", 0, "", "", 0, 0, 0, 0}
#define ETH_NAME "eth0"

#define COMMAND_LENGTH 40
#define MID_INFO_LENGTH 40
#define SMALL_INFO_LENGTH 20
#define IP_LENGTH 20
#define INFO_LENGTH 100
#define LARGE_INFO_LENGTH 1024
#define TERMINATE_THREAD 0
#define LINK_LOCAL_ADDR "169.254.166.174"
#define MACH_ADDR_LENGTH 6
#define DEFAULT_SCOPE "onvif://www.onvif.org"
#define ETH_NAME_LOCAL "eth0i"

#define YEAR_IN_SEC (31556926)
#define MONTH_IN_SEC (2629744)
#define DAY_IN_SEC (86400)
#define HOURS_IN_SEC (3600)
#define MIN_IN_SEC (60)
#define MAX_EVENTS (3)
#define KEY_NOTIFY_SEM (1729)
#define MOTION_DETECTED_MASK (1) 	
#define NO_MOTION_MASK (2)	


#endif

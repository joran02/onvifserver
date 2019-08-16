#ifndef _IPCAM_CONFIG_H_
#define _IPCAM_CONFIG_H_

#define ST_VERSION (1106)

#define MAX_RTSP_NUM (3)
#define NORMOL_STRING_NUM (50)

#define CONFIG_FILE_NAME  "/mnt/mtd/ipcam_config.data"
#define ONVIF_SCOPES_ITEM  "onvif://www.onvif.org/type/NetworkVideoTransmitter onvif://www.onvif.org/name/IPNC onvif://www.onvif.org/hardware/00E00ADD00A1 onvif://www.onvif.org/location "
#define ONVIF_MANUFACTURER "IPNC"
#define ONVIF_MODEL "IPCAM_Hi3518C"
#define ONVIF_FirmwareVersion "DM368 IPNC REF DESIGN VERSION 3.10.00.08"


#define IPCAM_NET_DEVICE_LAN 0
#define IPCAM_NET_DEVICE_WIRELESS 1
#define IPCAM_NET_WORK_MODE_IP 0 
#define IPCAM_NET_WORK_MODE_ID 1 

#define IPCAM_USER_NUM_MAX 5

#define IPCAM_USER_MODE_ADMIN (0x1 << 0)
#define IPCAM_USER_MODE_GUEST (0x1 << 1)

#define IPCAM_COLORMATRIX_NUM (10)

#define IPCAM_DAY_NIGHT_MODE_AUTO (0)
#define IPCAM_LIGHT_CONTROL_DAY_ENFORCE   (1)
#define IPCAM_LIGHT_CONTROL_NIGHT_ENFORCE   (2)


#define IPCAM_LIGHT_CONTROL_ADC_RESISTANCE_NORMAL_CHECK_AUTO (0)
#define IPCAM_LIGHT_CONTROL_RESISTANCE_GAIN_CHECK_WORK_AUTO (1)
#define IPCAM_LIGHT_CONTROL_MANUAL_DAY_ENFORCE (2)
#define IPCAM_LIGHT_CONTROL_MANUAL_NIGHT_ENFORCE (3)

enum
{
	IPCAM_CMD_BASE = 30000,
	IPCAM_CMD_GET_ALL_INFO,
	IPCAM_CMD_SET_ENCODE,
	IPCAM_ISP_CMD_saturation,
	IPCAM_ISP_CMD_au16StaticWB,
	IPCAM_ISP_CMD_AEAttr,
	IPCAM_ISP_CMD_vi_csc,
	IPCAM_ISP_CMD_IS_LIGHT_DAY,
	IPCAM_ISP_CMD_Load_Isp_default,
	IPCAM_CMD_SAVE_ALLINFO,
	IPCAM_ISP_CMD_SharpenAttr,
	IPCAM_CODE_SET_PARAMETERS,
	IPCAM_NET_SET_PARAMETERS,
	IPCAM_SYS_SET_PARAMETERS,
	IPCAM_SYS_GET_TIME,
	IPCAM_SYS_SET_TIME,
	IPCAM_SYS_SET_stardard,
	IPCAM_SYS_SET_USER,
	IPCAM_ISP_CMD_Load_Net_Default,
	IPCAM_ISP_CMD_Load_Sys_Default,
	IPCAM_ISP_CMD_DenoiseAttr,
	IPCAM_ISP_GET_COLORTMEP,
	IPCAM_ISP_SET_COLORMATRIX,
	IPCAM_ISP_SET_GAMMA_MODE,	
	IPCAM_ISP_SET_LIGHT_CHEKC_MODE,
	IPCAM_SET_TIME_ZONE
};

typedef struct  _IPCAM_RTSP_ST_
{
	int num;
	int url_port[MAX_RTSP_NUM];
	int server_port[MAX_RTSP_NUM];
	char stream_url[MAX_RTSP_NUM][NORMOL_STRING_NUM];
}IPCAM_RTSP_ST;

typedef struct  _IPCAM_PROFILE_ST_
{
	char profilename[NORMOL_STRING_NUM];
	char profiletoken[NORMOL_STRING_NUM];
	int x;
	int y;
	int h;
	int w;
}IPCAM_PROFILE_ST;

typedef struct  _IPCAM_INPUT_VS_ST_
{
	char profilename[NORMOL_STRING_NUM];
	char profiletoken[NORMOL_STRING_NUM];
	int x;
	int y;
	int h;
	int w;
}IPCAM_INPUT_VS_ST;

typedef struct  _IPCAM_VCODE_VS_ST_
{
	char Name[NORMOL_STRING_NUM];
	char token[NORMOL_STRING_NUM];
	int Encoding;	
	int h;
	int w;
	int Quality;
	int FrameRateLimit;
	int EncodingInterval;
	int BitrateLimit;
	int GovLength;
	int h264_mode; // 0= VBR, 1=CBR
	int H264_profile; //H264 profile  baseline/main/high  (2-0)
}IPCAM_VCODE_VS_ST;

typedef struct _IPCAM_ONVIF_ST_
{
	int __sizeProfiles;
	IPCAM_PROFILE_ST profile_st[MAX_RTSP_NUM];
	IPCAM_INPUT_VS_ST input[MAX_RTSP_NUM];
	IPCAM_VCODE_VS_ST vcode[MAX_RTSP_NUM];
}IPCAM_ONVIF_ST;

typedef struct  _IPCAM_NET_ST_
{
	unsigned char mac[16];
	unsigned char ipv4[8];
	unsigned char ipv6[8];
	unsigned char netmask[8];
	unsigned char gw[8];
	unsigned char dns1[8];
	unsigned char dns2[8];
	int  serv_port;  // IE PORT
	int  net_dev; //IPCAM_NET_DEVICE_LAN  or IPCAM_NET_DEVICE_WIRELESS
	int  net_work_mode; // IPCAM_IP_MODE or IPCAM_ID_MODE
      int  enable_wifi_auto_set_by_nvr; // 1 enable  0 disable
      int  net_use_dhcp; // Ipcam ip use dhcp
      int  show_wireless_status; // Show wifi signal status in pic.
      int  yun_server_open; // Ipcam use yun server mode.
      int  show_yun_id_osd; //Show yun id on osd. 
}IPCAM_NET_ST;

typedef struct _IPCAM_SYS_ST_
{
	long long  ipcam_time;
	int ipcam_stardard;  // TD_DRV_VIDEO_STARDARD_PAL=0     TD_DRV_VIDEO_STARDARD_NTSC=1
	char ipcam_username[IPCAM_USER_NUM_MAX][20];
	char ipcam_password[IPCAM_USER_NUM_MAX][20];
	int ipcam_user_mode[IPCAM_USER_NUM_MAX];  // IPCAM_USER_MODE_ADMIN OR IPCAM_USER_MODE_GUEST
	int op_mode;  // IPCAM_SYS_GET_TIME  OR IPCAM_SYS_SET_TIME
	int onvif_is_open;  //  1 is open onvif server, 0 is close onvif server
	int time_zone;     //¨º¡À?? GMT+08 ¡À¡À?? +8;
	int time_region;  //  TIME_REGION_ASIA=0  TIME_REGION_USA=1 TIME_REGION_EURO=2
	int reserved[10]; //¡À¡ê¨¢?
}IPCAM_SYS_ST;

/*
{
	"GMT-12",// Eniwetok, Kwajalein", 
	"GMT-11",// Midway Island, Samoa",
	"GMT-10",// Hawaii", // HAW10
	"GMT-09",// Alaska", // AKST9AKDT,M3.2.0,M11.1.0
	"GMT-08",// Pacific Time (US & Canada), Tijuana", // PST8PDT,M3.2.0,M11.1.0
	"GMT-07",// Mountain Time (US & Canada), Arizona", // MST7
	"GMT-06",// Central Time (US & Canada), Mexico City, Tegucigalpa, Saskatchewan", // CST6CDT,M3.2.0,M11.1.0
	"GMT-05",// Eastern Time (US & Canada), Indiana(East), Bogota, Lima", // EST5EDT,M3.2.0,M11.1.0
	"GMT-04",// Atlantic Time (Canada), Caracas, La Paz", // AST4ADT,M4.1.0/00:01:00,M10.5.0/00:01:00
	"GMT-03",// Brasilia, Buenos Aires, Georgetown",
	"GMT-02",// Mid-Atlantic",
	"GMT-01",// Azores, Cape Verdes Is.",
	"GMT+00",// GMT, Dublin, Edinburgh, London, Lisbon, Monrovia, Casablanca", // GMT+0BST-1,M3.5.0/01:00:00,M10.5.0/02:00:00
	"GMT+01",// Berlin, Stockholm, Rome, Bern, Brussels, Vienna, Paris, Madrid, Amsterdam, Prague, Warsaw, Budapest", // CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00
	"GMT+02",// Athens, Helsinki, Istanbul, Cairo, Eastern Europe, Harare, Pretoria, Israel", // EET-2EEST-3,M3.5.0/03:00:00,M10.5.0/04:00:00
	"GMT+03",// Baghdad, Kuwait, Nairobi, Riyadh, Moscow, St. Petersburg, Kazan, Volgograd", // MSK-3MSD,M3.5.0/2,M10.5.0/3
	"GMT+04",// Abu Dhabi, Muscat, Tbilisi",
	"GMT+05",// Islamabad, Karachi, Ekaterinburg, Tashkent",
	"GMT+06",// Alma Ata, Dhaka",
	"GMT+07",// Bangkok, Jakarta, Hanoi",
	"GMT+08",// Taipei, Beijing, Chongqing, Urumqi, Hong Kong, Perth, Singapore",
	"GMT+09",// Tokyo, Osaka, Sapporo, Seoul, Yakutsk",
	"GMT+10",// Brisbane, Melbourne, Sydney, Guam, Port Moresby, Vladivostok, Hobart",
	"GMT+11",// Magadan, Solomon Is., New Caledonia",
	"GMT+12",// Fiji, Kamchatka, Marshall Is., Wellington, Auckland"
}
*/

typedef struct _IPCAM_ISP_ST_
{
	int saturation;  //ar0130 0x90;
	int au16StaticWB_0; //ar0130 0x177;
	int au16StaticWB_1;
	int au16StaticWB_2;
	int au16StaticWB_3;
	int u16ExpTimeMax;
	int u16ExpTimeMin;
	int u16AGainMax;
	int u16AGainMin;
	int u16DGainMax;
	int u16DGainMin;
	int u8ExpStep;
	int u32LumaVal;                  /* Luminance: [0 ~ 100] */
	int u32ContrVal;                 /* Contrast: [0 ~ 100] */
	int u32HueVal;                   /* Hue: [0 ~ 100] */
	int u32SatuVal;                  /* Satuature: [0 ~ 100] */
	int u32IsLightDay;          //  day is 1  , night is 0;
	int u16NightExpTimeMax;
	int u16NightExpTimeMin;
	int u16NightAGainMax;
	int u16NightAGainMin;
	int u16NightDGainMax;
	int u16NightDGainMin;
	int u8StrengthTarget;   /*SHARPEN RW,  Range:[0, 0xFF]. */
	int u8StrengthMin;	 /*SHARPEN RW,  Range:[0, u32StrengthTarget]. */
	int u8StrengthUdTarget;
	int u8NightStrengthMin;  // night sharpen  if  u8NightStrengthMin > 0xff   ,   u8StrengthTarget = (u8NightStrengthMin & 0xff00)>>8 , 
						    //u8StrengthMin = u8NightStrengthMin & 0x00ff;
	int u32BackLight;      // 1 enable, 0 disable
	int u32wdr;              //wide dynamic  ,  1 enable , 0 disable
	int u32cds[2];          // IR  change value.
	int u32GameMode;   // 0= auto gama,  1 = gama 0,2 = gama 1,3 = gama 2
	int u32DenoiseMode; //mode : 0 close, 1 auto , 2 Manual_value.
	int u32DenoiseManualValue[3]; // 0-0x7f   low - mid - high
	int u32NightDenoiseManualValue; //
	int u32SharpenAuto;           // 0 is manual, 1is auto.
	int u8StrengthUdTargetOrigen; // default value use sharpenauto.
	int u8StrengthMinOrigen; // default value use sharpenauto.
	int u8StrengthTargetOrigen; // default value use sharpenauto.
	int u8ExpCompensation; //
	int u32ColorTemp;    // 
	char strSensorTypeName[20]; // sensor name
}IPCAM_ISP_ST;


typedef struct _IPCAM_ISP_COLORMATRIX_
{
	char config_name[20];  // camare name
	unsigned short color_array[30];
	int as32CurvePara[6];  /*RW, Range: [0x0, 0xFFFF], AWB curve parameter*/ 
    	unsigned short  au16StaticWB[4];  /*RW, Range: [0x0, 0xFFFF], Static white balance parameter*/
    	unsigned short  u16RefTemp;   /*RW, Range: [0x0, 0xFFFF], the reference color temperature of calibration*/ 
}IPCAM_ISP_COLORMATRIX;

typedef struct _IPCAM_ISP_COLORMATRIX_SET_
{
	int cur_color_index;
	int total_color_num;
	IPCAM_ISP_COLORMATRIX isp_color_st[IPCAM_COLORMATRIX_NUM];
}IPCAM_ISP_COLORMATRIX_SET;


typedef struct _IPCAM_DAY_NIGHT_MODE_
{
	int change_day_night_mode; // IPCAM_LIGHT_CONTROL_ADC_RESISTANCE_NORMAL_CHECK_AUTO
								//IPCAM_LIGHT_CONTROL_RESISTANCE_GAIN_CHECK_WORK_AUTO
	int day_night_mode;			//IPCAM_DAY_NIGHT_MODE_AUTO,IPCAM_LIGHT_CONTROL_DAY_ENFORCE,IPCAM_LIGHT_CONTROL_NIGHT_ENFORCE
	int u32IsLightDay;			 //  day is 1  , night is 0;
	int u32SystemGainMax;           // 300000 - 4000; ?¦Ì¨ª3??¨°?¡ê?????¨¢¨¢?¨¨¦Ì??¡ê
	int u32SlowFrame;                 // 16-32-48-64;  ????sonser¦Ì???1a¨º¡À??¦Ì?¡ê??¦Ì??¡ä¨®?¨ª?¨¢??¨¢¨¢?¡ê
}IPCAM_DAY_NIGHT_MODE;


typedef struct  _IPCAM_ALL_INFO_ST_
{	
	int st_version;
	IPCAM_RTSP_ST rtsp_st;
	IPCAM_ONVIF_ST onvif_st;
	int onvif_server_port;
	char app_version[20];
	IPCAM_ISP_ST isp_st;
	int load_default_isp;
	IPCAM_NET_ST net_st;
	int load_default_net_st;
	IPCAM_SYS_ST sys_st;
	int load_default_sys_st;
	IPCAM_ISP_COLORMATRIX_SET isp_color_st;	 
	IPCAM_ISP_ST isp_night_st;
	IPCAM_DAY_NIGHT_MODE light_check_mode;
}IPCAM_ALL_INFO_ST;

#endif
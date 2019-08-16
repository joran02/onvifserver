#include "stdio.h"
#include "stdlib.h"
#include "memory.h"
#include <time.h>


#include "ipcam_config.h"

#ifndef DPRINTK
#define DPRINTK(fmt, args...)	printf("onvif (%s,%d)%s: " fmt,__FILE__,__LINE__, __FUNCTION__ , ## args)
#endif

static IPCAM_ALL_INFO_ST g_ipcam_st;
static pthread_mutex_t write_lock;


void set_config_st_default(IPCAM_ALL_INFO_ST * pIpcam_st)
{
	int i = 0;
	memset(pIpcam_st,0x00,sizeof(IPCAM_ALL_INFO_ST));
	//memset(&pIpcam_st->isp_st,0xff,sizeof(IPCAM_ISP_ST));

	pIpcam_st->st_version = ST_VERSION;

	pIpcam_st->rtsp_st.num = MAX_RTSP_NUM;
	pIpcam_st->rtsp_st.url_port[0] = 18890;
	pIpcam_st->rtsp_st.url_port[1] = 18990;
	pIpcam_st->rtsp_st.server_port[0] = 16100;
	pIpcam_st->rtsp_st.server_port[1] = 16100;
	//pIpcam_st->onvif_server_port = 18622;
	pIpcam_st->onvif_server_port = 8080;
	strcpy(pIpcam_st->rtsp_st.stream_url[0],"0.ch");
	strcpy(pIpcam_st->rtsp_st.stream_url[1],"1.ch");
	strcpy(pIpcam_st->app_version,"V3.310C");

	for( i = 0; i < MAX_RTSP_NUM; i++)
	{
		if( i == 0 )
		{
			strcpy(pIpcam_st->onvif_st.profile_st[i].profilename,"Profile_0");
			strcpy(pIpcam_st->onvif_st.profile_st[i].profiletoken,"Profile_Token_0");
			strcpy(pIpcam_st->onvif_st.input[i].profilename,"Profile_input_0");
			strcpy(pIpcam_st->onvif_st.input[i].profiletoken,"Profile_input_Token_0");			
			pIpcam_st->onvif_st.input[i].w = 1280;
			pIpcam_st->onvif_st.input[i].h = 720;

			strcpy(pIpcam_st->onvif_st.vcode[i].Name,"Profile_vcode_0");
			strcpy(pIpcam_st->onvif_st.vcode[i].token,"Profile_vcode_Token_0");
			pIpcam_st->onvif_st.vcode[i].w = 1280;
			pIpcam_st->onvif_st.vcode[i].h = 720;			
			pIpcam_st->onvif_st.vcode[i].BitrateLimit  = 2999;			
			pIpcam_st->onvif_st.vcode[i].Encoding = 2;  //JPEG = 0, MPEG4 = 1, H264 = 2;
			pIpcam_st->onvif_st.vcode[i].EncodingInterval = 1;
			pIpcam_st->onvif_st.vcode[i].FrameRateLimit = 25;
			pIpcam_st->onvif_st.vcode[i].GovLength = 25;
			pIpcam_st->onvif_st.vcode[i].Quality = 100;
			pIpcam_st->onvif_st.vcode[i].h264_mode = 0;
			pIpcam_st->onvif_st.vcode[i].H264_profile = 0;  //H264 profile  baseline/main/high  (2-0)
			
		}else
		{
			strcpy(pIpcam_st->onvif_st.profile_st[i].profilename,"Profile_1");
			strcpy(pIpcam_st->onvif_st.profile_st[i].profiletoken,"Profile_Token_1");
			strcpy(pIpcam_st->onvif_st.input[i].profilename,"Profile_input_1");
			strcpy(pIpcam_st->onvif_st.input[i].profiletoken,"Profile_inputToken_1");			
			pIpcam_st->onvif_st.input[i].w = 640;
			pIpcam_st->onvif_st.input[i].h = 480;

			strcpy(pIpcam_st->onvif_st.vcode[i].Name,"Profile_vcode_1");
			strcpy(pIpcam_st->onvif_st.vcode[i].token,"Profile_vcode_Token_1");
			pIpcam_st->onvif_st.vcode[i].w = 640;
			pIpcam_st->onvif_st.vcode[i].h = 480;
			pIpcam_st->onvif_st.vcode[i].BitrateLimit  = 1000;
			pIpcam_st->onvif_st.vcode[i].Encoding = 2;  //JPEG = 0, MPEG4 = 1, H264 = 2;
			pIpcam_st->onvif_st.vcode[i].EncodingInterval = 1;
			pIpcam_st->onvif_st.vcode[i].FrameRateLimit = 25;
			pIpcam_st->onvif_st.vcode[i].GovLength = 25;
			pIpcam_st->onvif_st.vcode[i].Quality = 100;
			pIpcam_st->onvif_st.vcode[i].h264_mode = 0;
			pIpcam_st->onvif_st.vcode[i].H264_profile = 0;  //H264 profile  baseline/main/high  (2-0)
		}
		
		pIpcam_st->onvif_st.__sizeProfiles++;
		
	}

	{
		IPCAM_NET_ST * pNetSt = &pIpcam_st->net_st;
		pNetSt->net_dev = IPCAM_NET_DEVICE_LAN;
		pNetSt->ipv4[0] = 192;
		pNetSt->ipv4[1] = 168;
		pNetSt->ipv4[2] = 1;
		pNetSt->ipv4[3] = 60;

		pNetSt->netmask[0] = 255;
		pNetSt->netmask[1] = 255;
		pNetSt->netmask[2] = 255;
		pNetSt->netmask[3] = 0;

		pNetSt->gw[0] = 192;
		pNetSt->gw[1] = 168;
		pNetSt->gw[2] = 1;
		pNetSt->gw[3] = 1;

		pNetSt->dns1[0] = 8;
		pNetSt->dns1[1] = 8;
		pNetSt->dns1[2] = 8;
		pNetSt->dns1[3] = 8;

		pNetSt->dns2[0] = 9;
		pNetSt->dns2[1] = 9;
		pNetSt->dns2[2] = 9;
		pNetSt->dns2[3] = 9;	

		pNetSt->serv_port = 80;

		pNetSt->net_work_mode = 1004;  //IPCAM_IP_MODE  (1004)
	}

	{
		IPCAM_SYS_ST * pSysSt = &pIpcam_st->sys_st;
		int i = 0;

		pSysSt->op_mode = 0;
		pSysSt->ipcam_time = 0;

		for( i = 0; i < IPCAM_USER_NUM_MAX; i++)
		{
			pSysSt->ipcam_username[i][0] = 0;
			pSysSt->ipcam_password[i][0] = 0;
			pSysSt->ipcam_user_mode[i] = 0;
		}

		strcpy(pSysSt->ipcam_username[0],"admin");
		strcpy(pSysSt->ipcam_password[0],"admin");
		pSysSt->ipcam_user_mode[0] = IPCAM_USER_MODE_ADMIN;
	}


	pIpcam_st->load_default_isp = 1;

	DPRINTK("load default\n");
	
	
}


int load_config_file(IPCAM_ALL_INFO_ST * pIpcam_st)
{
	FILE * fp = NULL;
	int ret;
	
	fp = fopen(CONFIG_FILE_NAME,"rb");
	if( fp == NULL )
	{
		DPRINTK("%s is not exist,default value\n",CONFIG_FILE_NAME);
		goto set_default;
	}

	ret = fread(pIpcam_st,1,sizeof(IPCAM_ALL_INFO_ST),fp);
	if( ret != sizeof(IPCAM_ALL_INFO_ST) )
	{
		DPRINTK("%s read err,default value\n",CONFIG_FILE_NAME);
		goto set_default;
	}

	if( fp )
	{
		fclose(fp);
		fp = NULL;
	}

	pIpcam_st->st_version = ST_VERSION;

	return 1;

set_default:

	if( fp )
	{
		fclose(fp);
		fp = NULL;
	}
	set_config_st_default(pIpcam_st);
	return -1;	
}


int save_ipcam_config_st(IPCAM_ALL_INFO_ST * pIpcam_st)
{
	FILE * fp = NULL;
	int ret;
	static int first = 0;

	DPRINTK("save config\n");

	if(first == 0 )
	{
		first = 1;
		pthread_mutex_init(&write_lock, NULL);
	}

	if(1)
	{
		IPCAM_NET_ST * pNetSt = NULL;	
	
		pNetSt = &pIpcam_st->net_st;
		if( pNetSt->ipv4[0] == 0 )
			pNetSt->ipv4[0] = 1;
		if( pNetSt->ipv4[3] == 0 )
			pNetSt->ipv4[3] = 2;

		if( pNetSt->netmask[0] == 0 )
			pNetSt->netmask[0] = 255;

		if( pNetSt->gw[0] == 0 )
			pNetSt->gw[0] = 1;
		if( pNetSt->gw[3] == 0 )
			pNetSt->gw[3] = 1;
	}

	pthread_mutex_lock(&write_lock);
	
	fp = fopen(CONFIG_FILE_NAME,"wb");
	if( fp == NULL )
	{
		DPRINTK("%s can not create\n",CONFIG_FILE_NAME);
		goto failed;
	}

	ret = fwrite(pIpcam_st,1,sizeof(IPCAM_ALL_INFO_ST),fp);
	if( ret != sizeof(IPCAM_ALL_INFO_ST) )
	{
		DPRINTK("%s write err \n",CONFIG_FILE_NAME);
		goto failed;
	}

	if( fp )
	{
		fclose(fp);
		fp = NULL;
	}

	pthread_mutex_unlock(&write_lock);

	return 1;

failed:	
	if( fp )
	{
		fclose(fp);
		fp = NULL;
	}

	pthread_mutex_unlock(&write_lock);
	return -1;	
}

IPCAM_ALL_INFO_ST * get_ipcam_config_st()
{
	static int first = 0;

	if(first == 0 )
	{
		first = 1;
		load_config_file(&g_ipcam_st);
	}
	
	return &g_ipcam_st;
}


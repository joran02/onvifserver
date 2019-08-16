// devicemgmt.cpp : 定义控制台应用程序的入口点。
//

#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include  <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>

#include <netinet/tcp.h>
#include <pthread.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/un.h>


#include "soapH.h"
#include "DeviceIOBinding.nsmap"
#include "wsseapi.h"
#include "wsaapi.h"
#include "NetModule.h"
#include "stdsoap2.h"
#include "ipcam_config.h"
#include "net_config.h"

#define TRACE printf

#define USE_CMD_THREAD 

#define motion_recv_port (8080)

#define MAX_PTZ_PRESET_NUM (256)

typedef struct PTZ_PRESET_ST_
{
	char Name[50];
	char Token[50];
}PTZ_PRESET_ST;

char *_TZ_name[25] = {
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
};

#define dbg_test 0


NET_MODULE_ST * pCommuniteToOnvif = NULL;
NET_MODULE_ST * pNetModule[MAX_CHANNEL];
static IPCAM_INFO stIpcamInfo[MAX_CHANNEL];
static IPCAM_INFO stIpcamInfo_save[MAX_CHANNEL];
int msg_refress = 0;
static CYCLE_BUFFER * cycle_cmd_buf[MAX_CHANNEL] = {0};
static int onvif_motion_flag[MAX_CHANNEL] = {0};


static char auto_check_dvr_ip_array[MAX_CHANNEL][50];
static int auto_check_ip_num = 0;
static PTZ_PRESET_ST ptz_preset_array[MAX_CHANNEL][MAX_PTZ_PRESET_NUM] = {0};
static unsigned char host_gw_ip[50] = {0};
static IPCAM_ALL_INFO_ST  g_pIpcamAll;
static int get_ipcam_info_once = 0;
time_t current_time;
int _DateTimeType = DATE_MANUAL;


struct server_config_t {
 u_int32_t server; /* Our IP, in network order */
 u_int32_t start; /* Start address of leases, network order */
 u_int32_t end; /* End of leases, network order */
 struct option_set *options; /* List of DHCP options loaded from the config file */
 char *interface; /* The name of the interface to use */
 int ifindex; /* Index number of the interface to use */
 unsigned char arp[6]; /* Our arp address */
 unsigned long lease; /* lease time in seconds (host order) */
 unsigned long max_leases; /* maximum number of leases (including reserved address) */
 char remaining; /* should the lease file be interpreted as lease time remaining, or
 * as the time the lease expires */
 unsigned long auto_time; /* how long should udhcpd wait before writing a config file.
 * if this is zero, it will only write one on SIGUSR1 */
 unsigned long decline_time; /* how long an address is reserved if a client returns a
 * decline message */
 unsigned long conflict_time; /* how long an arp conflict offender is leased for */
 unsigned long offer_time; /* how long an offered address is reserved */
 unsigned long min_lease; /* minimum lease a client can request*/
 char *lease_file;
 char *pidfile;
 char *notify_file; /* What to run whenever leases are written */
 u_int32_t siaddr; /* next server bootp option */
 char *sname; /* bootp server name */
 char *boot_file; /* bootp boot file option */
}; 

int  get_ipcam_all_info();

int check_have_sd();



int IpcamSetValue(void * pBuf , int cmd)
{
	char pc_cmd[201024];
	int ret = 0;
	NET_COMMAND g_cmd;
	IPCAM_ALL_INFO_ST * pAllInfo = (IPCAM_ALL_INFO_ST *)pBuf;

	g_cmd.command = cmd;
			
	memset(pc_cmd,0x00,1024);
	memcpy(pc_cmd,&g_cmd,sizeof(g_cmd));	
	memcpy(pc_cmd + sizeof(g_cmd),pAllInfo,sizeof(IPCAM_ALL_INFO_ST));	
		
	Net_dvr_send_cam_data(pc_cmd,sizeof(g_cmd)+sizeof(IPCAM_ALL_INFO_ST),pCommuniteToOnvif);

	return 1;
}


/***************************************************************************
 *                                                                         *
 ***************************************************************************/
static int netsplit( char *pAddress, void *ip )
{
	unsigned int ret;
	NET_IPV4 *ipaddr = (NET_IPV4 *)ip;

	if ((ret = atoi(pAddress + 9)) > 255)
		return FALSE;
	ipaddr->str[3] = ret;

	*( pAddress + 9 ) = '\x0';
	if ((ret = atoi(pAddress + 6)) > 255)
		return FALSE;
	ipaddr->str[2] = ret;

	*( pAddress + 6 ) = '\x0';
	if ((ret = atoi(pAddress + 3)) > 255)
		return FALSE;
	ipaddr->str[1] = ret;

	*( pAddress + 3 ) = '\x0';
	if ((ret = atoi(pAddress + 0)) > 255)
		return FALSE;
	ipaddr->str[0] = ret;

	return TRUE;
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int ipv4_str_to_num(char *data, struct in_addr *ipaddr)
{
	if ( strchr(data, '.') == NULL )
		return netsplit(data, ipaddr);
	return inet_aton(data, ipaddr);
}


int have_wireless_dev()
{
	int ret = 0;
	ret = system("cat /proc/net/dev | grep ra0");
		
	if( ret == 0)
		return 1;
	else
		return 0;
}

/*
 *  @brief Converts XML period format (PnYnMnDTnHnMnS) to long data type
 */
/*
long periodtol(char *ptr)
{
	char buff[10] = "0";
	char convert;
	int i = 0; 
	int value = 0;
	int time = 0;
	int minus = 0;
	long cumulative = 0;
	if(*ptr == '-')
	{
		ptr++;
		minus = 1;
	}
	while(*ptr != '\0')
	{
		if(*ptr != 'P' || *ptr != 'T' || *ptr >= '0' || *ptr <= '9')
		{
			return -1;
		}
		if(*ptr == 'P' || *ptr == 'T')
		{
			ptr++;
			if(*ptr == 'T')
			{
				time = 1;
				ptr++;
			}
		}
		else
		{
			if(*ptr >= '0' && *ptr <= '9')
			{
				buff[i] = *ptr;
				i++;
				ptr++;
			}
			else
			{
				buff[i] = 0;
				value = atoi(buff);
				memset(buff, 0, sizeof(buff));
				i = 0;
				convert = *ptr;
				ptr++;
				cumulative = cumulative + epoch_convert_switch(value, convert, time);
			}
		}
	}
	if(minus == 1)
	{
		return -cumulative;
	}
	else
	{
		return cumulative;
	}
}
*/

unsigned long iptol(char *ip)
{
	unsigned char o1,o2,o3,o4; /* The 4 ocets */
	char tmp[13] = "000000000000\0";
	short i = 11; /* Current Index in tmp */
	short j = (strlen(ip) - 1);
	do 
	{
		if ((ip[--j] == '.'))
		{
			i -= (i % 3);
		}
		else 
		{
			tmp[--i] = ip[j];
		}
	}while (i > -1);

	o1 = (tmp[0] * 100) + (tmp[1] * 10) + tmp[2];
	o2 = (tmp[3] * 100) + (tmp[4] * 10) + tmp[5];
	o3 = (tmp[6] * 100) + (tmp[7] * 10) + tmp[8];
	o4 = (tmp[9] * 100) + (tmp[10] * 10) + tmp[11];
	return (o1 * 16777216) + (o2 * 65536) + (o3 * 256) + o4;
}

/* @brief Check if IP is valid */
int isValidIp4 (char *str) 
{
	int segs = 0;   /* Segment count. */     
	int chcnt = 0;  /* Character count within segment. */     
	int accum = 0;  /* Accumulator for segment. */      
	/* Catch NULL pointer. */      
	if (str == NULL) return 0;      
	/* Process every character in string. */      
	while (*str != '\0') 
	{         
		/* Segment changeover. */          
		if (*str == '.') 
		{             
			/* Must have some digits in segment. */              
			if (chcnt == 0) return 0;              
			/* Limit number of segments. */              
			if (++segs == 4) return 0;              
			/* Reset segment values and restart loop. */              
			chcnt = accum = 0;             
			str++;             
			continue;         
		}  

		/* Check numeric. */          
		if ((*str < '0') || (*str > '9')) return 0;
		/* Accumulate and check segment. */          
		if ((accum = accum * 10 + *str - '0') > 255) return 0;
		/* Advance other segment specific stuff and continue loop. */          
		chcnt++;         
		str++;     
	}      
	/* Check enough segments and enough characters in last segment. */      
	if (segs != 3) return 0;      
	if (chcnt == 0) return 0;      
	/* Address okay. */      
	return 1; 
} 

/* @brief Check if a hostname is valid */
int isValidHostname (char *str) 
{
	/* Catch NULL pointer. */      
	if (str == NULL) 
	{
		return 0;      
	}
	/* Process every character in string. */      
	while (*str != '\0') 
	{         
		if ((*str >= 'a' && *str <= 'z') || (*str >= 'A' && *str <= 'Z') || (*str >= '0' && *str <= '9') || (*str == '.') || (*str == '-') )
		{
			str++;
		}
		else
		{
			return 0;
		}
	}
	return 1; 
}

char *oget_timezone(int no)
{
	return _TZ_name[no];
}

int oset_timezone(char *TZ)
{
	if (strncmp(TZ, "IDLW", 4)==0)return 0;//-12
	else if (strncmp(TZ, "NT", 2)==0)return 1;//-11
	else if ((strncmp(TZ, "AHST", 4)==0)||(strncmp(TZ, "CAT", 3)==0)||(strncmp(TZ, "HST", 3)==0)||(strncmp(TZ, "HDT", 3)==0))return 2;//-10
	else if ((strncmp(TZ, "YST", 3)==0)||(strncmp(TZ, "YDT", 3)==0))return 3;//-9
	else if ((strncmp(TZ, "PST", 3)==0)||(strncmp(TZ, "PDT", 3)==0))return 4;//-8
	else if ((strncmp(TZ, "MST", 3)==0)||(strncmp(TZ, "MDT", 3)==0))return 5;//-7
	else if ((strncmp(TZ, "CST", 3)==0)||(strncmp(TZ, "CDT", 3)==0))return 6;//-6
	else if ((strncmp(TZ, "EST", 3)==0)||(strncmp(TZ, "EDT", 3)==0))return 7;//-5
	else if ((strncmp(TZ, "AST", 3)==0)||(strncmp(TZ, "ADT", 3)==0))return 8;//-4
	else if (strncmp(TZ, "GMT-03", 6)==0)return 9;//-3
	else if (strncmp(TZ, "AT", 2)==0)return 10;//-2
	else if (strncmp(TZ, "WAT", 3)==0)return 11;//-1
	else if ((strncmp(TZ, "GMT", 3)==0)||(strncmp(TZ, "UT", 2)==0)||(strncmp(TZ, "UTC", 3)==0)||(strncmp(TZ, "BST", 3)==0))return 12;//-0
	else if ((strncmp(TZ, "CET", 3)==0)||(strncmp(TZ, "FWT", 3)==0)||(strncmp(TZ, "MET", 3)==0)\
		||(strncmp(TZ, "MEWT", 4)==0)||(strncmp(TZ, "SWT", 3)==0)||(strncmp(TZ, "MEST", 4)==0)\
		||(strncmp(TZ, "MESZ", 4)==0)||(strncmp(TZ, "SST", 3)==0)||(strncmp(TZ, "FST", 3)==0))return 13;//1
	else if (strncmp(TZ, "EET", 3)==0)return 14;//2
	else if (strncmp(TZ, "BT", 2)==0)return 15;//3
	else if (strncmp(TZ, "ZP4", 3)==0)return 16;//4
	else if (strncmp(TZ, "ZP5", 3)==0)return 17;//5
	else if (strncmp(TZ, "ZP6", 3)==0)return 18;//6
	else if (strncmp(TZ, "ZP7", 3)==0)return 19;//7
	else if (strncmp(TZ, "WAST", 4)==0)return 20;//8
	else if (strncmp(TZ, "JST", 3)==0)return 21;//9
	else if (strncmp(TZ, "ACT", 3)==0)return 22;//10
	else if (strncmp(TZ, "EAST", 4)==0)return 23;//11
	else if (strncmp(TZ, "IDLE", 4)==0)return 24;//12
	else return 100;//ERROR
	
	
}

int sys_set_date(int year, int month, int day)
{
	struct tm *tm;
	time_t now;
	unsigned char v2;

	now = time(NULL);
	tm = localtime(&now);

	year = (year>1900) ? year-1900 : 0;
	tm->tm_year = year;
	month = (month>0) ? month-1 : 0;
	tm->tm_mon = month;
	tm->tm_mday = day;

	if ((now = mktime(tm)) < 0)
		return -1;
	
	stime(&now);
	//system("hwclock -uw");

	return 0;
}

int sys_set_time(int hour, int min, int sec)
{
	struct tm *tm;
	time_t now;
	unsigned char v2;

	now = time(NULL);
	tm = localtime(&now);

	tm->tm_hour = hour;
	tm->tm_min = min;
	tm->tm_sec = sec + 3;

	if ((now = mktime(tm)) < 0)
		return -1;
	
	stime(&now);
	//system("hwclock -uw");

	return 0;
}


int onvif_fault(struct soap *soap,char *value1,char *value2)
{
	soap->fault = (struct SOAP_ENV__Fault*)soap_malloc(soap,(sizeof(struct SOAP_ENV__Fault)));
	soap->fault->SOAP_ENV__Code = (struct SOAP_ENV__Code*)soap_malloc(soap,(sizeof(struct SOAP_ENV__Code)));
	soap->fault->SOAP_ENV__Code->SOAP_ENV__Value = "SOAP-ENV:Sender";
	soap->fault->SOAP_ENV__Code->SOAP_ENV__Subcode = (struct SOAP_ENV__Code*)soap_malloc(soap,(sizeof(struct SOAP_ENV__Code)));;
	soap->fault->SOAP_ENV__Code->SOAP_ENV__Subcode->SOAP_ENV__Value = value1;
	soap->fault->SOAP_ENV__Code->SOAP_ENV__Subcode->SOAP_ENV__Subcode = (struct SOAP_ENV__Code*)soap_malloc(soap,(sizeof(struct SOAP_ENV__Code)));
	soap->fault->SOAP_ENV__Code->SOAP_ENV__Subcode->SOAP_ENV__Subcode->SOAP_ENV__Value = value2;
	soap->fault->SOAP_ENV__Code->SOAP_ENV__Subcode->SOAP_ENV__Subcode->SOAP_ENV__Subcode = NULL;
	soap->fault->faultcode = NULL;
	soap->fault->faultstring = NULL;
	soap->fault->faultactor = NULL;
	soap->fault->detail = NULL;
	soap->fault->SOAP_ENV__Reason = NULL;
	soap->fault->SOAP_ENV__Node = NULL;
	soap->fault->SOAP_ENV__Role = NULL;
	soap->fault->SOAP_ENV__Detail = NULL;
}

SOAP_FMAC5 int SOAP_FMAC6 __dndl__Probe(struct soap* soap, struct d__ProbeType *dn__Probe, struct d__ProbeMatchesType *dn__ProbeResponse)
{

	char macaddr[MACH_ADDR_LENGTH];
	char _IPAddr[INFO_LENGTH];
	char _HwId[LARGE_INFO_LENGTH];	

	TRACE("%s in \n",__FUNCTION__);
		
	dn__ProbeResponse->ProbeMatch = (struct d__ProbeMatchType *)soap_malloc(soap, sizeof(struct d__ProbeMatchType));
	soap_default_d__ProbeMatchType(soap, dn__ProbeResponse->ProbeMatch);
	soap_default_wsa__EndpointReferenceType(soap,&dn__ProbeResponse->ProbeMatch->wsa__EndpointReference);

	dn__ProbeResponse->ProbeMatch->XAddrs = (char *)soap_malloc(soap, sizeof(char) * INFO_LENGTH);
	dn__ProbeResponse->ProbeMatch->Types = (char *)soap_malloc(soap, sizeof(char) * INFO_LENGTH);
	dn__ProbeResponse->ProbeMatch->Scopes = (struct d__ScopesType*)soap_malloc(soap,sizeof(struct d__ScopesType));
	soap_default_d__ScopesType(soap, dn__ProbeResponse->ProbeMatch->Scopes);
	dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.ReferenceProperties = (struct wsa__ReferencePropertiesType*)soap_malloc(soap,sizeof(struct wsa__ReferencePropertiesType));
	soap_default_wsa__ReferencePropertiesType(soap, dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.ReferenceProperties);
	dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.ReferenceParameters = (struct wsa__ReferenceParametersType*)soap_malloc(soap,sizeof(struct wsa__ReferenceParametersType));
	soap_default_wsa__ReferenceParametersType(soap, dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.ReferenceParameters );
	dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.ServiceName = (struct wsa__ServiceNameType*)soap_malloc(soap,sizeof(struct wsa__ServiceNameType));
	soap_default_wsa__ServiceNameType(soap, dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.ServiceName );
	dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.PortType = (char **)soap_malloc(soap, sizeof(char *) * SMALL_INFO_LENGTH);
	dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.__any = (char **)soap_malloc(soap, sizeof(char*) * SMALL_INFO_LENGTH);
	dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.__anyAttribute = (char *)soap_malloc(soap, sizeof(char) * SMALL_INFO_LENGTH);
	dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.Address = (char *)soap_malloc(soap, sizeof(char) * INFO_LENGTH);

		
	NET_IPV4 ip;
	net_get_hwaddr(ETH_NAME, macaddr);
	sprintf(_HwId,"urn:uuid:1419d68a-1dd2-11b2-a105-%02X%02X%02X%02X%02X%02X",macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);

	ip.int32 = net_get_ifaddr(ETH_NAME);
	sprintf(_IPAddr, "http://%d.%d.%d.%d:%d/onvif/device_service", ip.str[0], ip.str[1], ip.str[2], ip.str[3],g_pIpcamAll.onvif_server_port);

		
	dn__ProbeResponse->__sizeProbeMatch = 1;
	dn__ProbeResponse->ProbeMatch->Scopes->__item =(char *)soap_malloc(soap, LARGE_INFO_LENGTH);
	memset(dn__ProbeResponse->ProbeMatch->Scopes->__item,0,LARGE_INFO_LENGTH);	
	
	strcpy(dn__ProbeResponse->ProbeMatch->Scopes->__item,ONVIF_SCOPES_ITEM);

		

	dn__ProbeResponse->ProbeMatch->Scopes->MatchBy = NULL;
	strcpy(dn__ProbeResponse->ProbeMatch->XAddrs, _IPAddr);
	strcpy(dn__ProbeResponse->ProbeMatch->Types, "dn:NetworkVideoTransmitter");
	dn__ProbeResponse->ProbeMatch->MetadataVersion = 1;
	dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.ReferenceProperties->__size = 0;
	dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.ReferenceProperties->__any = NULL;
	dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.ReferenceParameters->__size = 0;
	dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.ReferenceParameters->__any = NULL;
	dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.PortType[0] = (char *)soap_malloc(soap, sizeof(char) * SMALL_INFO_LENGTH);
	strcpy(dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.PortType[0], "ttl");
	dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.ServiceName->__item = NULL;
	dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.ServiceName->PortName = NULL;
	dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.ServiceName->__anyAttribute = NULL;
	dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.__any[0] = (char *)soap_malloc(soap, sizeof(char) * SMALL_INFO_LENGTH);
	strcpy(dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.__any[0], "Any");
	strcpy(dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.__anyAttribute, "Attribute");
	dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.__size = 0;
	strcpy(dn__ProbeResponse->ProbeMatch->wsa__EndpointReference.Address, _HwId);
	

	if (soap->header)
	{
      	    static char MessageID[100];    	    
    	    static struct wsa__Relationship wsa_RelatesTo;
	    static int num = 0;
          sprintf(MessageID, "urn:uuid:11223344-5566-7788-%0.4d-000102030405ff",num);

		  num++;
		  if( num > 9998)
		  	num = 0;
 	

		if(soap->header->wsa__MessageID)
		{
			printf("remote wsa__MessageID : %s\n",soap->header->wsa__MessageID);
 			soap->header->wsa__RelatesTo =&wsa_RelatesTo;
			soap_default__wsa__RelatesTo(soap, soap->header->wsa__RelatesTo);
			soap->header->wsa__RelatesTo->__item = soap->header->wsa__MessageID;

			soap->header->wsa__MessageID = MessageID;
			soap->header->wsa__To = "http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous";
			soap->header->wsa__Action = "http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches";
		}
	}

/*

	static struct d__ProbeMatchType ProbeMatch;
	static struct d__ScopesType scopes;
    static char MessageID[100];
    static char szXAddr[64];
    static char szEndpointReference[64];
    static struct wsa__Relationship wsa_RelatesTo;

   	unsigned char MacAddr[8] ={0x00, 0x16, 0xe8, 0x53, 0x13, 0xc7};
    int localIP = 0x6207A8C0;
    //localIP = niGetLocalIPAddr(g_netEthNi);
    //memcpy(MacAddr, niGetHwAddr(g_netEthNi), 6);

    //    if(d__Probe->Types && strcasecmp(d__Probe->Types,"dn:NetworkVideoTransmitter")==0)

    sprintf(szXAddr, "http://%s/onvif/device_service", inet_ntoa(*((struct in_addr *)&localIP)));
	sprintf(szEndpointReference, "urn:uuid:11323344-5566-7788-99aa-%02x%02x%02x%02x%02x%02x",
                        MacAddr[0],MacAddr[1],MacAddr[2],MacAddr[3],MacAddr[4],MacAddr[5]);

	//printf("__dndl__Probe %x, %x, %d\n", d__ProbeMatches, d__ProbeMatches->ProbeMatch, d__ProbeMatches->__sizeProbeMatch);
	soap_default_d__ProbeMatchType(soap, &ProbeMatch);
	soap_default_d__ScopesType(soap, &scopes);
	
	if (soap->header)
	{
       // uuid_t uuid;
		
       // uuid_generate(uuid);
          strcpy(MessageID, "urn:uuid:11223344-5566-7788-9999-000102030405ff");
 	//	strcpy(MessageID, "{X-X-X-XX-XXXXXX}");	
        //uuid_unparse(uuid, MessageID+9);

		//NewGuid(MessageID);

		if(soap->header->wsa__MessageID)
		{
			printf("remote wsa__MessageID : %s\n",soap->header->wsa__MessageID);
 			soap->header->wsa__RelatesTo =&wsa_RelatesTo;
			soap_default__wsa__RelatesTo(soap, soap->header->wsa__RelatesTo);
			soap->header->wsa__RelatesTo->__item = soap->header->wsa__MessageID;

			soap->header->wsa__MessageID = MessageID;
			soap->header->wsa__To = "http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous";
			soap->header->wsa__Action = "http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches";
		}
	}

	scopes.__item =  "onvif://www.onvif.org/type/video_encoder onvif://www.onvif.org/type/audio_encoder onvif://www.onvif.org/hardware/IPC-model onvif://www.onvif.org/name/IPC-model";
	
	ProbeMatch.wsa__EndpointReference.Address = szEndpointReference;//"urn:uuid:464A4854-4656-5242-4530-313035394100";
	ProbeMatch.Types = "dn:NetworkVideoTransmitter";
	ProbeMatch.Scopes = &scopes;
	ProbeMatch.XAddrs = szXAddr;//"http://192.168.7.98/onvif/device_service";
	ProbeMatch.MetadataVersion = 1;

	dn__ProbeResponse->__sizeProbeMatch = 1;
	dn__ProbeResponse->ProbeMatch = &ProbeMatch;
	*/

TRACE("%s out \n",__FUNCTION__);
	return SOAP_OK;
}

SOAP_FMAC5 int SOAP_FMAC6 __dnrd__Hello(struct soap* soap, struct d__HelloType *dn__Hello, struct d__ResolveType *dn__HelloResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);onvif_fault(soap,"ter:InvalidArgVal", "ter:NoConfig ");return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __dnrd__Bye(struct soap* soap, struct d__ByeType *dn__Bye, struct d__ResolveType *dn__ByeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);onvif_fault(soap,"ter:InvalidArgVal", "ter:NoConfig ");return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tad__GetServiceCapabilities(struct soap* soap, struct _tad__GetServiceCapabilities *tad__GetServiceCapabilities, struct _tad__GetServiceCapabilitiesResponse *tad__GetServiceCapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tad__DeleteAnalyticsEngineControl(struct soap* soap, struct _tad__DeleteAnalyticsEngineControl *tad__DeleteAnalyticsEngineControl, struct _tad__DeleteAnalyticsEngineControlResponse *tad__DeleteAnalyticsEngineControlResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tad__CreateAnalyticsEngineControl(struct soap* soap, struct _tad__CreateAnalyticsEngineControl *tad__CreateAnalyticsEngineControl, struct _tad__CreateAnalyticsEngineControlResponse *tad__CreateAnalyticsEngineControlResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tad__SetAnalyticsEngineControl(struct soap* soap, struct _tad__SetAnalyticsEngineControl *tad__SetAnalyticsEngineControl, struct _tad__SetAnalyticsEngineControlResponse *tad__SetAnalyticsEngineControlResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tad__GetAnalyticsEngineControl(struct soap* soap, struct _tad__GetAnalyticsEngineControl *tad__GetAnalyticsEngineControl, struct _tad__GetAnalyticsEngineControlResponse *tad__GetAnalyticsEngineControlResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tad__GetAnalyticsEngineControls(struct soap* soap, struct _tad__GetAnalyticsEngineControls *tad__GetAnalyticsEngineControls, struct _tad__GetAnalyticsEngineControlsResponse *tad__GetAnalyticsEngineControlsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tad__GetAnalyticsEngine(struct soap* soap, struct _tad__GetAnalyticsEngine *tad__GetAnalyticsEngine, struct _tad__GetAnalyticsEngineResponse *tad__GetAnalyticsEngineResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tad__GetAnalyticsEngines(struct soap* soap, struct _tad__GetAnalyticsEngines *tad__GetAnalyticsEngines, struct _tad__GetAnalyticsEnginesResponse *tad__GetAnalyticsEnginesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tad__SetVideoAnalyticsConfiguration(struct soap* soap, struct _tad__SetVideoAnalyticsConfiguration *tad__SetVideoAnalyticsConfiguration, struct _tad__SetVideoAnalyticsConfigurationResponse *tad__SetVideoAnalyticsConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tad__SetAnalyticsEngineInput(struct soap* soap, struct _tad__SetAnalyticsEngineInput *tad__SetAnalyticsEngineInput, struct _tad__SetAnalyticsEngineInputResponse *tad__SetAnalyticsEngineInputResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tad__GetAnalyticsEngineInput(struct soap* soap, struct _tad__GetAnalyticsEngineInput *tad__GetAnalyticsEngineInput, struct _tad__GetAnalyticsEngineInputResponse *tad__GetAnalyticsEngineInputResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tad__GetAnalyticsEngineInputs(struct soap* soap, struct _tad__GetAnalyticsEngineInputs *tad__GetAnalyticsEngineInputs, struct _tad__GetAnalyticsEngineInputsResponse *tad__GetAnalyticsEngineInputsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tad__GetAnalyticsDeviceStreamUri(struct soap* soap, struct _tad__GetAnalyticsDeviceStreamUri *tad__GetAnalyticsDeviceStreamUri, struct _tad__GetAnalyticsDeviceStreamUriResponse *tad__GetAnalyticsDeviceStreamUriResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tad__GetVideoAnalyticsConfiguration(struct soap* soap, struct _tad__GetVideoAnalyticsConfiguration *tad__GetVideoAnalyticsConfiguration, struct _tad__GetVideoAnalyticsConfigurationResponse *tad__GetVideoAnalyticsConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tad__CreateAnalyticsEngineInputs(struct soap* soap, struct _tad__CreateAnalyticsEngineInputs *tad__CreateAnalyticsEngineInputs, struct _tad__CreateAnalyticsEngineInputsResponse *tad__CreateAnalyticsEngineInputsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tad__DeleteAnalyticsEngineInputs(struct soap* soap, struct _tad__DeleteAnalyticsEngineInputs *tad__DeleteAnalyticsEngineInputs, struct _tad__DeleteAnalyticsEngineInputsResponse *tad__DeleteAnalyticsEngineInputsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tad__GetAnalyticsState(struct soap* soap, struct _tad__GetAnalyticsState *tad__GetAnalyticsState, struct _tad__GetAnalyticsStateResponse *tad__GetAnalyticsStateResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tanae__GetServiceCapabilities(struct soap* soap, struct _tan__GetServiceCapabilities *tan__GetServiceCapabilities, struct _tan__GetServiceCapabilitiesResponse *tan__GetServiceCapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tanae__GetSupportedAnalyticsModules(struct soap* soap, struct _tan__GetSupportedAnalyticsModules *tan__GetSupportedAnalyticsModules, struct _tan__GetSupportedAnalyticsModulesResponse *tan__GetSupportedAnalyticsModulesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tanae__CreateAnalyticsModules(struct soap* soap, struct _tan__CreateAnalyticsModules *tan__CreateAnalyticsModules, struct _tan__CreateAnalyticsModulesResponse *tan__CreateAnalyticsModulesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tanae__DeleteAnalyticsModules(struct soap* soap, struct _tan__DeleteAnalyticsModules *tan__DeleteAnalyticsModules, struct _tan__DeleteAnalyticsModulesResponse *tan__DeleteAnalyticsModulesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tanae__GetAnalyticsModules(struct soap* soap, struct _tan__GetAnalyticsModules *tan__GetAnalyticsModules, struct _tan__GetAnalyticsModulesResponse *tan__GetAnalyticsModulesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tanae__ModifyAnalyticsModules(struct soap* soap, struct _tan__ModifyAnalyticsModules *tan__ModifyAnalyticsModules, struct _tan__ModifyAnalyticsModulesResponse *tan__ModifyAnalyticsModulesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tanre__GetSupportedRules(struct soap* soap, struct _tan__GetSupportedRules *tan__GetSupportedRules, struct _tan__GetSupportedRulesResponse *tan__GetSupportedRulesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tanre__CreateRules(struct soap* soap, struct _tan__CreateRules *tan__CreateRules, struct _tan__CreateRulesResponse *tan__CreateRulesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tanre__DeleteRules(struct soap* soap, struct _tan__DeleteRules *tan__DeleteRules, struct _tan__DeleteRulesResponse *tan__DeleteRulesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tanre__GetRules(struct soap* soap, struct _tan__GetRules *tan__GetRules, struct _tan__GetRulesResponse *tan__GetRulesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tanre__ModifyRules(struct soap* soap, struct _tan__ModifyRules *tan__ModifyRules, struct _tan__ModifyRulesResponse *tan__ModifyRulesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetServices(struct soap* soap, struct _tds__GetServices *tds__GetServices, struct _tds__GetServicesResponse *tds__GetServicesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetServiceCapabilities(struct soap* soap, struct _tds__GetServiceCapabilities *tds__GetServiceCapabilities, struct _tds__GetServiceCapabilitiesResponse *tds__GetServiceCapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetDeviceInformation(struct soap* soap, struct _tds__GetDeviceInformation *tds__GetDeviceInformation, struct _tds__GetDeviceInformationResponse *tds__GetDeviceInformationResponse)
{
	
	char mac[MACH_ADDR_LENGTH];

	char *_Manufacturer = (char *) malloc(sizeof(char) * LARGE_INFO_LENGTH);
	char *_Model = (char *) malloc(sizeof(char) * LARGE_INFO_LENGTH);
	char *_FirmwareVersion =  (char *) malloc(sizeof(char) * LARGE_INFO_LENGTH);
	char *_SerialNumber = (char *) malloc(sizeof(char) * LARGE_INFO_LENGTH);
	char *_HardwareId = (char *) malloc(sizeof(char) * LARGE_INFO_LENGTH);

	strcpy(_Manufacturer,ONVIF_MANUFACTURER);
	strcpy(_Model,ONVIF_MODEL);
	
	strcpy(_FirmwareVersion,g_pIpcamAll.app_version);
	
	net_get_hwaddr(ETH_NAME, mac);

	sprintf(_SerialNumber,"%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	sprintf(_HardwareId,"1419d68a-1dd2-11b2-a105-%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	tds__GetDeviceInformationResponse->Manufacturer = (char *)soap_malloc(soap, sizeof(char) * LARGE_INFO_LENGTH);
	tds__GetDeviceInformationResponse->Model = (char *)soap_malloc(soap, sizeof(char) * LARGE_INFO_LENGTH);
	tds__GetDeviceInformationResponse->FirmwareVersion = (char *)soap_malloc(soap, sizeof(char) * LARGE_INFO_LENGTH);
	tds__GetDeviceInformationResponse->SerialNumber = (char *)soap_malloc(soap, sizeof(char) * LARGE_INFO_LENGTH);
	tds__GetDeviceInformationResponse->HardwareId = (char *)soap_malloc(soap, sizeof(char) * LARGE_INFO_LENGTH);

	strcpy(tds__GetDeviceInformationResponse->Manufacturer, _Manufacturer);
	strcpy(tds__GetDeviceInformationResponse->Model, _Model);
	strcpy(tds__GetDeviceInformationResponse->FirmwareVersion, _FirmwareVersion);
	strcpy(tds__GetDeviceInformationResponse->SerialNumber, _SerialNumber);
	strcpy(tds__GetDeviceInformationResponse->HardwareId, _HardwareId);

	free(_Manufacturer);
	free(_Model);
	free(_FirmwareVersion);
	free(_SerialNumber);
	free(_HardwareId);	
	
	return SOAP_OK;
}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetSystemDateAndTime(struct soap* soap, struct _tds__SetSystemDateAndTime *tds__SetSystemDateAndTime, struct _tds__SetSystemDateAndTimeResponse *tds__SetSystemDateAndTimeResponse)
{
	char value[INFO_LENGTH];
	int ret;
	int _Year;
	int _Month;
	int _Day;
	int _Hour;
	int _Minute;
	int _Second;
	int _DaylightSavings;
	char *_TZ;

	DPRINTK("in\n");
	//_DateTimeType = tds__SetSystemDateAndTime->DateTimeType; //Manual = 0, NTP = 1
	/* Time Zone */
	if(tds__SetSystemDateAndTime->TimeZone)
	{		
		_TZ  = tds__SetSystemDateAndTime->TimeZone->TZ;
		ret = oset_timezone(_TZ);
		if(ret == 100)
		{
			onvif_fault(soap,"ter:InvalidArgVal","ter:InvalidTimeZone");
			return SOAP_FAULT;
		}		

		DPRINTK("TZ=%s\n",_TZ);
	}

	/* DayLight */
	if(tds__SetSystemDateAndTime->DaylightSavings)
	{
		_DaylightSavings = tds__SetSystemDateAndTime->DaylightSavings;
	}

/*
	if(_DateTimeType == 1) // NTP
	{
		ControlSystemData(SFIELD_GET_SNTP_FQDN, (void *)value, sizeof(value)); //get sntp ip
		run_ntpclient(value);		
		return SOAP_OK;
	}
*/

	if(tds__SetSystemDateAndTime->UTCDateTime)
	{
		_Year = tds__SetSystemDateAndTime->UTCDateTime->Date->Year;
		_Month = tds__SetSystemDateAndTime->UTCDateTime->Date->Month;
		_Day = tds__SetSystemDateAndTime->UTCDateTime->Date->Day;
		_Hour = tds__SetSystemDateAndTime->UTCDateTime->Time->Hour;
		_Minute = tds__SetSystemDateAndTime->UTCDateTime->Time->Minute;
		_Second = tds__SetSystemDateAndTime->UTCDateTime->Time->Second;

		DPRINTK("set time: %0.4d-%0.2d-%0.2d %0.2d:%0.2d:%0.2d\n",
			_Year,_Month,_Day,_Hour,_Minute,_Second);

		if((_Hour>24) || (_Minute>60) || (_Second>60) || (_Month>12) || (_Day>31))
		{		
			onvif_fault(soap,"ter:InvalidArgVal", "ter:InvalidDateTime");
			return SOAP_FAULT;
		}

		if( check_have_sd() == 1 )
		{
			DPRINTK("Ipcam have sd, so can't set time\n");
		}else
		{
			char cmd[256];
			sprintf(cmd,"echo %0.4d-%0.2d-%0.2d %0.2d:%0.2d:%0.2d > /tmp/onvif_set_time\n",
			_Year,_Month,_Day,_Hour,_Minute,_Second);
			system(cmd);

			DPRINTK("write onvif time to /tmp/onvif_set_time\n");			

			/*
			sys_set_date(_Year, _Month, _Day);
			sys_set_time(_Hour, _Minute, _Second);
			*/
		}		
		
	}

	return SOAP_OK;
}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetSystemDateAndTime(struct soap* soap, struct _tds__GetSystemDateAndTime *tds__GetSystemDateAndTime, struct _tds__GetSystemDateAndTimeResponse *tds__GetSystemDateAndTimeResponse)
{
	/* UTC - Coordinated Universal Time */
	time(&current_time);
	time_t tnow;
	time_t tnow_utc;
	struct tm *tmnow;
	struct tm *tmnow_utc;
	int ntp_timezone = 12;

	 get_ipcam_all_info();

	 ntp_timezone = g_pIpcamAll.sys_st.time_zone + 12;

	DPRINTK("in\n");

	tds__GetSystemDateAndTimeResponse->SystemDateAndTime = (struct tt__SystemDateTime*)soap_malloc(soap, sizeof(struct tt__SystemDateTime));
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->DateTimeType = _DateTimeType; 
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->DaylightSavings = _false;
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->Extension = NULL;
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->TimeZone = (struct tt__TimeZone*)soap_malloc(soap, sizeof(struct tt__TimeZone));
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->TimeZone->TZ = oget_timezone(ntp_timezone);//"GMT+05";//"NZST-12NZDT,M10.1.0/2,M3.3.0/3";//timezone;POSIX 1003.1

	time(&tnow);
	tmnow = localtime(&tnow);

	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->LocalDateTime = (struct tt__DateTime*)soap_malloc(soap, sizeof(struct tt__DateTime));
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->LocalDateTime->Time = (struct tt__Time*)soap_malloc(soap, sizeof(struct tt__Time));
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->LocalDateTime->Time->Hour = tmnow->tm_hour;
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->LocalDateTime->Time->Minute = tmnow->tm_min;
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->LocalDateTime->Time->Second = tmnow->tm_sec;
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->LocalDateTime->Date = (struct tt__Date*)soap_malloc(soap, sizeof(struct tt__Date));
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->LocalDateTime->Date->Year =  tmnow->tm_year + 1900;
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->LocalDateTime->Date->Month =  tmnow->tm_mon + 1;
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->LocalDateTime->Date->Day = tmnow->tm_mday;

	tnow_utc = tnow - (ntp_timezone - 12) * 3600;
	tmnow_utc = localtime(&tnow_utc);

	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->UTCDateTime = (struct tt__DateTime*)soap_malloc(soap, sizeof(struct tt__DateTime));
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->UTCDateTime->Time = (struct tt__Time*)soap_malloc(soap, sizeof(struct tt__Time));
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->UTCDateTime->Time->Hour = tmnow_utc->tm_hour;
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->UTCDateTime->Time->Minute = tmnow_utc->tm_min;
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->UTCDateTime->Time->Second = tmnow_utc->tm_sec;
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->UTCDateTime->Date = (struct tt__Date*)soap_malloc(soap, sizeof(struct tt__Date));
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->UTCDateTime->Date->Year = tmnow_utc->tm_year + 1900;
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->UTCDateTime->Date->Month = tmnow_utc->tm_mon + 1;
	tds__GetSystemDateAndTimeResponse->SystemDateAndTime->UTCDateTime->Date->Day = tmnow_utc->tm_mday;
	return SOAP_OK; 
}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetSystemFactoryDefault(struct soap* soap, struct _tds__SetSystemFactoryDefault *tds__SetSystemFactoryDefault, struct _tds__SetSystemFactoryDefaultResponse *tds__SetSystemFactoryDefaultResponse)
{
	TRACE("%s in \n",__FUNCTION__);
	system("rm -rf /mnt/mtd/ipcam_config.data;reboot;");
	TRACE("%s out \n",__FUNCTION__);
	return 0;
}

SOAP_FMAC5 int SOAP_FMAC6 __tds__UpgradeSystemFirmware(struct soap* soap, struct _tds__UpgradeSystemFirmware *tds__UpgradeSystemFirmware, struct _tds__UpgradeSystemFirmwareResponse *tds__UpgradeSystemFirmwareResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SystemReboot(struct soap* soap, struct _tds__SystemReboot *tds__SystemReboot, struct _tds__SystemRebootResponse *tds__SystemRebootResponse)
{
	TRACE("%s in \n",__FUNCTION__);
	system("reboot &");
	TRACE("%s out \n",__FUNCTION__);
	return 0;
}

SOAP_FMAC5 int SOAP_FMAC6 __tds__RestoreSystem(struct soap* soap, struct _tds__RestoreSystem *tds__RestoreSystem, struct _tds__RestoreSystemResponse *tds__RestoreSystemResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetSystemBackup(struct soap* soap, struct _tds__GetSystemBackup *tds__GetSystemBackup, struct _tds__GetSystemBackupResponse *tds__GetSystemBackupResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetSystemLog(struct soap* soap, struct _tds__GetSystemLog *tds__GetSystemLog, struct _tds__GetSystemLogResponse *tds__GetSystemLogResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetSystemSupportInformation(struct soap* soap, struct _tds__GetSystemSupportInformation *tds__GetSystemSupportInformation, struct _tds__GetSystemSupportInformationResponse *tds__GetSystemSupportInformationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetScopes(struct soap* soap, struct _tds__GetScopes *tds__GetScopes, struct _tds__GetScopesResponse *tds__GetScopesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetScopes(struct soap* soap, struct _tds__SetScopes *tds__SetScopes, struct _tds__SetScopesResponse *tds__SetScopesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__AddScopes(struct soap* soap, struct _tds__AddScopes *tds__AddScopes, struct _tds__AddScopesResponse *tds__AddScopesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__RemoveScopes(struct soap* soap, struct _tds__RemoveScopes *tds__RemoveScopes, struct _tds__RemoveScopesResponse *tds__RemoveScopesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetDiscoveryMode(struct soap* soap, struct _tds__GetDiscoveryMode *tds__GetDiscoveryMode, struct _tds__GetDiscoveryModeResponse *tds__GetDiscoveryModeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetDiscoveryMode(struct soap* soap, struct _tds__SetDiscoveryMode *tds__SetDiscoveryMode, struct _tds__SetDiscoveryModeResponse *tds__SetDiscoveryModeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetRemoteDiscoveryMode(struct soap* soap, struct _tds__GetRemoteDiscoveryMode *tds__GetRemoteDiscoveryMode, struct _tds__GetRemoteDiscoveryModeResponse *tds__GetRemoteDiscoveryModeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetRemoteDiscoveryMode(struct soap* soap, struct _tds__SetRemoteDiscoveryMode *tds__SetRemoteDiscoveryMode, struct _tds__SetRemoteDiscoveryModeResponse *tds__SetRemoteDiscoveryModeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetDPAddresses(struct soap* soap, struct _tds__GetDPAddresses *tds__GetDPAddresses, struct _tds__GetDPAddressesResponse *tds__GetDPAddressesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetEndpointReference(struct soap* soap, struct _tds__GetEndpointReference *tds__GetEndpointReference, struct _tds__GetEndpointReferenceResponse *tds__GetEndpointReferenceResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetRemoteUser(struct soap* soap, struct _tds__GetRemoteUser *tds__GetRemoteUser, struct _tds__GetRemoteUserResponse *tds__GetRemoteUserResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetRemoteUser(struct soap* soap, struct _tds__SetRemoteUser *tds__SetRemoteUser, struct _tds__SetRemoteUserResponse *tds__SetRemoteUserResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetUsers(struct soap* soap, struct _tds__GetUsers *tds__GetUsers, struct _tds__GetUsersResponse *tds__GetUsersResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__CreateUsers(struct soap* soap, struct _tds__CreateUsers *tds__CreateUsers, struct _tds__CreateUsersResponse *tds__CreateUsersResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__DeleteUsers(struct soap* soap, struct _tds__DeleteUsers *tds__DeleteUsers, struct _tds__DeleteUsersResponse *tds__DeleteUsersResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetUser(struct soap* soap, struct _tds__SetUser *tds__SetUser, struct _tds__SetUserResponse *tds__SetUserResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetWsdlUrl(struct soap* soap, struct _tds__GetWsdlUrl *tds__GetWsdlUrl, struct _tds__GetWsdlUrlResponse *tds__GetWsdlUrlResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetCapabilities(struct soap* soap, struct _tds__GetCapabilities *tds__GetCapabilities, struct _tds__GetCapabilitiesResponse *tds__GetCapabilitiesResponse)
{
	static int  m_true,m_false;
	NET_IPV4 ip;
	ip.int32 = net_get_ifaddr(ETH_NAME);
	int _Category;
	char _IPv4Address[LARGE_INFO_LENGTH]; 

	m_true = _true;
	m_false = _false;

	if(tds__GetCapabilities->Category == NULL)
	{
		tds__GetCapabilities->Category=(int *)soap_malloc(soap, sizeof(int));
		soap_default_tt__CapabilityCategory(soap,tds__GetCapabilities->Category);
		*tds__GetCapabilities->Category = ALL;
		_Category = ALL;
	}
	else
	{
		_Category = *tds__GetCapabilities->Category;
	}

	DPRINTK("2 _Category=%d\n",_Category);
	sprintf(_IPv4Address, "http://%d.%d.%d.%d:%d/onvif/services", ip.str[0], ip.str[1], ip.str[2], ip.str[3],g_pIpcamAll.onvif_server_port);
	tds__GetCapabilitiesResponse->Capabilities = (struct tt__Capabilities*)soap_malloc(soap, sizeof(struct tt__Capabilities));
	soap_default_tt__Capabilities(soap,tds__GetCapabilitiesResponse->Capabilities);
	tds__GetCapabilitiesResponse->Capabilities->Analytics = NULL;
	tds__GetCapabilitiesResponse->Capabilities->Device = NULL;
	tds__GetCapabilitiesResponse->Capabilities->Events = NULL;
	tds__GetCapabilitiesResponse->Capabilities->Imaging = NULL;
	tds__GetCapabilitiesResponse->Capabilities->Media = NULL;
	tds__GetCapabilitiesResponse->Capabilities->PTZ = NULL;
	tds__GetCapabilitiesResponse->Capabilities->Extension = (struct tt__CapabilitiesExtension*)soap_malloc(soap, sizeof(struct tt__CapabilitiesExtension));
	soap_default_tt__CapabilitiesExtension(soap,tds__GetCapabilitiesResponse->Capabilities->Extension);
	tds__GetCapabilitiesResponse->Capabilities->Extension->DeviceIO = (struct tt__DeviceIOCapabilities*)soap_malloc(soap, sizeof(struct tt__DeviceIOCapabilities));
	soap_default_tt__DeviceIOCapabilities(soap,tds__GetCapabilitiesResponse->Capabilities->Extension->DeviceIO);
	tds__GetCapabilitiesResponse->Capabilities->Extension->DeviceIO->XAddr = (char *)soap_malloc(soap, sizeof(char) * INFO_LENGTH);
	strcpy(tds__GetCapabilitiesResponse->Capabilities->Extension->DeviceIO->XAddr,_IPv4Address);
	tds__GetCapabilitiesResponse->Capabilities->Extension->DeviceIO->VideoSources = TRUE;
	tds__GetCapabilitiesResponse->Capabilities->Extension->DeviceIO->VideoOutputs = TRUE;
	tds__GetCapabilitiesResponse->Capabilities->Extension->DeviceIO->AudioSources = TRUE;
	tds__GetCapabilitiesResponse->Capabilities->Extension->DeviceIO->AudioOutputs = TRUE;
	tds__GetCapabilitiesResponse->Capabilities->Extension->DeviceIO->RelayOutputs = TRUE;
	tds__GetCapabilitiesResponse->Capabilities->Extension->Display = NULL;
	tds__GetCapabilitiesResponse->Capabilities->Extension->Recording = NULL;
	tds__GetCapabilitiesResponse->Capabilities->Extension->Search = NULL;
	tds__GetCapabilitiesResponse->Capabilities->Extension->Replay = NULL;
	tds__GetCapabilitiesResponse->Capabilities->Extension->Receiver = NULL;
	tds__GetCapabilitiesResponse->Capabilities->Extension->AnalyticsDevice = NULL;
	tds__GetCapabilitiesResponse->Capabilities->Extension->Extensions = NULL;
	

	if((_Category == ALL) || (_Category == ANALYTICS))
	{		
		tds__GetCapabilitiesResponse->Capabilities->Analytics = (struct tt__AnalyticsCapabilities*)soap_malloc(soap, sizeof(struct tt__AnalyticsCapabilities));
		soap_default_tt__AnalyticsCapabilities(soap,tds__GetCapabilitiesResponse->Capabilities->Analytics);
		tds__GetCapabilitiesResponse->Capabilities->Analytics->XAddr = (char *) soap_malloc(soap, sizeof(char) * LARGE_INFO_LENGTH);
		strcpy(tds__GetCapabilitiesResponse->Capabilities->Analytics->XAddr, _IPv4Address);
		tds__GetCapabilitiesResponse->Capabilities->Analytics->RuleSupport = FALSE;	
		tds__GetCapabilitiesResponse->Capabilities->Analytics->AnalyticsModuleSupport = FALSE;
	}
	if((_Category == ALL) || (_Category == DEVICE))
	{		
		tds__GetCapabilitiesResponse->Capabilities->Device = (struct tt__DeviceCapabilities*)soap_malloc(soap, sizeof(struct tt__DeviceCapabilities));
		soap_default_tt__DeviceCapabilities(soap,tds__GetCapabilitiesResponse->Capabilities->Device );
		tds__GetCapabilitiesResponse->Capabilities->Device->XAddr = (char *) soap_malloc(soap, sizeof(char) * LARGE_INFO_LENGTH);
		strcpy(tds__GetCapabilitiesResponse->Capabilities->Device->XAddr, _IPv4Address);	
		tds__GetCapabilitiesResponse->Capabilities->Device->Extension = NULL;

		tds__GetCapabilitiesResponse->Capabilities->Device->Network = (struct tt__NetworkCapabilities*)soap_malloc(soap, sizeof(struct tt__NetworkCapabilities));
		soap_default_tt__NetworkCapabilities(soap,tds__GetCapabilitiesResponse->Capabilities->Device->Network);
		tds__GetCapabilitiesResponse->Capabilities->Device->Network->IPFilter = (int *)soap_malloc(soap, sizeof(int));
		tds__GetCapabilitiesResponse->Capabilities->Device->Network->ZeroConfiguration = (int *)soap_malloc(soap, sizeof(int));
		tds__GetCapabilitiesResponse->Capabilities->Device->Network->IPVersion6 = (int *)soap_malloc(soap, sizeof(int));
		tds__GetCapabilitiesResponse->Capabilities->Device->Network->DynDNS = (int *)soap_malloc(soap, sizeof(int));
		*tds__GetCapabilitiesResponse->Capabilities->Device->Network->IPFilter = _false;	
		*tds__GetCapabilitiesResponse->Capabilities->Device->Network->ZeroConfiguration = _false;
		*tds__GetCapabilitiesResponse->Capabilities->Device->Network->IPVersion6 = _false;
		*tds__GetCapabilitiesResponse->Capabilities->Device->Network->DynDNS = _false;	
		tds__GetCapabilitiesResponse->Capabilities->Device->Network->Extension = (struct tt__NetworkCapabilitiesExtension*)soap_malloc(soap, sizeof(struct tt__NetworkCapabilitiesExtension));
		soap_default_tt__NetworkCapabilitiesExtension(soap,tds__GetCapabilitiesResponse->Capabilities->Device->Network->Extension);
		tds__GetCapabilitiesResponse->Capabilities->Device->Network->Extension->Dot11Configuration = (int *)soap_malloc(soap, sizeof(int)); 
		*tds__GetCapabilitiesResponse->Capabilities->Device->Network->Extension->Dot11Configuration = _false; 
		tds__GetCapabilitiesResponse->Capabilities->Device->Network->Extension->Extension = NULL;

		tds__GetCapabilitiesResponse->Capabilities->Device->System = (struct tt__SystemCapabilities*)soap_malloc(soap, sizeof(struct tt__SystemCapabilities));	
		soap_default_tt__SystemCapabilities(soap,tds__GetCapabilitiesResponse->Capabilities->Device->System);
		tds__GetCapabilitiesResponse->Capabilities->Device->System->DiscoveryResolve = FALSE;
		tds__GetCapabilitiesResponse->Capabilities->Device->System->DiscoveryBye = FALSE;
		tds__GetCapabilitiesResponse->Capabilities->Device->System->RemoteDiscovery = FALSE;
		tds__GetCapabilitiesResponse->Capabilities->Device->System->SystemBackup = FALSE;
		tds__GetCapabilitiesResponse->Capabilities->Device->System->SystemLogging = TRUE;
		tds__GetCapabilitiesResponse->Capabilities->Device->System->FirmwareUpgrade = TRUE;	
		tds__GetCapabilitiesResponse->Capabilities->Device->System->__sizeSupportedVersions = TRUE;
		tds__GetCapabilitiesResponse->Capabilities->Device->System->SupportedVersions = (struct tt__OnvifVersion*)soap_malloc(soap, sizeof(struct tt__OnvifVersion));
		tds__GetCapabilitiesResponse->Capabilities->Device->System->SupportedVersions->Major = MAJOR;
		tds__GetCapabilitiesResponse->Capabilities->Device->System->SupportedVersions->Minor = MINOR;
		tds__GetCapabilitiesResponse->Capabilities->Device->System->Extension = (struct tt__SystemCapabilitiesExtension*)soap_malloc(soap, sizeof(struct tt__SystemCapabilitiesExtension));
		soap_default_tt__SystemCapabilitiesExtension(soap,tds__GetCapabilitiesResponse->Capabilities->Device->System->Extension);
		tds__GetCapabilitiesResponse->Capabilities->Device->System->Extension->HttpSystemBackup = (int *)soap_malloc(soap, sizeof(int));
		*tds__GetCapabilitiesResponse->Capabilities->Device->System->Extension->HttpSystemBackup = _false;
		tds__GetCapabilitiesResponse->Capabilities->Device->System->Extension->HttpFirmwareUpgrade = (int *)soap_malloc(soap, sizeof(int));
		*tds__GetCapabilitiesResponse->Capabilities->Device->System->Extension->HttpFirmwareUpgrade = _true;
		tds__GetCapabilitiesResponse->Capabilities->Device->System->Extension->HttpSystemLogging = (int *)soap_malloc(soap, sizeof(int));
		*tds__GetCapabilitiesResponse->Capabilities->Device->System->Extension->HttpSystemLogging = _true;
		tds__GetCapabilitiesResponse->Capabilities->Device->System->Extension->HttpSupportInformation = (int *)soap_malloc(soap, sizeof(int));
		*tds__GetCapabilitiesResponse->Capabilities->Device->System->Extension->HttpSupportInformation = _true;
		tds__GetCapabilitiesResponse->Capabilities->Device->System->Extension->Extension = NULL;

		tds__GetCapabilitiesResponse->Capabilities->Device->IO = (struct tt__IOCapabilities*)soap_malloc(soap, sizeof(struct tt__IOCapabilities));
		soap_default_tt__IOCapabilities(soap,tds__GetCapabilitiesResponse->Capabilities->Device->IO);
		tds__GetCapabilitiesResponse->Capabilities->Device->IO->InputConnectors = &m_false;
		tds__GetCapabilitiesResponse->Capabilities->Device->IO->RelayOutputs = &m_true;
		tds__GetCapabilitiesResponse->Capabilities->Device->IO->Extension = NULL;

		tds__GetCapabilitiesResponse->Capabilities->Device->Security = (struct tt__SecurityCapabilities*)soap_malloc(soap, sizeof(struct tt__SecurityCapabilities));
		soap_default_tt__SecurityCapabilities(soap,tds__GetCapabilitiesResponse->Capabilities->Device->Security);
		tds__GetCapabilitiesResponse->Capabilities->Device->Security->TLS1_x002e1 = FALSE;	
		tds__GetCapabilitiesResponse->Capabilities->Device->Security->TLS1_x002e2 = FALSE;	
		tds__GetCapabilitiesResponse->Capabilities->Device->Security->OnboardKeyGeneration = FALSE;
		tds__GetCapabilitiesResponse->Capabilities->Device->Security->AccessPolicyConfig = FALSE;
		tds__GetCapabilitiesResponse->Capabilities->Device->Security->X_x002e509Token = FALSE;
		tds__GetCapabilitiesResponse->Capabilities->Device->Security->SAMLToken = FALSE;	
		tds__GetCapabilitiesResponse->Capabilities->Device->Security->KerberosToken = FALSE;
		tds__GetCapabilitiesResponse->Capabilities->Device->Security->RELToken = FALSE;	
		tds__GetCapabilitiesResponse->Capabilities->Device->Security->Extension = NULL;
	
	}
	if((_Category == ALL) || (_Category == EVENTS))
	{			
		tds__GetCapabilitiesResponse->Capabilities->Events = (struct tt__EventCapabilities*)soap_malloc(soap, sizeof(struct tt__EventCapabilities));
		soap_default_tt__EventCapabilities(soap,tds__GetCapabilitiesResponse->Capabilities->Events);
		tds__GetCapabilitiesResponse->Capabilities->Events->XAddr = (char *) soap_malloc(soap, sizeof(char) * LARGE_INFO_LENGTH);
		strcpy(tds__GetCapabilitiesResponse->Capabilities->Events->XAddr, _IPv4Address);
		tds__GetCapabilitiesResponse->Capabilities->Events->WSSubscriptionPolicySupport = FALSE;	
		tds__GetCapabilitiesResponse->Capabilities->Events->WSPullPointSupport = FALSE;	
		tds__GetCapabilitiesResponse->Capabilities->Events->WSPausableSubscriptionManagerInterfaceSupport = FALSE;
	}
	if((_Category == ALL) || (_Category == IMAGING))
	{			
		tds__GetCapabilitiesResponse->Capabilities->Imaging = (struct tt__ImagingCapabilities*)soap_malloc(soap, sizeof(struct tt__ImagingCapabilities));
		soap_default_tt__ImagingCapabilities(soap,tds__GetCapabilitiesResponse->Capabilities->Imaging);
		tds__GetCapabilitiesResponse->Capabilities->Imaging->XAddr = (char *) soap_malloc(soap, sizeof(char) * LARGE_INFO_LENGTH);
		strcpy(tds__GetCapabilitiesResponse->Capabilities->Imaging->XAddr, _IPv4Address);
	}

	if((_Category == ALL) || (_Category == PANTILTZOOM))
	{		
		tds__GetCapabilitiesResponse->Capabilities->PTZ = (struct tt__PTZCapabilities*)soap_malloc(soap, sizeof(struct tt__PTZCapabilities));
		soap_default_tt__PTZCapabilities(soap,tds__GetCapabilitiesResponse->Capabilities->PTZ);
		tds__GetCapabilitiesResponse->Capabilities->PTZ->XAddr = (char *) soap_malloc(soap, sizeof(char) * LARGE_INFO_LENGTH);
		strcpy(tds__GetCapabilitiesResponse->Capabilities->PTZ->XAddr, _IPv4Address);
	}

	
	if((_Category == ALL) || (_Category == MEDIA))
	{		
		
		tds__GetCapabilitiesResponse->Capabilities->Media = (struct tt__MediaCapabilities*)soap_malloc(soap, sizeof(struct tt__MediaCapabilities));
		soap_default_tt__MediaCapabilities(soap,tds__GetCapabilitiesResponse->Capabilities->Media);
		tds__GetCapabilitiesResponse->Capabilities->Media->XAddr = (char *) soap_malloc(soap, sizeof(char) * LARGE_INFO_LENGTH);
		strcpy(tds__GetCapabilitiesResponse->Capabilities->Media->XAddr, _IPv4Address);
		tds__GetCapabilitiesResponse->Capabilities->Media->StreamingCapabilities = (struct tt__RealTimeStreamingCapabilities*)soap_malloc(soap, sizeof(struct tt__RealTimeStreamingCapabilities));
		soap_default_tt__RealTimeStreamingCapabilities(soap,tds__GetCapabilitiesResponse->Capabilities->Media->StreamingCapabilities);
		tds__GetCapabilitiesResponse->Capabilities->Media->StreamingCapabilities->RTPMulticast = (int *)soap_malloc(soap, sizeof(int));	
		*tds__GetCapabilitiesResponse->Capabilities->Media->StreamingCapabilities->RTPMulticast = _false;	
		tds__GetCapabilitiesResponse->Capabilities->Media->StreamingCapabilities->RTP_USCORETCP = (int *)soap_malloc(soap, sizeof(int));
		*tds__GetCapabilitiesResponse->Capabilities->Media->StreamingCapabilities->RTP_USCORETCP = _true;	
		tds__GetCapabilitiesResponse->Capabilities->Media->StreamingCapabilities->RTP_USCORERTSP_USCORETCP = (int *)soap_malloc(soap, sizeof(int));
		*tds__GetCapabilitiesResponse->Capabilities->Media->StreamingCapabilities->RTP_USCORERTSP_USCORETCP = _true;	
		tds__GetCapabilitiesResponse->Capabilities->Media->StreamingCapabilities->Extension = NULL;
		tds__GetCapabilitiesResponse->Capabilities->Media->Extension = NULL;
	}
	
	return SOAP_OK;
}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetDPAddresses(struct soap* soap, struct _tds__SetDPAddresses *tds__SetDPAddresses, struct _tds__SetDPAddressesResponse *tds__SetDPAddressesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetHostname(struct soap* soap, struct _tds__GetHostname *tds__GetHostname, struct _tds__GetHostnameResponse *tds__GetHostnameResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetHostname(struct soap* soap, struct _tds__SetHostname *tds__SetHostname, struct _tds__SetHostnameResponse *tds__SetHostnameResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetHostnameFromDHCP(struct soap* soap, struct _tds__SetHostnameFromDHCP *tds__SetHostnameFromDHCP, struct _tds__SetHostnameFromDHCPResponse *tds__SetHostnameFromDHCPResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetDNS(struct soap* soap, struct _tds__GetDNS *tds__GetDNS, struct _tds__GetDNSResponse *tds__GetDNSResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetDNS(struct soap* soap, struct _tds__SetDNS *tds__SetDNS, struct _tds__SetDNSResponse *tds__SetDNSResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetNTP(struct soap* soap, struct _tds__GetNTP *tds__GetNTP, struct _tds__GetNTPResponse *tds__GetNTPResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetNTP(struct soap* soap, struct _tds__SetNTP *tds__SetNTP, struct _tds__SetNTPResponse *tds__SetNTPResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetDynamicDNS(struct soap* soap, struct _tds__GetDynamicDNS *tds__GetDynamicDNS, struct _tds__GetDynamicDNSResponse *tds__GetDynamicDNSResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetDynamicDNS(struct soap* soap, struct _tds__SetDynamicDNS *tds__SetDynamicDNS, struct _tds__SetDynamicDNSResponse *tds__SetDynamicDNSResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetNetworkInterfaces(struct soap* soap, struct _tds__GetNetworkInterfaces *tds__GetNetworkInterfaces, struct _tds__GetNetworkInterfacesResponse *tds__GetNetworkInterfacesResponse)
{	
	static char dev_name[40] = "eth0";
	char mac[MACH_ADDR_LENGTH];
	char _mac[LARGE_INFO_LENGTH];
	char _IPAddr[LARGE_INFO_LENGTH];
	char _IPAddr_local[LARGE_INFO_LENGTH];
	NET_IPV4 ip;
	NET_IPV4 ip_local;

	TRACE("%s in \n",__FUNCTION__);

	if( have_wireless_dev() == 1 )
		strcpy(dev_name,"ra0");	

	printf("dev_name=%s\n",dev_name);

	ip.int32 = net_get_ifaddr(dev_name);
	ip_local.int32 = net_get_ifaddr(dev_name);
	net_get_hwaddr(dev_name, mac);
	sprintf(_mac, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	sprintf(_IPAddr, "%d.%d.%d.%d", ip.str[0], ip.str[1], ip.str[2], ip.str[3]); 
	sprintf(_IPAddr_local, "%d.%d.%d.%d", ip_local.str[0], ip_local.str[1], ip_local.str[2], ip_local.str[3]); 
	printf("_mac=%s\n",_mac);
	printf("_IPAddr=%s\n",_IPAddr);
	printf("_IPAddr_local=%s\n",_IPAddr_local);
	tds__GetNetworkInterfacesResponse->__sizeNetworkInterfaces = 1;
	tds__GetNetworkInterfacesResponse->NetworkInterfaces = (struct tt__NetworkInterface *)soap_malloc(soap, sizeof(struct tt__NetworkInterface));
	soap_default_tt__NetworkInterface(soap,tds__GetNetworkInterfacesResponse->NetworkInterfaces);
	tds__GetNetworkInterfacesResponse->NetworkInterfaces->token = dev_name;
	tds__GetNetworkInterfacesResponse->NetworkInterfaces->Enabled = TRUE;

	tds__GetNetworkInterfacesResponse->NetworkInterfaces->Info = (struct tt__NetworkInterfaceInfo *)soap_malloc(soap, sizeof(struct tt__NetworkInterfaceInfo));
	soap_default_tt__NetworkInterfaceInfo(soap,tds__GetNetworkInterfacesResponse->NetworkInterfaces->Info );
	tds__GetNetworkInterfacesResponse->NetworkInterfaces->Info->Name = dev_name;
	tds__GetNetworkInterfacesResponse->NetworkInterfaces->Info->HwAddress = (char *)soap_malloc(soap, LARGE_INFO_LENGTH);
	strcpy(tds__GetNetworkInterfacesResponse->NetworkInterfaces->Info->HwAddress,_mac);
	tds__GetNetworkInterfacesResponse->NetworkInterfaces->Info->MTU = (int *)soap_malloc(soap, sizeof(int));
	tds__GetNetworkInterfacesResponse->NetworkInterfaces->Info->MTU[0] = 1500;

	tds__GetNetworkInterfacesResponse->NetworkInterfaces->Link = NULL;
	tds__GetNetworkInterfacesResponse->NetworkInterfaces->IPv4 = (struct tt__IPv4NetworkInterface *)soap_malloc(soap, sizeof(struct tt__IPv4NetworkInterface));
	soap_default_tt__IPv4NetworkInterface(soap,tds__GetNetworkInterfacesResponse->NetworkInterfaces->IPv4);
	tds__GetNetworkInterfacesResponse->NetworkInterfaces->IPv4->Enabled = TRUE;
	tds__GetNetworkInterfacesResponse->NetworkInterfaces->IPv4->Config = (struct tt__IPv4Configuration *)soap_malloc(soap, sizeof(struct tt__IPv4Configuration));
	soap_default_tt__IPv4Configuration(soap,tds__GetNetworkInterfacesResponse->NetworkInterfaces->IPv4->Config);
	tds__GetNetworkInterfacesResponse->NetworkInterfaces->IPv4->Config->__sizeManual = 1;
	tds__GetNetworkInterfacesResponse->NetworkInterfaces->IPv4->Config->Manual = (struct tt__PrefixedIPv4Address *)soap_malloc(soap, sizeof(struct tt__PrefixedIPv4Address));
	soap_default_tt__PrefixedIPv4Address(soap,tds__GetNetworkInterfacesResponse->NetworkInterfaces->IPv4->Config->Manual);
	tds__GetNetworkInterfacesResponse->NetworkInterfaces->IPv4->Config->Manual->Address = (char *)soap_malloc(soap, sizeof(char) * MID_INFO_LENGTH);
	strcpy(tds__GetNetworkInterfacesResponse->NetworkInterfaces->IPv4->Config->Manual->Address,_IPAddr);

	tds__GetNetworkInterfacesResponse->NetworkInterfaces->IPv4->Config->LinkLocal = (struct tt__PrefixedIPv4Address *)soap_malloc(soap, sizeof(struct tt__PrefixedIPv4Address));
	soap_default_tt__PrefixedIPv4Address(soap,tds__GetNetworkInterfacesResponse->NetworkInterfaces->IPv4->Config->LinkLocal);
	tds__GetNetworkInterfacesResponse->NetworkInterfaces->IPv4->Config->LinkLocal->Address = _IPAddr_local;
	tds__GetNetworkInterfacesResponse->NetworkInterfaces->IPv4->Config->FromDHCP = (struct tt__PrefixedIPv4Address *)soap_malloc(soap, sizeof(struct tt__PrefixedIPv4Address));
	soap_default_tt__PrefixedIPv4Address(soap,tds__GetNetworkInterfacesResponse->NetworkInterfaces->IPv4->Config->FromDHCP);
	tds__GetNetworkInterfacesResponse->NetworkInterfaces->IPv4->Config->FromDHCP->Address = (char *)soap_malloc(soap, LARGE_INFO_LENGTH);
	strcpy(tds__GetNetworkInterfacesResponse->NetworkInterfaces->IPv4->Config->FromDHCP->Address, _IPAddr); 
	tds__GetNetworkInterfacesResponse->NetworkInterfaces->IPv4->Config->FromDHCP->PrefixLength = 24;
	tds__GetNetworkInterfacesResponse->NetworkInterfaces->IPv4->Config->DHCP = FALSE;
	tds__GetNetworkInterfacesResponse->NetworkInterfaces->IPv6 = NULL;
	tds__GetNetworkInterfacesResponse->NetworkInterfaces->Extension = NULL;	

	TRACE("%s out \n",__FUNCTION__);
	return SOAP_OK;	
}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetNetworkInterfaces(struct soap* soap, struct _tds__SetNetworkInterfaces *tds__SetNetworkInterfaces, struct _tds__SetNetworkInterfacesResponse *tds__SetNetworkInterfacesResponse)
{
	struct in_addr ipaddr, sys_ip;
	int value;
	int ret;	
	int local_dhcp_status = 0; 
	static char dev_name[40] = "eth0";

	TRACE("%s in \n",__FUNCTION__);

	DPRINTK("1\n");

	if( have_wireless_dev() == 1 )
		strcpy(dev_name,"ra0");

	if(tds__SetNetworkInterfaces->InterfaceToken != NULL)
	{
		if(strcmp(tds__SetNetworkInterfaces->InterfaceToken, dev_name))
		{
			onvif_fault(soap,"ter:InvalidArgVal", "ter:InvalidNetworkInterface");
			return SOAP_FAULT;
		}
	}

	DPRINTK("2\n");

	
	if(tds__SetNetworkInterfaces->NetworkInterface->MTU != NULL)
	{
		//onvif_fault(soap,"ter:InvalidArgVal", "ter:SettingMTUNotSupported");
		//return SOAP_FAULT;
	}

	DPRINTK("3\n");
	
	if(tds__SetNetworkInterfaces->NetworkInterface->Link != NULL)
	{
		//onvif_fault(soap,"ter:InvalidArgVal", "ter:SettingLinkValuesNotSupported");
		//return SOAP_FAULT;
	}

	DPRINTK("4\n");
	
	if(tds__SetNetworkInterfaces->NetworkInterface->IPv6 != NULL)
	{
		if(tds__SetNetworkInterfaces->NetworkInterface->IPv6->Enabled == 1)
		{	
			//onvif_fault(soap,"ter:InvalidArgVal", "ter:IPv6NotSupported");
			//return SOAP_FAULT;
		}
	}	

	DPRINTK("5\n");
	
	if(tds__SetNetworkInterfaces->NetworkInterface->IPv4 == NULL)
	{
		onvif_fault(soap,"ter:InvalidArgVal", "ter:IPv4ValuesMissing");
		return SOAP_FAULT;
	}

	DPRINTK("6\n");
	
	if(tds__SetNetworkInterfaces->NetworkInterface->IPv4->Enabled != NULL)
	{
		if(tds__SetNetworkInterfaces->NetworkInterface->IPv4->Manual != NULL)
		{
			if(tds__SetNetworkInterfaces->NetworkInterface->IPv4->Manual->Address != NULL)
			{
				if(isValidIp4(tds__SetNetworkInterfaces->NetworkInterface->IPv4->Manual->Address) == 0) // Check IP address
				{
					onvif_fault(soap, "ter:InvalidArgVal", "ter:InvalidIPv4Address");
					return SOAP_FAULT;
				}
				if (ipv4_str_to_num(tds__SetNetworkInterfaces->NetworkInterface->IPv4->Manual->Address, &ipaddr) == 0)
				{
					return SOAP_FAULT;
				}
			}
			if(tds__SetNetworkInterfaces->InterfaceToken != NULL)
			{
				if ((sys_ip.s_addr = net_get_ifaddr(tds__SetNetworkInterfaces->InterfaceToken)) == -1)
				{
					return SOAP_FAULT;
				}
				if (sys_ip.s_addr != ipaddr.s_addr)
				{							
					 NET_IPV4 * tmp_ipaddr = (NET_IPV4 *)&ipaddr; 
					 get_ipcam_all_info();

					 g_pIpcamAll.net_st.ipv4[0] = tmp_ipaddr->str[0];
					 g_pIpcamAll.net_st.ipv4[1] = tmp_ipaddr->str[1];
					 g_pIpcamAll.net_st.ipv4[2] = tmp_ipaddr->str[2];
					 g_pIpcamAll.net_st.ipv4[3] = tmp_ipaddr->str[3];

					 IpcamSetValue(&g_pIpcamAll,IPCAM_NET_SET_PARAMETERS);

					 /*
					if (net_set_ifaddr(tds__SetNetworkInterfaces->InterfaceToken, ipaddr.s_addr) < 0)
					{
						return SOAP_FAULT;
					}*/
					
				}
			}
		}
		else
		{
			if(tds__SetNetworkInterfaces->NetworkInterface->IPv4->DHCP != NULL)
			{
				if(*(tds__SetNetworkInterfaces->NetworkInterface->IPv4->DHCP))
				{
					if(local_dhcp_status != 1)
					{
						net_enable_dhcpcd();
					}
					value = 1;
				}
				else
				{
					if(local_dhcp_status != 0)
					{
						net_disable_dhcpcd();
					}
					value = 0;
				}
			}
			if(local_dhcp_status != value)
			{
				//ControlSystemData(SFIELD_SET_DHCPC_ENABLE, (void *)&value, sizeof(value));
			}
		}
	}

	DPRINTK("7\n");
	
	tds__SetNetworkInterfacesResponse->RebootNeeded = FALSE;
	TRACE("%s out \n",__FUNCTION__);
	return SOAP_OK;
}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetNetworkProtocols(struct soap* soap, struct _tds__GetNetworkProtocols *tds__GetNetworkProtocols, struct _tds__GetNetworkProtocolsResponse *tds__GetNetworkProtocolsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetNetworkProtocols(struct soap* soap, struct _tds__SetNetworkProtocols *tds__SetNetworkProtocols, struct _tds__SetNetworkProtocolsResponse *tds__SetNetworkProtocolsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetNetworkDefaultGateway(struct soap* soap, struct _tds__GetNetworkDefaultGateway *tds__GetNetworkDefaultGateway, struct _tds__GetNetworkDefaultGatewayResponse *tds__GetNetworkDefaultGatewayResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetNetworkDefaultGateway(struct soap* soap, struct _tds__SetNetworkDefaultGateway *tds__SetNetworkDefaultGateway, struct _tds__SetNetworkDefaultGatewayResponse *tds__SetNetworkDefaultGatewayResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetZeroConfiguration(struct soap* soap, struct _tds__GetZeroConfiguration *tds__GetZeroConfiguration, struct _tds__GetZeroConfigurationResponse *tds__GetZeroConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetZeroConfiguration(struct soap* soap, struct _tds__SetZeroConfiguration *tds__SetZeroConfiguration, struct _tds__SetZeroConfigurationResponse *tds__SetZeroConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetIPAddressFilter(struct soap* soap, struct _tds__GetIPAddressFilter *tds__GetIPAddressFilter, struct _tds__GetIPAddressFilterResponse *tds__GetIPAddressFilterResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetIPAddressFilter(struct soap* soap, struct _tds__SetIPAddressFilter *tds__SetIPAddressFilter, struct _tds__SetIPAddressFilterResponse *tds__SetIPAddressFilterResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__AddIPAddressFilter(struct soap* soap, struct _tds__AddIPAddressFilter *tds__AddIPAddressFilter, struct _tds__AddIPAddressFilterResponse *tds__AddIPAddressFilterResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__RemoveIPAddressFilter(struct soap* soap, struct _tds__RemoveIPAddressFilter *tds__RemoveIPAddressFilter, struct _tds__RemoveIPAddressFilterResponse *tds__RemoveIPAddressFilterResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetAccessPolicy(struct soap* soap, struct _tds__GetAccessPolicy *tds__GetAccessPolicy, struct _tds__GetAccessPolicyResponse *tds__GetAccessPolicyResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetAccessPolicy(struct soap* soap, struct _tds__SetAccessPolicy *tds__SetAccessPolicy, struct _tds__SetAccessPolicyResponse *tds__SetAccessPolicyResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__CreateCertificate(struct soap* soap, struct _tds__CreateCertificate *tds__CreateCertificate, struct _tds__CreateCertificateResponse *tds__CreateCertificateResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetCertificates(struct soap* soap, struct _tds__GetCertificates *tds__GetCertificates, struct _tds__GetCertificatesResponse *tds__GetCertificatesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetCertificatesStatus(struct soap* soap, struct _tds__GetCertificatesStatus *tds__GetCertificatesStatus, struct _tds__GetCertificatesStatusResponse *tds__GetCertificatesStatusResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetCertificatesStatus(struct soap* soap, struct _tds__SetCertificatesStatus *tds__SetCertificatesStatus, struct _tds__SetCertificatesStatusResponse *tds__SetCertificatesStatusResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__DeleteCertificates(struct soap* soap, struct _tds__DeleteCertificates *tds__DeleteCertificates, struct _tds__DeleteCertificatesResponse *tds__DeleteCertificatesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetPkcs10Request(struct soap* soap, struct _tds__GetPkcs10Request *tds__GetPkcs10Request, struct _tds__GetPkcs10RequestResponse *tds__GetPkcs10RequestResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__LoadCertificates(struct soap* soap, struct _tds__LoadCertificates *tds__LoadCertificates, struct _tds__LoadCertificatesResponse *tds__LoadCertificatesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetClientCertificateMode(struct soap* soap, struct _tds__GetClientCertificateMode *tds__GetClientCertificateMode, struct _tds__GetClientCertificateModeResponse *tds__GetClientCertificateModeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetClientCertificateMode(struct soap* soap, struct _tds__SetClientCertificateMode *tds__SetClientCertificateMode, struct _tds__SetClientCertificateModeResponse *tds__SetClientCertificateModeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetRelayOutputs(struct soap* soap, struct _tds__GetRelayOutputs *tds__GetRelayOutputs, struct _tds__GetRelayOutputsResponse *tds__GetRelayOutputsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetRelayOutputSettings(struct soap* soap, struct _tds__SetRelayOutputSettings *tds__SetRelayOutputSettings, struct _tds__SetRelayOutputSettingsResponse *tds__SetRelayOutputSettingsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetRelayOutputState(struct soap* soap, struct _tds__SetRelayOutputState *tds__SetRelayOutputState, struct _tds__SetRelayOutputStateResponse *tds__SetRelayOutputStateResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SendAuxiliaryCommand(struct soap* soap, struct _tds__SendAuxiliaryCommand *tds__SendAuxiliaryCommand, struct _tds__SendAuxiliaryCommandResponse *tds__SendAuxiliaryCommandResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetCACertificates(struct soap* soap, struct _tds__GetCACertificates *tds__GetCACertificates, struct _tds__GetCACertificatesResponse *tds__GetCACertificatesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__LoadCertificateWithPrivateKey(struct soap* soap, struct _tds__LoadCertificateWithPrivateKey *tds__LoadCertificateWithPrivateKey, struct _tds__LoadCertificateWithPrivateKeyResponse *tds__LoadCertificateWithPrivateKeyResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetCertificateInformation(struct soap* soap, struct _tds__GetCertificateInformation *tds__GetCertificateInformation, struct _tds__GetCertificateInformationResponse *tds__GetCertificateInformationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__LoadCACertificates(struct soap* soap, struct _tds__LoadCACertificates *tds__LoadCACertificates, struct _tds__LoadCACertificatesResponse *tds__LoadCACertificatesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__CreateDot1XConfiguration(struct soap* soap, struct _tds__CreateDot1XConfiguration *tds__CreateDot1XConfiguration, struct _tds__CreateDot1XConfigurationResponse *tds__CreateDot1XConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__SetDot1XConfiguration(struct soap* soap, struct _tds__SetDot1XConfiguration *tds__SetDot1XConfiguration, struct _tds__SetDot1XConfigurationResponse *tds__SetDot1XConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetDot1XConfiguration(struct soap* soap, struct _tds__GetDot1XConfiguration *tds__GetDot1XConfiguration, struct _tds__GetDot1XConfigurationResponse *tds__GetDot1XConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetDot1XConfigurations(struct soap* soap, struct _tds__GetDot1XConfigurations *tds__GetDot1XConfigurations, struct _tds__GetDot1XConfigurationsResponse *tds__GetDot1XConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__DeleteDot1XConfiguration(struct soap* soap, struct _tds__DeleteDot1XConfiguration *tds__DeleteDot1XConfiguration, struct _tds__DeleteDot1XConfigurationResponse *tds__DeleteDot1XConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetDot11Capabilities(struct soap* soap, struct _tds__GetDot11Capabilities *tds__GetDot11Capabilities, struct _tds__GetDot11CapabilitiesResponse *tds__GetDot11CapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetDot11Status(struct soap* soap, struct _tds__GetDot11Status *tds__GetDot11Status, struct _tds__GetDot11StatusResponse *tds__GetDot11StatusResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__ScanAvailableDot11Networks(struct soap* soap, struct _tds__ScanAvailableDot11Networks *tds__ScanAvailableDot11Networks, struct _tds__ScanAvailableDot11NetworksResponse *tds__ScanAvailableDot11NetworksResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetSystemUris(struct soap* soap, struct _tds__GetSystemUris *tds__GetSystemUris, struct _tds__GetSystemUrisResponse *tds__GetSystemUrisResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__StartFirmwareUpgrade(struct soap* soap, struct _tds__StartFirmwareUpgrade *tds__StartFirmwareUpgrade, struct _tds__StartFirmwareUpgradeResponse *tds__StartFirmwareUpgradeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__StartSystemRestore(struct soap* soap, struct _tds__StartSystemRestore *tds__StartSystemRestore, struct _tds__StartSystemRestoreResponse *tds__StartSystemRestoreResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tetcp__CreatePullPoint(struct soap* soap, struct _wsnt__CreatePullPoint *wsnt__CreatePullPoint, struct _wsnt__CreatePullPointResponse *wsnt__CreatePullPointResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tete__GetServiceCapabilities(struct soap* soap, struct _tev__GetServiceCapabilities *tev__GetServiceCapabilities, struct _tev__GetServiceCapabilitiesResponse *tev__GetServiceCapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tete__CreatePullPointSubscription(struct soap* soap, struct _tev__CreatePullPointSubscription *tev__CreatePullPointSubscription, struct _tev__CreatePullPointSubscriptionResponse *tev__CreatePullPointSubscriptionResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tete__GetEventProperties(struct soap* soap, struct _tev__GetEventProperties *tev__GetEventProperties, struct _tev__GetEventPropertiesResponse *tev__GetEventPropertiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tetnc__Notify(struct soap* soap, struct _wsnt__Notify *wsnt__Notify){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tetnp__Subscribe(struct soap* soap, struct _wsnt__Subscribe *wsnt__Subscribe, struct _wsnt__SubscribeResponse *wsnt__SubscribeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tetnp__GetCurrentMessage(struct soap* soap, struct _wsnt__GetCurrentMessage *wsnt__GetCurrentMessage, struct _wsnt__GetCurrentMessageResponse *wsnt__GetCurrentMessageResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tetpp__GetMessages(struct soap* soap, struct _wsnt__GetMessages *wsnt__GetMessages, struct _wsnt__GetMessagesResponse *wsnt__GetMessagesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tetpp__DestroyPullPoint(struct soap* soap, struct _wsnt__DestroyPullPoint *wsnt__DestroyPullPoint, struct _wsnt__DestroyPullPointResponse *wsnt__DestroyPullPointResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tetpp__Notify(struct soap* soap, struct _wsnt__Notify *wsnt__Notify){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tetpps__PullMessages(struct soap* soap, struct _tev__PullMessages *tev__PullMessages, struct _tev__PullMessagesResponse *tev__PullMessagesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tetpps__SetSynchronizationPoint(struct soap* soap, struct _tev__SetSynchronizationPoint *tev__SetSynchronizationPoint, struct _tev__SetSynchronizationPointResponse *tev__SetSynchronizationPointResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tetps__Renew(struct soap* soap, struct _wsnt__Renew *wsnt__Renew, struct _wsnt__RenewResponse *wsnt__RenewResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tetps__Unsubscribe(struct soap* soap, struct _wsnt__Unsubscribe *wsnt__Unsubscribe, struct _wsnt__UnsubscribeResponse *wsnt__UnsubscribeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tetps__PauseSubscription(struct soap* soap, struct _wsnt__PauseSubscription *wsnt__PauseSubscription, struct _wsnt__PauseSubscriptionResponse *wsnt__PauseSubscriptionResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tetps__ResumeSubscription(struct soap* soap, struct _wsnt__ResumeSubscription *wsnt__ResumeSubscription, struct _wsnt__ResumeSubscriptionResponse *wsnt__ResumeSubscriptionResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tetsm__Renew(struct soap* soap, struct _wsnt__Renew *wsnt__Renew, struct _wsnt__RenewResponse *wsnt__RenewResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tetsm__Unsubscribe(struct soap* soap, struct _wsnt__Unsubscribe *wsnt__Unsubscribe, struct _wsnt__UnsubscribeResponse *wsnt__UnsubscribeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __timg__GetServiceCapabilities(struct soap* soap, struct _timg__GetServiceCapabilities *timg__GetServiceCapabilities, struct _timg__GetServiceCapabilitiesResponse *timg__GetServiceCapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __timg__GetImagingSettings(struct soap* soap, struct _timg__GetImagingSettings *timg__GetImagingSettings, struct _timg__GetImagingSettingsResponse *timg__GetImagingSettingsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __timg__SetImagingSettings(struct soap* soap, struct _timg__SetImagingSettings *timg__SetImagingSettings, struct _timg__SetImagingSettingsResponse *timg__SetImagingSettingsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __timg__GetOptions(struct soap* soap, struct _timg__GetOptions *timg__GetOptions, struct _timg__GetOptionsResponse *timg__GetOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __timg__Move(struct soap* soap, struct _timg__Move *timg__Move, struct _timg__MoveResponse *timg__MoveResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __timg__Stop(struct soap* soap, struct _timg__Stop *timg__Stop, struct _timg__StopResponse *timg__StopResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __timg__GetStatus(struct soap* soap, struct _timg__GetStatus *timg__GetStatus, struct _timg__GetStatusResponse *timg__GetStatusResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __timg__GetMoveOptions(struct soap* soap, struct _timg__GetMoveOptions *timg__GetMoveOptions, struct _timg__GetMoveOptionsResponse *timg__GetMoveOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tls__GetServiceCapabilities(struct soap* soap, struct _tls__GetServiceCapabilities *tls__GetServiceCapabilities, struct _tls__GetServiceCapabilitiesResponse *tls__GetServiceCapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tls__GetLayout(struct soap* soap, struct _tls__GetLayout *tls__GetLayout, struct _tls__GetLayoutResponse *tls__GetLayoutResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tls__SetLayout(struct soap* soap, struct _tls__SetLayout *tls__SetLayout, struct _tls__SetLayoutResponse *tls__SetLayoutResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tls__GetDisplayOptions(struct soap* soap, struct _tls__GetDisplayOptions *tls__GetDisplayOptions, struct _tls__GetDisplayOptionsResponse *tls__GetDisplayOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tls__GetPaneConfigurations(struct soap* soap, struct _tls__GetPaneConfigurations *tls__GetPaneConfigurations, struct _tls__GetPaneConfigurationsResponse *tls__GetPaneConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tls__GetPaneConfiguration(struct soap* soap, struct _tls__GetPaneConfiguration *tls__GetPaneConfiguration, struct _tls__GetPaneConfigurationResponse *tls__GetPaneConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tls__SetPaneConfigurations(struct soap* soap, struct _tls__SetPaneConfigurations *tls__SetPaneConfigurations, struct _tls__SetPaneConfigurationsResponse *tls__SetPaneConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tls__SetPaneConfiguration(struct soap* soap, struct _tls__SetPaneConfiguration *tls__SetPaneConfiguration, struct _tls__SetPaneConfigurationResponse *tls__SetPaneConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tls__CreatePaneConfiguration(struct soap* soap, struct _tls__CreatePaneConfiguration *tls__CreatePaneConfiguration, struct _tls__CreatePaneConfigurationResponse *tls__CreatePaneConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tls__DeletePaneConfiguration(struct soap* soap, struct _tls__DeletePaneConfiguration *tls__DeletePaneConfiguration, struct _tls__DeletePaneConfigurationResponse *tls__DeletePaneConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__GetServiceCapabilities(struct soap* soap, struct _tmd__GetServiceCapabilities *tmd__GetServiceCapabilities, struct _tmd__GetServiceCapabilitiesResponse *tmd__GetServiceCapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__GetAudioSources(struct soap* soap, struct tmd__Get *trt__GetAudioSources, struct tmd__GetResponse *trt__GetAudioSourcesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__GetAudioOutputs(struct soap* soap, struct tmd__Get *trt__GetAudioOutputs, struct tmd__GetResponse *trt__GetAudioOutputsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__GetVideoSources(struct soap* soap, struct tmd__Get *trt__GetVideoSources, struct tmd__GetResponse *trt__GetVideoSourcesResponse)
{
	int flag = 0, j =0;
	float _Framerate; 
	float _Brightness;
	float _Saturation; 
	float _Contrast; 
	float _Sharpness; 
	float _WideDynamicRange;
	int _Backlight; 
	float _BacklightCompensationLevel; 
	int _WhiteBalance;
	int height;
	int size = 0;//oSysInfo->nprofile; 
	int i = 0;
	int n = 0;

	TRACE("%s in \n",__FUNCTION__);
	
#if 0 
	size = trt__GetVideoSourcesResponse->__sizeVideoSources =1;
	trt__GetVideoSourcesResponse->VideoSources = (struct tt__VideoSource *)soap_malloc(soap, sizeof(struct tt__VideoSource) * size);
	
	
	for(i = 0; i < 1 ; i++)
	{		
		soap_default_tt__VideoSource(soap,&trt__GetVideoSourcesResponse->VideoSources[n]);
		flag = NOT_EXIST;
	
		if(!flag)  
		{
			_Framerate = 30;
			_Brightness = (float)(50 * 100) / 255; 
			_Saturation = (float)(50 * 100) / 255; 
			_Contrast = (float)(50 * 100) / 255; 
			_Sharpness = (float)(50 * 100) / 255;
			_WideDynamicRange = 0;
			_Backlight = 0; 
			_BacklightCompensationLevel = 0;
			_WhiteBalance = 1;
			height = g_pIpcamAll.onvif_st.input[i].h;

			//		for(i=0; i< size; i++)
			
			trt__GetVideoSourcesResponse->VideoSources[n].token =  g_pIpcamAll.onvif_st.input[i].profiletoken;//"IPNC_VideoSource";
			trt__GetVideoSourcesResponse->VideoSources[n].Framerate = _Framerate;
			trt__GetVideoSourcesResponse->VideoSources[n].Resolution = (struct tt__VideoResolution*)soap_malloc(soap, sizeof(struct tt__VideoResolution));
			trt__GetVideoSourcesResponse->VideoSources[n].Resolution->Width =  g_pIpcamAll.onvif_st.input[i].w;//width;
			trt__GetVideoSourcesResponse->VideoSources[n].Resolution->Height =  g_pIpcamAll.onvif_st.input[i].h;
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging =(struct tt__ImagingSettings*)soap_malloc(soap, sizeof(struct tt__ImagingSettings));
			soap_default_tt__ImagingSettings(soap,trt__GetVideoSourcesResponse->VideoSources[n].Imaging);
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->Brightness = (float*)soap_malloc(soap,sizeof(float));
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->Brightness[0] = _Brightness;
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->ColorSaturation = (float*)soap_malloc(soap,sizeof(float));
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->ColorSaturation[0] = _Saturation;
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->Contrast = (float*)soap_malloc(soap,sizeof(float));
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->Contrast[0] = _Contrast;
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->IrCutFilter = (int *)soap_malloc(soap,sizeof(int));
			*trt__GetVideoSourcesResponse->VideoSources[n].Imaging->IrCutFilter =  0; // dummy //{onv__IrCutFilterMode__ON = 0, onv__IrCutFilterMode__OFF = 1, onv__IrCutFilterMode__AUTO = 2}
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->Sharpness = (float*)soap_malloc(soap,sizeof(float));
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->Sharpness[0] = _Sharpness;
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->BacklightCompensation = (struct tt__BacklightCompensation*)soap_malloc(soap, sizeof(struct tt__BacklightCompensation));
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->BacklightCompensation->Mode = _Backlight;//Streaming_onvif->BacklightCompensationMode;  //{onv__BacklightCompensationMode__OFF = 0, onv__BacklightCompensationMode__ON = 1}
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->BacklightCompensation->Level = _BacklightCompensationLevel;//float
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->Exposure = NULL;
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->Focus = NULL;
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->WideDynamicRange = (struct tt__WideDynamicRange*)soap_malloc(soap, sizeof(struct tt__WideDynamicRange));
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->WideDynamicRange->Mode = _WideDynamicRange;   //{onv__WideDynamicMode__OFF = 0, onv__WideDynamicMode__ON = 1}
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->WideDynamicRange->Level = 0;// dummy float
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->WhiteBalance = (struct tt__WhiteBalance*)soap_malloc(soap, sizeof(struct tt__WhiteBalance));
			soap_default_tt__WhiteBalance(soap,trt__GetVideoSourcesResponse->VideoSources[n].Imaging->WhiteBalance);
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->WhiteBalance->Mode = _WhiteBalance;	//{onv__WhiteBalanceMode__AUTO = 0, onv__WhiteBalanceMode__MANUAL = 1}
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->WhiteBalance->CrGain = 0; // dummy
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->WhiteBalance->CbGain = 0; // dummy
			trt__GetVideoSourcesResponse->VideoSources[n].Imaging->Extension = NULL;
			trt__GetVideoSourcesResponse->VideoSources[n].Extension = NULL;
			n++;
		}
		}
#else
	soap_default_tmd__GetResponse(soap,trt__GetVideoSourcesResponse);
	size = trt__GetVideoSourcesResponse->__sizeToken =1;
	trt__GetVideoSourcesResponse->Token = (char *)soap_malloc(soap, sizeof(char *)*1);
	trt__GetVideoSourcesResponse->Token[0] = (char *)soap_malloc(soap, sizeof(char)*64);
	strcpy(trt__GetVideoSourcesResponse->Token[0],g_pIpcamAll.onvif_st.input[0].profiletoken);//"IPNC_VideoSource";
	
#endif
	TRACE("%s out \n",__FUNCTION__);
	return SOAP_OK; 

}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__GetVideoOutputs(struct soap* soap, struct _tmd__GetVideoOutputs *tmd__GetVideoOutputs, struct _tmd__GetVideoOutputsResponse *tmd__GetVideoOutputsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__GetVideoSourceConfiguration(struct soap* soap, struct _tmd__GetVideoSourceConfiguration *tmd__GetVideoSourceConfiguration, struct _tmd__GetVideoSourceConfigurationResponse *tmd__GetVideoSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__GetVideoOutputConfiguration(struct soap* soap, struct _tmd__GetVideoOutputConfiguration *tmd__GetVideoOutputConfiguration, struct _tmd__GetVideoOutputConfigurationResponse *tmd__GetVideoOutputConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__GetAudioSourceConfiguration(struct soap* soap, struct _tmd__GetAudioSourceConfiguration *tmd__GetAudioSourceConfiguration, struct _tmd__GetAudioSourceConfigurationResponse *tmd__GetAudioSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__GetAudioOutputConfiguration(struct soap* soap, struct _tmd__GetAudioOutputConfiguration *tmd__GetAudioOutputConfiguration, struct _tmd__GetAudioOutputConfigurationResponse *tmd__GetAudioOutputConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__SetVideoSourceConfiguration(struct soap* soap, struct _tmd__SetVideoSourceConfiguration *tmd__SetVideoSourceConfiguration, struct _tmd__SetVideoSourceConfigurationResponse *tmd__SetVideoSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__SetVideoOutputConfiguration(struct soap* soap, struct _tmd__SetVideoOutputConfiguration *tmd__SetVideoOutputConfiguration, struct _tmd__SetVideoOutputConfigurationResponse *tmd__SetVideoOutputConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__SetAudioSourceConfiguration(struct soap* soap, struct _tmd__SetAudioSourceConfiguration *tmd__SetAudioSourceConfiguration, struct _tmd__SetAudioSourceConfigurationResponse *tmd__SetAudioSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__SetAudioOutputConfiguration(struct soap* soap, struct _tmd__SetAudioOutputConfiguration *tmd__SetAudioOutputConfiguration, struct _tmd__SetAudioOutputConfigurationResponse *tmd__SetAudioOutputConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__GetVideoSourceConfigurationOptions(struct soap* soap, struct _tmd__GetVideoSourceConfigurationOptions *tmd__GetVideoSourceConfigurationOptions, struct _tmd__GetVideoSourceConfigurationOptionsResponse *tmd__GetVideoSourceConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__GetVideoOutputConfigurationOptions(struct soap* soap, struct _tmd__GetVideoOutputConfigurationOptions *tmd__GetVideoOutputConfigurationOptions, struct _tmd__GetVideoOutputConfigurationOptionsResponse *tmd__GetVideoOutputConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__GetAudioSourceConfigurationOptions(struct soap* soap, struct _tmd__GetAudioSourceConfigurationOptions *tmd__GetAudioSourceConfigurationOptions, struct _tmd__GetAudioSourceConfigurationOptionsResponse *tmd__GetAudioSourceConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__GetAudioOutputConfigurationOptions(struct soap* soap, struct _tmd__GetAudioOutputConfigurationOptions *tmd__GetAudioOutputConfigurationOptions, struct _tmd__GetAudioOutputConfigurationOptionsResponse *tmd__GetAudioOutputConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__GetRelayOutputs(struct soap* soap, struct _tds__GetRelayOutputs *tds__GetRelayOutputs, struct _tds__GetRelayOutputsResponse *tds__GetRelayOutputsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__SetRelayOutputSettings(struct soap* soap, struct _tmd__SetRelayOutputSettings *tmd__SetRelayOutputSettings, struct _tmd__SetRelayOutputSettingsResponse *tmd__SetRelayOutputSettingsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__SetRelayOutputState(struct soap* soap, struct _tds__SetRelayOutputState *tds__SetRelayOutputState, struct _tds__SetRelayOutputStateResponse *tds__SetRelayOutputStateResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tptz__GetServiceCapabilities(struct soap* soap, struct _tptz__GetServiceCapabilities *tptz__GetServiceCapabilities, struct _tptz__GetServiceCapabilitiesResponse *tptz__GetServiceCapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tptz__GetConfigurations(struct soap* soap, struct _tptz__GetConfigurations *tptz__GetConfigurations, struct _tptz__GetConfigurationsResponse *tptz__GetConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tptz__GetPresets(struct soap* soap, struct _tptz__GetPresets *tptz__GetPresets, struct _tptz__GetPresetsResponse *tptz__GetPresetsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tptz__SetPreset(struct soap* soap, struct _tptz__SetPreset *tptz__SetPreset, struct _tptz__SetPresetResponse *tptz__SetPresetResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tptz__RemovePreset(struct soap* soap, struct _tptz__RemovePreset *tptz__RemovePreset, struct _tptz__RemovePresetResponse *tptz__RemovePresetResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tptz__GotoPreset(struct soap* soap, struct _tptz__GotoPreset *tptz__GotoPreset, struct _tptz__GotoPresetResponse *tptz__GotoPresetResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tptz__GetStatus(struct soap* soap, struct _tptz__GetStatus *tptz__GetStatus, struct _tptz__GetStatusResponse *tptz__GetStatusResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tptz__GetConfiguration(struct soap* soap, struct _tptz__GetConfiguration *tptz__GetConfiguration, struct _tptz__GetConfigurationResponse *tptz__GetConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tptz__GetNodes(struct soap* soap, struct _tptz__GetNodes *tptz__GetNodes, struct _tptz__GetNodesResponse *tptz__GetNodesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tptz__GetNode(struct soap* soap, struct _tptz__GetNode *tptz__GetNode, struct _tptz__GetNodeResponse *tptz__GetNodeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tptz__SetConfiguration(struct soap* soap, struct _tptz__SetConfiguration *tptz__SetConfiguration, struct _tptz__SetConfigurationResponse *tptz__SetConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tptz__GetConfigurationOptions(struct soap* soap, struct _tptz__GetConfigurationOptions *tptz__GetConfigurationOptions, struct _tptz__GetConfigurationOptionsResponse *tptz__GetConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tptz__GotoHomePosition(struct soap* soap, struct _tptz__GotoHomePosition *tptz__GotoHomePosition, struct _tptz__GotoHomePositionResponse *tptz__GotoHomePositionResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tptz__SetHomePosition(struct soap* soap, struct _tptz__SetHomePosition *tptz__SetHomePosition, struct _tptz__SetHomePositionResponse *tptz__SetHomePositionResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tptz__ContinuousMove(struct soap* soap, struct _tptz__ContinuousMove *tptz__ContinuousMove, struct _tptz__ContinuousMoveResponse *tptz__ContinuousMoveResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tptz__RelativeMove(struct soap* soap, struct _tptz__RelativeMove *tptz__RelativeMove, struct _tptz__RelativeMoveResponse *tptz__RelativeMoveResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tptz__SendAuxiliaryCommand(struct soap* soap, struct _tptz__SendAuxiliaryCommand *tptz__SendAuxiliaryCommand, struct _tptz__SendAuxiliaryCommandResponse *tptz__SendAuxiliaryCommandResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tptz__AbsoluteMove(struct soap* soap, struct _tptz__AbsoluteMove *tptz__AbsoluteMove, struct _tptz__AbsoluteMoveResponse *tptz__AbsoluteMoveResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tptz__Stop(struct soap* soap, struct _tptz__Stop *tptz__Stop, struct _tptz__StopResponse *tptz__StopResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trc__GetServiceCapabilities(struct soap* soap, struct _trc__GetServiceCapabilities *trc__GetServiceCapabilities, struct _trc__GetServiceCapabilitiesResponse *trc__GetServiceCapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trc__CreateRecording(struct soap* soap, struct _trc__CreateRecording *trc__CreateRecording, struct _trc__CreateRecordingResponse *trc__CreateRecordingResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trc__DeleteRecording(struct soap* soap, struct _trc__DeleteRecording *trc__DeleteRecording, struct _trc__DeleteRecordingResponse *trc__DeleteRecordingResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trc__GetRecordings(struct soap* soap, struct _trc__GetRecordings *trc__GetRecordings, struct _trc__GetRecordingsResponse *trc__GetRecordingsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trc__SetRecordingConfiguration(struct soap* soap, struct _trc__SetRecordingConfiguration *trc__SetRecordingConfiguration, struct _trc__SetRecordingConfigurationResponse *trc__SetRecordingConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trc__GetRecordingConfiguration(struct soap* soap, struct _trc__GetRecordingConfiguration *trc__GetRecordingConfiguration, struct _trc__GetRecordingConfigurationResponse *trc__GetRecordingConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trc__CreateTrack(struct soap* soap, struct _trc__CreateTrack *trc__CreateTrack, struct _trc__CreateTrackResponse *trc__CreateTrackResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trc__DeleteTrack(struct soap* soap, struct _trc__DeleteTrack *trc__DeleteTrack, struct _trc__DeleteTrackResponse *trc__DeleteTrackResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trc__GetTrackConfiguration(struct soap* soap, struct _trc__GetTrackConfiguration *trc__GetTrackConfiguration, struct _trc__GetTrackConfigurationResponse *trc__GetTrackConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trc__SetTrackConfiguration(struct soap* soap, struct _trc__SetTrackConfiguration *trc__SetTrackConfiguration, struct _trc__SetTrackConfigurationResponse *trc__SetTrackConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trc__CreateRecordingJob(struct soap* soap, struct _trc__CreateRecordingJob *trc__CreateRecordingJob, struct _trc__CreateRecordingJobResponse *trc__CreateRecordingJobResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trc__DeleteRecordingJob(struct soap* soap, struct _trc__DeleteRecordingJob *trc__DeleteRecordingJob, struct _trc__DeleteRecordingJobResponse *trc__DeleteRecordingJobResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trc__GetRecordingJobs(struct soap* soap, struct _trc__GetRecordingJobs *trc__GetRecordingJobs, struct _trc__GetRecordingJobsResponse *trc__GetRecordingJobsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trc__SetRecordingJobConfiguration(struct soap* soap, struct _trc__SetRecordingJobConfiguration *trc__SetRecordingJobConfiguration, struct _trc__SetRecordingJobConfigurationResponse *trc__SetRecordingJobConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trc__GetRecordingJobConfiguration(struct soap* soap, struct _trc__GetRecordingJobConfiguration *trc__GetRecordingJobConfiguration, struct _trc__GetRecordingJobConfigurationResponse *trc__GetRecordingJobConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trc__SetRecordingJobMode(struct soap* soap, struct _trc__SetRecordingJobMode *trc__SetRecordingJobMode, struct _trc__SetRecordingJobModeResponse *trc__SetRecordingJobModeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trc__GetRecordingJobState(struct soap* soap, struct _trc__GetRecordingJobState *trc__GetRecordingJobState, struct _trc__GetRecordingJobStateResponse *trc__GetRecordingJobStateResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trp__GetServiceCapabilities(struct soap* soap, struct _trp__GetServiceCapabilities *trp__GetServiceCapabilities, struct _trp__GetServiceCapabilitiesResponse *trp__GetServiceCapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trp__GetReplayUri(struct soap* soap, struct _trp__GetReplayUri *trp__GetReplayUri, struct _trp__GetReplayUriResponse *trp__GetReplayUriResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trp__GetReplayConfiguration(struct soap* soap, struct _trp__GetReplayConfiguration *trp__GetReplayConfiguration, struct _trp__GetReplayConfigurationResponse *trp__GetReplayConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trp__SetReplayConfiguration(struct soap* soap, struct _trp__SetReplayConfiguration *trp__SetReplayConfiguration, struct _trp__SetReplayConfigurationResponse *trp__SetReplayConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetServiceCapabilities(struct soap* soap, struct _trt__GetServiceCapabilities *trt__GetServiceCapabilities, struct _trt__GetServiceCapabilitiesResponse *trt__GetServiceCapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoSources(struct soap* soap, struct _trt__GetVideoSources *trt__GetVideoSources, struct _trt__GetVideoSourcesResponse *trt__GetVideoSourcesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioSources(struct soap* soap, struct _trt__GetAudioSources *trt__GetAudioSources, struct _trt__GetAudioSourcesResponse *trt__GetAudioSourcesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioOutputs(struct soap* soap, struct _trt__GetAudioOutputs *trt__GetAudioOutputs, struct _trt__GetAudioOutputsResponse *trt__GetAudioOutputsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__CreateProfile(struct soap* soap, struct _trt__CreateProfile *trt__CreateProfile, struct _trt__CreateProfileResponse *trt__CreateProfileResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetProfile(struct soap* soap, struct _trt__GetProfile *trt__GetProfile, struct _trt__GetProfileResponse *trt__GetProfileResponse)
{

	char _IPAddr[LARGE_INFO_LENGTH];
	NET_IPV4 ip;
	ip.int32 = net_get_ifaddr(ETH_NAME);
	sprintf(_IPAddr, "%3d.%3d.%3d.%3d", ip.str[0], ip.str[1], ip.str[2], ip.str[3]); 
	int i;
	int Ptoken_status = NOT_EXIST;

	DPRINTK("1 %s\n",trt__GetProfile->ProfileToken);

	if((trt__GetProfile->ProfileToken) == NULL)
	{
		onvif_fault(soap,"ter:InvalidArgVal", "ter:InvalidInputToken"); 
		return SOAP_FAULT;
	}
	if(strcmp(trt__GetProfile->ProfileToken, "") == 0)
	{
		onvif_fault(soap,"ter:InvalidArgVal", "ter:InvalidInputToken"); 
		return SOAP_FAULT;
	}

	/* Check if ProfileToken Exist or Not */
	for(i = 0; i < MAX_RTSP_NUM; i++)
	{
		if(strcmp(trt__GetProfile->ProfileToken, g_pIpcamAll.onvif_st.profile_st[i].profiletoken) == 0)
		{
			Ptoken_status = EXIST;
			break;
		}
	}
	if(!Ptoken_status)
	{
		onvif_fault(soap, "ter:InvalidArgVal", "ter:NoProfile"); 
		return SOAP_FAULT;
	}

	trt__GetProfileResponse->Profile = (struct tt__Profile *)soap_malloc(soap, sizeof(struct tt__Profile));
	soap_default_tt__Profile(soap,trt__GetProfileResponse->Profile);
	trt__GetProfileResponse->Profile->Name =   g_pIpcamAll.onvif_st.profile_st[i].profilename;
	trt__GetProfileResponse->Profile->token =  g_pIpcamAll.onvif_st.profile_st[i].profiletoken;
	trt__GetProfileResponse->Profile->fixed = (int *)soap_malloc(soap, sizeof(int)); //xsd__boolean__false_ = 0, xsd__boolean__true_ = 1
	*trt__GetProfileResponse->Profile->fixed = _false; //xsd__boolean__false_ = 0, xsd__boolean__true_ = 1
	/* VideoSourceConfiguration */
	trt__GetProfileResponse->Profile->VideoSourceConfiguration = (struct tt__VideoSourceConfiguration *)soap_malloc(soap, sizeof(struct tt__VideoSourceConfiguration));
	soap_default_tt__VideoSourceConfiguration(soap,trt__GetProfileResponse->Profile->VideoSourceConfiguration );
	trt__GetProfileResponse->Profile->VideoSourceConfiguration->Name = g_pIpcamAll.onvif_st.input[i].profilename;
	trt__GetProfileResponse->Profile->VideoSourceConfiguration->UseCount = 1;
	trt__GetProfileResponse->Profile->VideoSourceConfiguration->token = g_pIpcamAll.onvif_st.input[i].profiletoken;
	trt__GetProfileResponse->Profile->VideoSourceConfiguration->SourceToken = g_pIpcamAll.onvif_st.input[i].profiletoken;
	trt__GetProfileResponse->Profile->VideoSourceConfiguration->Bounds = (struct tt__IntRectangle *)soap_malloc(soap, sizeof(struct tt__IntRectangle));
	trt__GetProfileResponse->Profile->VideoSourceConfiguration->Bounds->y = g_pIpcamAll.onvif_st.input[i].y;
	trt__GetProfileResponse->Profile->VideoSourceConfiguration->Bounds->x = g_pIpcamAll.onvif_st.input[i].x;
	trt__GetProfileResponse->Profile->VideoSourceConfiguration->Bounds->width  = g_pIpcamAll.onvif_st.input[i].w;
	trt__GetProfileResponse->Profile->VideoSourceConfiguration->Bounds->height = g_pIpcamAll.onvif_st.input[i].h;

	/* AudioSourceConfiguration */
	trt__GetProfileResponse->Profile->AudioSourceConfiguration = NULL;
	/*VideoEncoderConfiguration */
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration = (struct tt__VideoEncoderConfiguration *)soap_malloc(soap, sizeof(struct tt__VideoEncoderConfiguration));
	soap_default_tt__VideoEncoderConfiguration(soap,trt__GetProfileResponse->Profile->VideoEncoderConfiguration );
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->Name =g_pIpcamAll.onvif_st.vcode[i].Name;
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->UseCount = 1;
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->token = g_pIpcamAll.onvif_st.vcode[i].token;
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->Encoding = g_pIpcamAll.onvif_st.vcode[i].Encoding;//JPEG = 0, MPEG4 = 1, H264 = 2;
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->Resolution = (struct tt__VideoResolution *)soap_malloc(soap, sizeof(struct tt__VideoResolution));
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->Resolution->Width = g_pIpcamAll.onvif_st.vcode[i].w;
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->Resolution->Height = g_pIpcamAll.onvif_st.vcode[i].h;
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->Quality = g_pIpcamAll.onvif_st.vcode[i].Quality; 
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->RateControl = (struct tt__VideoRateControl *)soap_malloc(soap, sizeof(struct tt__VideoRateControl));
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->RateControl->FrameRateLimit = g_pIpcamAll.onvif_st.vcode[i].FrameRateLimit; 
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->RateControl->EncodingInterval =  g_pIpcamAll.onvif_st.vcode[i].EncodingInterval;
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->RateControl->BitrateLimit = g_pIpcamAll.onvif_st.vcode[i].BitrateLimit;


	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->MPEG4 = (struct tt__Mpeg4Configuration *)soap_malloc(soap, sizeof(struct tt__Mpeg4Configuration));
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->MPEG4->GovLength = g_pIpcamAll.onvif_st.vcode[i].GovLength;
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->MPEG4->Mpeg4Profile = 0;//SP = 0, ASP = 1;
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->H264 = (struct tt__H264Configuration *)soap_malloc(soap, sizeof(struct tt__H264Configuration));
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->H264->GovLength = g_pIpcamAll.onvif_st.vcode[i].GovLength;
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->H264->H264Profile = 1;//Baseline = 0, Main = 1, High = 3
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->Multicast = (struct tt__MulticastConfiguration *)soap_malloc(soap, sizeof(struct tt__MulticastConfiguration));
	soap_default_tt__MulticastConfiguration(soap,trt__GetProfileResponse->Profile->VideoEncoderConfiguration->Multicast);
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->Multicast->Port = 0;
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->Multicast->TTL = 0;
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->Multicast->AutoStart = _true;
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->Multicast->Address  = (struct tt__IPAddress *)soap_malloc(soap, sizeof(struct tt__IPAddress));
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->Multicast->Address->Type = tt__IPType__IPv4;
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->Multicast->Address->IPv4Address = (char **)soap_malloc(soap, sizeof(char *));
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->Multicast->Address->IPv4Address[0] = (char *)soap_malloc(soap, sizeof(char) * LARGE_INFO_LENGTH);
	strcpy(*trt__GetProfileResponse->Profile->VideoEncoderConfiguration->Multicast->Address->IPv4Address, _IPAddr);
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->Multicast->Address->IPv6Address = NULL;
	trt__GetProfileResponse->Profile->VideoEncoderConfiguration->SessionTimeout= (char *)soap_malloc(soap, sizeof(char) * INFO_LENGTH);
	strcpy(trt__GetProfileResponse->Profile->VideoEncoderConfiguration->SessionTimeout , "PT0H12M0S");	
	/* AudioEncoderConfiguration */
	trt__GetProfileResponse->Profile->AudioEncoderConfiguration = NULL;
	/* VideoAnalyticsConfiguration */
	trt__GetProfileResponse->Profile->VideoAnalyticsConfiguration = NULL;
	/* PTZConfiguration */    
	trt__GetProfileResponse->Profile->PTZConfiguration = NULL;
	/* MetadataConfiguration */
	trt__GetProfileResponse->Profile->MetadataConfiguration = NULL;
	trt__GetProfileResponse->Profile->Extension = NULL;

	DPRINTK("2\n");
	
	return SOAP_OK;
}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetProfiles(struct soap* soap, struct _trt__GetProfiles *trt__GetProfiles, struct _trt__GetProfilesResponse *trt__GetProfilesResponse)
{
	
	int i;
	char _IPAddr[LARGE_INFO_LENGTH];
	NET_IPV4 ip;
	ip.int32 = net_get_ifaddr(ETH_NAME);
	sprintf(_IPAddr, "%d.%d.%d.%d", ip.str[0], ip.str[1], ip.str[2], ip.str[3]); 

	soap_default__trt__GetProfilesResponse(soap,trt__GetProfilesResponse);
	
	trt__GetProfilesResponse->Profiles =(struct tt__Profile *)soap_malloc(soap, sizeof(struct tt__Profile) * g_pIpcamAll.onvif_st.__sizeProfiles);
	soap_default_tt__Profile(soap,trt__GetProfilesResponse->Profiles );
	trt__GetProfilesResponse->__sizeProfiles = 2;//g_pIpcamAll.onvif_st.__sizeProfiles;	

	DPRINTK("nprofile =%d\n",trt__GetProfilesResponse->__sizeProfiles);
	g_pIpcamAll.onvif_st.vcode[0].BitrateLimit = 4096;

	for(i = 0; i < trt__GetProfilesResponse->__sizeProfiles; i++)
	{		
		trt__GetProfilesResponse->Profiles[i].Name =  g_pIpcamAll.onvif_st.profile_st[i].profilename;	
		trt__GetProfilesResponse->Profiles[i].token = g_pIpcamAll.onvif_st.profile_st[i].profiletoken;
		trt__GetProfilesResponse->Profiles[i].fixed = (int *)soap_malloc(soap, sizeof(int)); //false_ = 0, true_ = 1
		*trt__GetProfilesResponse->Profiles[i].fixed = _true; //false_ = 0, true_ = 1

		DPRINTK("Name=%s 1\n",trt__GetProfilesResponse->Profiles[i].Name);

		/* VideoSourceConfiguration */
		if(strcmp(g_pIpcamAll.onvif_st.input[i].profiletoken, "") != 0 )
		{
			trt__GetProfilesResponse->Profiles[i].VideoSourceConfiguration = 
				(struct tt__VideoSourceConfiguration *)soap_malloc(soap, sizeof(struct tt__VideoSourceConfiguration));
			soap_default_tt__VideoSourceConfiguration(soap,trt__GetProfilesResponse->Profiles[i].VideoSourceConfiguration );
			
			trt__GetProfilesResponse->Profiles[i].VideoSourceConfiguration->Name = g_pIpcamAll.onvif_st.input[i].profilename;	
			trt__GetProfilesResponse->Profiles[i].VideoSourceConfiguration->UseCount = 1;
			trt__GetProfilesResponse->Profiles[i].VideoSourceConfiguration->token = g_pIpcamAll.onvif_st.input[i].profiletoken;	
			trt__GetProfilesResponse->Profiles[i].VideoSourceConfiguration->SourceToken = g_pIpcamAll.onvif_st.input[0].profiletoken;	
			trt__GetProfilesResponse->Profiles[i].VideoSourceConfiguration->Bounds = (struct tt__IntRectangle *)soap_malloc(soap, sizeof(struct tt__IntRectangle));
			trt__GetProfilesResponse->Profiles[i].VideoSourceConfiguration->Bounds->y = g_pIpcamAll.onvif_st.input[i].y;
			trt__GetProfilesResponse->Profiles[i].VideoSourceConfiguration->Bounds->x = g_pIpcamAll.onvif_st.input[i].x;
			trt__GetProfilesResponse->Profiles[i].VideoSourceConfiguration->Bounds->width  = g_pIpcamAll.onvif_st.input[i].w;
			trt__GetProfilesResponse->Profiles[i].VideoSourceConfiguration->Bounds->height = g_pIpcamAll.onvif_st.input[i].h;
			
		} else {
			 trt__GetProfilesResponse->Profiles[i].VideoSourceConfiguration = NULL;
		}
		
		trt__GetProfilesResponse->Profiles[i].AudioSourceConfiguration = NULL;

		
		if(strcmp(g_pIpcamAll.onvif_st.vcode[i].token, "") != 0 )
		{
			trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration = 
				(struct tt__VideoEncoderConfiguration *)soap_malloc(soap, sizeof(struct tt__VideoEncoderConfiguration));
			soap_default_tt__VideoEncoderConfiguration(soap,trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration  );

		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->Name = g_pIpcamAll.onvif_st.vcode[i].Name;	
		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->UseCount = 1;
		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->token = g_pIpcamAll.onvif_st.vcode[i].token;
		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->Encoding = g_pIpcamAll.onvif_st.vcode[i].Encoding;//JPEG = 0, MPEG4 = 1, H264 = 2;

		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->Resolution = (struct tt__VideoResolution *)soap_malloc(soap, sizeof(struct tt__VideoResolution));

		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->Resolution->Width = g_pIpcamAll.onvif_st.vcode[i].w;
		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->Resolution->Height = g_pIpcamAll.onvif_st.vcode[i].h;
		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->Quality = g_pIpcamAll.onvif_st.vcode[i].Quality; 

		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->RateControl = (struct tt__VideoRateControl *)soap_malloc(soap, sizeof(struct tt__VideoRateControl));
		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->RateControl->FrameRateLimit = g_pIpcamAll.onvif_st.vcode[i].FrameRateLimit;
		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->RateControl->EncodingInterval = g_pIpcamAll.onvif_st.vcode[i].EncodingInterval;
		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->RateControl->BitrateLimit = g_pIpcamAll.onvif_st.vcode[i].BitrateLimit;

		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->MPEG4 = (struct tt__Mpeg4Configuration *)soap_malloc(soap, sizeof(struct tt__Mpeg4Configuration));
		if(i == 0)	
		{
			trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->MPEG4->GovLength = g_pIpcamAll.onvif_st.vcode[i].GovLength;
			trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->MPEG4->Mpeg4Profile = 0; //SP = 0, ASP = 1
		}
		else
		{
			trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->MPEG4->GovLength = g_pIpcamAll.onvif_st.vcode[i].GovLength;
			trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->MPEG4->Mpeg4Profile = 0; //SP = 0, ASP = 1
		}
		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->H264 = (struct tt__H264Configuration *)soap_malloc(soap, sizeof(struct tt__H264Configuration));
		if(i == 0)	
		{
			trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->H264->GovLength = g_pIpcamAll.onvif_st.vcode[i].GovLength;
			trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->H264->H264Profile = 1;//Baseline = 0, Main = 1, High = 3
		}
		else
		{
			trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->H264->GovLength = g_pIpcamAll.onvif_st.vcode[i].GovLength;
			trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->H264->H264Profile = 1; //Baseline = 0, Main = 1, High = 3
		}
			trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->Multicast = 
					(struct tt__MulticastConfiguration *)soap_malloc(soap, sizeof(struct tt__MulticastConfiguration));
			soap_default_tt__MulticastConfiguration(soap,trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->Multicast  );

		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->Multicast->Port = 0;
		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->Multicast->TTL = 0;
		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->Multicast->AutoStart = _true;//false
		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->Multicast->Address  = (struct tt__IPAddress *)soap_malloc(soap, sizeof(struct tt__IPAddress));
		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->Multicast->Address->Type = tt__IPType__IPv4;
		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->Multicast->Address->IPv4Address = (char **)soap_malloc(soap, sizeof(char *));
		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->Multicast->Address->IPv4Address[0] = (char *)soap_malloc(soap, sizeof(char) * INFO_LENGTH);
		strcpy(*trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->Multicast->Address->IPv4Address, _IPAddr);
		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->Multicast->Address->IPv6Address = NULL;
		trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->SessionTimeout= (char *)soap_malloc(soap, sizeof(char) * INFO_LENGTH);
		strcpy(trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration->SessionTimeout , "PT0H12M0S");
		} else {
			trt__GetProfilesResponse->Profiles[i].VideoEncoderConfiguration = NULL;
		}
	
		trt__GetProfilesResponse->Profiles[i].AudioEncoderConfiguration =NULL;
		
		trt__GetProfilesResponse->Profiles[i].VideoAnalyticsConfiguration = NULL;
		
		trt__GetProfilesResponse->Profiles[i].PTZConfiguration = NULL;
		
		trt__GetProfilesResponse->Profiles[i].MetadataConfiguration = NULL;
		trt__GetProfilesResponse->Profiles[i].Extension = NULL;	
		
	}

	DPRINTK("end\n");
	return SOAP_OK;
}

SOAP_FMAC5 int SOAP_FMAC6 __trt__AddVideoEncoderConfiguration(struct soap* soap, struct _trt__AddVideoEncoderConfiguration *trt__AddVideoEncoderConfiguration, struct _trt__AddVideoEncoderConfigurationResponse *trt__AddVideoEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__AddVideoSourceConfiguration(struct soap* soap, struct _trt__AddVideoSourceConfiguration *trt__AddVideoSourceConfiguration, struct _trt__AddVideoSourceConfigurationResponse *trt__AddVideoSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__AddAudioEncoderConfiguration(struct soap* soap, struct _trt__AddAudioEncoderConfiguration *trt__AddAudioEncoderConfiguration, struct _trt__AddAudioEncoderConfigurationResponse *trt__AddAudioEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__AddAudioSourceConfiguration(struct soap* soap, struct _trt__AddAudioSourceConfiguration *trt__AddAudioSourceConfiguration, struct _trt__AddAudioSourceConfigurationResponse *trt__AddAudioSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__AddPTZConfiguration(struct soap* soap, struct _trt__AddPTZConfiguration *trt__AddPTZConfiguration, struct _trt__AddPTZConfigurationResponse *trt__AddPTZConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__AddVideoAnalyticsConfiguration(struct soap* soap, struct _trt__AddVideoAnalyticsConfiguration *trt__AddVideoAnalyticsConfiguration, struct _trt__AddVideoAnalyticsConfigurationResponse *trt__AddVideoAnalyticsConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__AddMetadataConfiguration(struct soap* soap, struct _trt__AddMetadataConfiguration *trt__AddMetadataConfiguration, struct _trt__AddMetadataConfigurationResponse *trt__AddMetadataConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__AddAudioOutputConfiguration(struct soap* soap, struct _trt__AddAudioOutputConfiguration *trt__AddAudioOutputConfiguration, struct _trt__AddAudioOutputConfigurationResponse *trt__AddAudioOutputConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__AddAudioDecoderConfiguration(struct soap* soap, struct _trt__AddAudioDecoderConfiguration *trt__AddAudioDecoderConfiguration, struct _trt__AddAudioDecoderConfigurationResponse *trt__AddAudioDecoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveVideoEncoderConfiguration(struct soap* soap, struct _trt__RemoveVideoEncoderConfiguration *trt__RemoveVideoEncoderConfiguration, struct _trt__RemoveVideoEncoderConfigurationResponse *trt__RemoveVideoEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveVideoSourceConfiguration(struct soap* soap, struct _trt__RemoveVideoSourceConfiguration *trt__RemoveVideoSourceConfiguration, struct _trt__RemoveVideoSourceConfigurationResponse *trt__RemoveVideoSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveAudioEncoderConfiguration(struct soap* soap, struct _trt__RemoveAudioEncoderConfiguration *trt__RemoveAudioEncoderConfiguration, struct _trt__RemoveAudioEncoderConfigurationResponse *trt__RemoveAudioEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveAudioSourceConfiguration(struct soap* soap, struct _trt__RemoveAudioSourceConfiguration *trt__RemoveAudioSourceConfiguration, struct _trt__RemoveAudioSourceConfigurationResponse *trt__RemoveAudioSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__RemovePTZConfiguration(struct soap* soap, struct _trt__RemovePTZConfiguration *trt__RemovePTZConfiguration, struct _trt__RemovePTZConfigurationResponse *trt__RemovePTZConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveVideoAnalyticsConfiguration(struct soap* soap, struct _trt__RemoveVideoAnalyticsConfiguration *trt__RemoveVideoAnalyticsConfiguration, struct _trt__RemoveVideoAnalyticsConfigurationResponse *trt__RemoveVideoAnalyticsConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveMetadataConfiguration(struct soap* soap, struct _trt__RemoveMetadataConfiguration *trt__RemoveMetadataConfiguration, struct _trt__RemoveMetadataConfigurationResponse *trt__RemoveMetadataConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveAudioOutputConfiguration(struct soap* soap, struct _trt__RemoveAudioOutputConfiguration *trt__RemoveAudioOutputConfiguration, struct _trt__RemoveAudioOutputConfigurationResponse *trt__RemoveAudioOutputConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveAudioDecoderConfiguration(struct soap* soap, struct _trt__RemoveAudioDecoderConfiguration *trt__RemoveAudioDecoderConfiguration, struct _trt__RemoveAudioDecoderConfigurationResponse *trt__RemoveAudioDecoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__DeleteProfile(struct soap* soap, struct _trt__DeleteProfile *trt__DeleteProfile, struct _trt__DeleteProfileResponse *trt__DeleteProfileResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoSourceConfigurations(struct soap* soap, struct _trt__GetVideoSourceConfigurations *trt__GetVideoSourceConfigurations, struct _trt__GetVideoSourceConfigurationsResponse *trt__GetVideoSourceConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoEncoderConfigurations(struct soap* soap, struct _trt__GetVideoEncoderConfigurations *trt__GetVideoEncoderConfigurations, struct _trt__GetVideoEncoderConfigurationsResponse *trt__GetVideoEncoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioSourceConfigurations(struct soap* soap, struct _trt__GetAudioSourceConfigurations *trt__GetAudioSourceConfigurations, struct _trt__GetAudioSourceConfigurationsResponse *trt__GetAudioSourceConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioEncoderConfigurations(struct soap* soap, struct _trt__GetAudioEncoderConfigurations *trt__GetAudioEncoderConfigurations, struct _trt__GetAudioEncoderConfigurationsResponse *trt__GetAudioEncoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoAnalyticsConfigurations(struct soap* soap, struct _trt__GetVideoAnalyticsConfigurations *trt__GetVideoAnalyticsConfigurations, struct _trt__GetVideoAnalyticsConfigurationsResponse *trt__GetVideoAnalyticsConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetMetadataConfigurations(struct soap* soap, struct _trt__GetMetadataConfigurations *trt__GetMetadataConfigurations, struct _trt__GetMetadataConfigurationsResponse *trt__GetMetadataConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioOutputConfigurations(struct soap* soap, struct _trt__GetAudioOutputConfigurations *trt__GetAudioOutputConfigurations, struct _trt__GetAudioOutputConfigurationsResponse *trt__GetAudioOutputConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioDecoderConfigurations(struct soap* soap, struct _trt__GetAudioDecoderConfigurations *trt__GetAudioDecoderConfigurations, struct _trt__GetAudioDecoderConfigurationsResponse *trt__GetAudioDecoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoSourceConfiguration(struct soap* soap, struct _trt__GetVideoSourceConfiguration *trt__GetVideoSourceConfiguration, struct _trt__GetVideoSourceConfigurationResponse *trt__GetVideoSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoEncoderConfiguration(struct soap* soap, struct _trt__GetVideoEncoderConfiguration *trt__GetVideoEncoderConfiguration, struct _trt__GetVideoEncoderConfigurationResponse *trt__GetVideoEncoderConfigurationResponse)
{
	DPRINTK("%s in \n",__FUNCTION__);
	
	int i;
	int num_tokens = 0;
	NET_IPV4 ip;
	char _IPAddr[INFO_LENGTH];
	ip.int32 = net_get_ifaddr(ETH_NAME);
	sprintf(_IPAddr, "%d.%d.%d.%d", ip.str[0], ip.str[1], ip.str[2], ip.str[3]); 
	int Ptoken_exist = NOT_EXIST;

	get_ipcam_all_info();
	g_pIpcamAll.onvif_st.__sizeProfiles = 2;
	for(i = 0; i < g_pIpcamAll.onvif_st.__sizeProfiles; i++)
	{
		DPRINTK("%s->%s\n",trt__GetVideoEncoderConfiguration->ConfigurationToken, 
			g_pIpcamAll.onvif_st.vcode[i].token);
		if(strcmp(trt__GetVideoEncoderConfiguration->ConfigurationToken, g_pIpcamAll.onvif_st.vcode[i].token) == 0)
		{
			num_tokens++;
		}
	}

	for(i = 0; i < g_pIpcamAll.onvif_st.__sizeProfiles; i++)
	{
		DPRINTK("%s->%s\n",trt__GetVideoEncoderConfiguration->ConfigurationToken, 
			g_pIpcamAll.onvif_st.vcode[i].token);
	
		if(strcmp(trt__GetVideoEncoderConfiguration->ConfigurationToken, g_pIpcamAll.onvif_st.vcode[i].token) == 0)
		{
					
			Ptoken_exist = EXIST;
			break;
		}
	}
	/* if ConfigurationToken does not exist */
	if(!Ptoken_exist) 
	{
		onvif_fault(soap,"ter:InvalidArgVal", "ter:NoConfig");
		return SOAP_FAULT;
	}
	trt__GetVideoEncoderConfigurationResponse->Configuration = (struct tt__VideoEncoderConfiguration *)soap_malloc(soap, sizeof(struct tt__VideoEncoderConfiguration));
	soap_default_tt__VideoEncoderConfiguration(soap,trt__GetVideoEncoderConfigurationResponse->Configuration);
	trt__GetVideoEncoderConfigurationResponse->Configuration->Name =g_pIpcamAll.onvif_st.vcode[i].Name;
	trt__GetVideoEncoderConfigurationResponse->Configuration->UseCount = num_tokens;//oSysInfo->Oprofile[i].AESC.VEusercount;
	trt__GetVideoEncoderConfigurationResponse->Configuration->token = g_pIpcamAll.onvif_st.vcode[i].token;
	trt__GetVideoEncoderConfigurationResponse->Configuration->Encoding = g_pIpcamAll.onvif_st.vcode[i].Encoding;//JPEG = 0, MPEG4 = 1, H264 = 2;
	trt__GetVideoEncoderConfigurationResponse->Configuration->Resolution = (struct tt__VideoResolution *)soap_malloc(soap, sizeof(struct tt__VideoResolution));
	trt__GetVideoEncoderConfigurationResponse->Configuration->Resolution->Width = g_pIpcamAll.onvif_st.vcode[i].w;
	trt__GetVideoEncoderConfigurationResponse->Configuration->Resolution->Height = g_pIpcamAll.onvif_st.vcode[i].h;
	trt__GetVideoEncoderConfigurationResponse->Configuration->Quality = g_pIpcamAll.onvif_st.vcode[i].Quality; 
	trt__GetVideoEncoderConfigurationResponse->Configuration->RateControl = (struct tt__VideoRateControl *)soap_malloc(soap, sizeof(struct tt__VideoRateControl));
	trt__GetVideoEncoderConfigurationResponse->Configuration->RateControl->FrameRateLimit = g_pIpcamAll.onvif_st.vcode[i].FrameRateLimit;   
	trt__GetVideoEncoderConfigurationResponse->Configuration->RateControl->EncodingInterval = g_pIpcamAll.onvif_st.vcode[i].EncodingInterval;
	trt__GetVideoEncoderConfigurationResponse->Configuration->RateControl->BitrateLimit = g_pIpcamAll.onvif_st.vcode[i].BitrateLimit;
	trt__GetVideoEncoderConfigurationResponse->Configuration->MPEG4 = (struct tt__Mpeg4Configuration *)soap_malloc(soap, sizeof(struct tt__Mpeg4Configuration));
	trt__GetVideoEncoderConfigurationResponse->Configuration->MPEG4->GovLength = g_pIpcamAll.onvif_st.vcode[i].GovLength;
	trt__GetVideoEncoderConfigurationResponse->Configuration->MPEG4->Mpeg4Profile = 0;//SP = 0, ASP = 1;
	trt__GetVideoEncoderConfigurationResponse->Configuration->H264 = (struct tt__H264Configuration *)soap_malloc(soap, sizeof(struct tt__H264Configuration));
	trt__GetVideoEncoderConfigurationResponse->Configuration->H264->GovLength = g_pIpcamAll.onvif_st.vcode[i].GovLength;
	trt__GetVideoEncoderConfigurationResponse->Configuration->H264->H264Profile = 1;//Baseline = 0, Main = 1, High = 3
	trt__GetVideoEncoderConfigurationResponse->Configuration->Multicast = (struct tt__MulticastConfiguration *)soap_malloc(soap, sizeof(struct tt__MulticastConfiguration));
	soap_default_tt__MulticastConfiguration(soap,trt__GetVideoEncoderConfigurationResponse->Configuration->Multicast);
	trt__GetVideoEncoderConfigurationResponse->Configuration->Multicast->Port = 0;
	trt__GetVideoEncoderConfigurationResponse->Configuration->Multicast->TTL = 0;
	trt__GetVideoEncoderConfigurationResponse->Configuration->Multicast->AutoStart = _true;
	trt__GetVideoEncoderConfigurationResponse->Configuration->Multicast->Address  = (struct tt__IPAddress *)soap_malloc(soap, sizeof(struct tt__IPAddress));
	soap_default_tt__IPAddress(soap,trt__GetVideoEncoderConfigurationResponse->Configuration->Multicast->Address);
	trt__GetVideoEncoderConfigurationResponse->Configuration->Multicast->Address->Type = tt__IPType__IPv4;
	trt__GetVideoEncoderConfigurationResponse->Configuration->Multicast->Address->IPv4Address = (char **)soap_malloc(soap, sizeof(char *));
	trt__GetVideoEncoderConfigurationResponse->Configuration->Multicast->Address->IPv4Address[0] = (char *)soap_malloc(soap, sizeof(char) * INFO_LENGTH);
	strcpy(*trt__GetVideoEncoderConfigurationResponse->Configuration->Multicast->Address->IPv4Address,_IPAddr);
	trt__GetVideoEncoderConfigurationResponse->Configuration->Multicast->Address->IPv6Address = NULL;
	trt__GetVideoEncoderConfigurationResponse->Configuration->SessionTimeout = (char *)soap_malloc(soap, sizeof(char) * INFO_LENGTH);
	strcpy(trt__GetVideoEncoderConfigurationResponse->Configuration->SessionTimeout  , "PT0H12M0S");

	DPRINTK("%s out \n",__FUNCTION__);
	return SOAP_OK;
	
}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioSourceConfiguration(struct soap* soap, struct _trt__GetAudioSourceConfiguration *trt__GetAudioSourceConfiguration, struct _trt__GetAudioSourceConfigurationResponse *trt__GetAudioSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioEncoderConfiguration(struct soap* soap, struct _trt__GetAudioEncoderConfiguration *trt__GetAudioEncoderConfiguration, struct _trt__GetAudioEncoderConfigurationResponse *trt__GetAudioEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoAnalyticsConfiguration(struct soap* soap, struct _trt__GetVideoAnalyticsConfiguration *trt__GetVideoAnalyticsConfiguration, struct _trt__GetVideoAnalyticsConfigurationResponse *trt__GetVideoAnalyticsConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetMetadataConfiguration(struct soap* soap, struct _trt__GetMetadataConfiguration *trt__GetMetadataConfiguration, struct _trt__GetMetadataConfigurationResponse *trt__GetMetadataConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioOutputConfiguration(struct soap* soap, struct _trt__GetAudioOutputConfiguration *trt__GetAudioOutputConfiguration, struct _trt__GetAudioOutputConfigurationResponse *trt__GetAudioOutputConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioDecoderConfiguration(struct soap* soap, struct _trt__GetAudioDecoderConfiguration *trt__GetAudioDecoderConfiguration, struct _trt__GetAudioDecoderConfigurationResponse *trt__GetAudioDecoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleVideoEncoderConfigurations(struct soap* soap, struct _trt__GetCompatibleVideoEncoderConfigurations *trt__GetCompatibleVideoEncoderConfigurations, struct _trt__GetCompatibleVideoEncoderConfigurationsResponse *trt__GetCompatibleVideoEncoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleVideoSourceConfigurations(struct soap* soap, struct _trt__GetCompatibleVideoSourceConfigurations *trt__GetCompatibleVideoSourceConfigurations, struct _trt__GetCompatibleVideoSourceConfigurationsResponse *trt__GetCompatibleVideoSourceConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleAudioEncoderConfigurations(struct soap* soap, struct _trt__GetCompatibleAudioEncoderConfigurations *trt__GetCompatibleAudioEncoderConfigurations, struct _trt__GetCompatibleAudioEncoderConfigurationsResponse *trt__GetCompatibleAudioEncoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleAudioSourceConfigurations(struct soap* soap, struct _trt__GetCompatibleAudioSourceConfigurations *trt__GetCompatibleAudioSourceConfigurations, struct _trt__GetCompatibleAudioSourceConfigurationsResponse *trt__GetCompatibleAudioSourceConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleVideoAnalyticsConfigurations(struct soap* soap, struct _trt__GetCompatibleVideoAnalyticsConfigurations *trt__GetCompatibleVideoAnalyticsConfigurations, struct _trt__GetCompatibleVideoAnalyticsConfigurationsResponse *trt__GetCompatibleVideoAnalyticsConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleMetadataConfigurations(struct soap* soap, struct _trt__GetCompatibleMetadataConfigurations *trt__GetCompatibleMetadataConfigurations, struct _trt__GetCompatibleMetadataConfigurationsResponse *trt__GetCompatibleMetadataConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleAudioOutputConfigurations(struct soap* soap, struct _trt__GetCompatibleAudioOutputConfigurations *trt__GetCompatibleAudioOutputConfigurations, struct _trt__GetCompatibleAudioOutputConfigurationsResponse *trt__GetCompatibleAudioOutputConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleAudioDecoderConfigurations(struct soap* soap, struct _trt__GetCompatibleAudioDecoderConfigurations *trt__GetCompatibleAudioDecoderConfigurations, struct _trt__GetCompatibleAudioDecoderConfigurationsResponse *trt__GetCompatibleAudioDecoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__SetVideoSourceConfiguration(struct soap* soap, struct _trt__SetVideoSourceConfiguration *trt__SetVideoSourceConfiguration, struct _trt__SetVideoSourceConfigurationResponse *trt__SetVideoSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__SetVideoEncoderConfiguration(struct soap* soap, struct _trt__SetVideoEncoderConfiguration *trt__SetVideoEncoderConfiguration, struct _trt__SetVideoEncoderConfigurationResponse *trt__SetVideoEncoderConfigurationResponse)
{
	 int i=0;
	int Ptoken_exist = NOT_EXIST;
	int ret;
        __u8 codec_combo = 0;
        __u8 codec_res = 0;
        __u8 mode = 1;
        __u8 encodinginterval = 0;
        __u8 frameratelimit = 0;
        int _width = 0;
        int _height = 0;
        int bitrate = 0;
	int govlength = 0;	
	
	if(trt__SetVideoEncoderConfiguration->Configuration != NULL)
	{
		if(trt__SetVideoEncoderConfiguration->Configuration->Resolution != NULL)
		{
			_width = trt__SetVideoEncoderConfiguration->Configuration->Resolution->Width;
			_height = trt__SetVideoEncoderConfiguration->Configuration->Resolution->Height;
		}
	}
	if(trt__SetVideoEncoderConfiguration->Configuration->RateControl != NULL)
	{
		encodinginterval = trt__SetVideoEncoderConfiguration->Configuration->RateControl->EncodingInterval;
		bitrate = trt__SetVideoEncoderConfiguration->Configuration->RateControl->BitrateLimit;
                bitrate *= 1000;
		frameratelimit = trt__SetVideoEncoderConfiguration->Configuration->RateControl->FrameRateLimit;
	}
	for(i = 0; i <= g_pIpcamAll.onvif_st.__sizeProfiles; i++)
        {
		if(strcmp(trt__SetVideoEncoderConfiguration->Configuration->token,g_pIpcamAll.onvif_st.profile_st[i].profiletoken)==0)
              {
	      		Ptoken_exist = EXIST;
			break;
		}

		if(strcmp(trt__SetVideoEncoderConfiguration->Configuration->token,g_pIpcamAll.onvif_st.vcode[i].token)==0)
              {
	      		Ptoken_exist = EXIST;
			break;
		}
	}
	/* if ConfigurationToken does not exist */
	if(!Ptoken_exist) 
	{
		onvif_fault(soap,"ter:InvalidArgVal", "ter:NoVideoSource");
		return SOAP_FAULT;
	}	
	
   
	if(trt__SetVideoEncoderConfiguration->Configuration->H264 != NULL)
	{
		govlength = trt__SetVideoEncoderConfiguration->Configuration->H264->GovLength;		
	}else
	{
		govlength = frameratelimit;
	}

	
	{
		/*
		NET_COMMAND g_cmd;

		memset(&g_cmd,0,sizeof(NET_COMMAND));
		
		g_cmd.command = IPCAM_CMD_SET_ENCODE;
		g_cmd.length = i;
		g_cmd.play_fileId = bitrate/1000;
		g_cmd.page = frameratelimit + (govlength*frameratelimit) * 100;

		Net_dvr_send_cam_data(&g_cmd,sizeof(g_cmd),pCommuniteToOnvif);
		*/

		 get_ipcam_all_info();
		
		g_pIpcamAll.onvif_st.vcode[i].BitrateLimit = bitrate/1000;
		g_pIpcamAll.onvif_st.vcode[i].GovLength = govlength;
		g_pIpcamAll.onvif_st.vcode[i].FrameRateLimit = frameratelimit;

		DPRINTK("[%d] set %d,%d,%d\n",i,g_pIpcamAll.onvif_st.vcode[i].BitrateLimit,
			frameratelimit,govlength);
		
		IpcamSetValue((void*)&g_pIpcamAll,IPCAM_CODE_SET_PARAMETERS);
		IpcamSetValue((void*)&g_pIpcamAll,IPCAM_CMD_SAVE_ALLINFO);		

	}
	
		
	
        return SOAP_OK;

}

SOAP_FMAC5 int SOAP_FMAC6 __trt__SetAudioSourceConfiguration(struct soap* soap, struct _trt__SetAudioSourceConfiguration *trt__SetAudioSourceConfiguration, struct _trt__SetAudioSourceConfigurationResponse *trt__SetAudioSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__SetAudioEncoderConfiguration(struct soap* soap, struct _trt__SetAudioEncoderConfiguration *trt__SetAudioEncoderConfiguration, struct _trt__SetAudioEncoderConfigurationResponse *trt__SetAudioEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__SetVideoAnalyticsConfiguration(struct soap* soap, struct _trt__SetVideoAnalyticsConfiguration *trt__SetVideoAnalyticsConfiguration, struct _trt__SetVideoAnalyticsConfigurationResponse *trt__SetVideoAnalyticsConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__SetMetadataConfiguration(struct soap* soap, struct _trt__SetMetadataConfiguration *trt__SetMetadataConfiguration, struct _trt__SetMetadataConfigurationResponse *trt__SetMetadataConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__SetAudioOutputConfiguration(struct soap* soap, struct _trt__SetAudioOutputConfiguration *trt__SetAudioOutputConfiguration, struct _trt__SetAudioOutputConfigurationResponse *trt__SetAudioOutputConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__SetAudioDecoderConfiguration(struct soap* soap, struct _trt__SetAudioDecoderConfiguration *trt__SetAudioDecoderConfiguration, struct _trt__SetAudioDecoderConfigurationResponse *trt__SetAudioDecoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoSourceConfigurationOptions(struct soap* soap, struct _trt__GetVideoSourceConfigurationOptions *trt__GetVideoSourceConfigurationOptions, struct _trt__GetVideoSourceConfigurationOptionsResponse *trt__GetVideoSourceConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoEncoderConfigurationOptions(struct soap* soap, struct _trt__GetVideoEncoderConfigurationOptions *trt__GetVideoEncoderConfigurationOptions, struct _trt__GetVideoEncoderConfigurationOptionsResponse *trt__GetVideoEncoderConfigurationOptionsResponse)
{
	int i;
	char JPEG_profile;
	char MPEG4_profile;
	char H264_profile;

	DPRINTK(" in \n");
	
	trt__GetVideoEncoderConfigurationOptionsResponse->Options  = (struct tt__VideoEncoderConfigurationOptions *)soap_malloc(soap, sizeof(struct tt__VideoEncoderConfigurationOptions)); 
	soap_default_tt__VideoEncoderConfigurationOptions(soap,trt__GetVideoEncoderConfigurationOptionsResponse->Options );
	trt__GetVideoEncoderConfigurationOptionsResponse->Options->JPEG  = NULL;
	trt__GetVideoEncoderConfigurationOptionsResponse->Options->MPEG4 = NULL;
	trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264  = NULL;
    trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension  = NULL;

    trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension = (struct  tt__VideoEncoderOptionsExtension *)soap_malloc(soap, sizeof(struct tt__VideoEncoderOptionsExtension));
	soap_default_tt__VideoEncoderOptionsExtension(soap,trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension );
	trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension->JPEG  = NULL;
    trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension->H264  = NULL;
    trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension->MPEG4  = NULL;

	trt__GetVideoEncoderConfigurationOptionsResponse->Options->QualityRange  = (struct tt__IntRange *)soap_malloc(soap, sizeof(struct tt__IntRange)); 
	soap_default_tt__IntRange(soap,trt__GetVideoEncoderConfigurationOptionsResponse->Options->QualityRange );
	trt__GetVideoEncoderConfigurationOptionsResponse->Options->QualityRange->Min = 10; //dummy
	trt__GetVideoEncoderConfigurationOptionsResponse->Options->QualityRange->Max = 100; //dummy

	char PToken[MID_INFO_LENGTH];
	char CToken[MID_INFO_LENGTH];

	if(trt__GetVideoEncoderConfigurationOptions->ProfileToken)
	{
		strcpy(PToken, trt__GetVideoEncoderConfigurationOptions->ProfileToken);
	}
	if(trt__GetVideoEncoderConfigurationOptions->ConfigurationToken)
	{
		strcpy(CToken, trt__GetVideoEncoderConfigurationOptions->ConfigurationToken);
	}

	get_ipcam_all_info();
	g_pIpcamAll.onvif_st.__sizeProfiles = 2;

	for(i = 0; i < g_pIpcamAll.onvif_st.__sizeProfiles; i++)
	{
		DPRINTK("%s %s -> %s\n",PToken, CToken,g_pIpcamAll.onvif_st.vcode[i].token);
		if((strcmp(CToken, g_pIpcamAll.onvif_st.vcode[i].token) == 0) || 
			(strcmp(PToken, g_pIpcamAll.onvif_st.profile_st[i].profiletoken) == 0))
		{			
			H264_profile = EXIST;			
		}
	}

	

	if(H264_profile )
	{
		DPRINTK("here 1\n");
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264  = (struct tt__H264Options *)soap_malloc(soap, sizeof(struct tt__H264Options)); 
		soap_default_tt__H264Options(soap,trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264 );
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->__sizeResolutionsAvailable = 2;	//720(1280x720) : D1(720x480) : SXVGA(1280x960) : 1080(1920x1080) : 720MAX60(1280x720)
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->ResolutionsAvailable  = (struct tt__VideoResolution *)soap_malloc(soap,2* sizeof(struct tt__VideoResolution)); 
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->ResolutionsAvailable[0].Width  = g_pIpcamAll.onvif_st.vcode[0].w; 
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->ResolutionsAvailable[0].Height = g_pIpcamAll.onvif_st.vcode[0].h; 
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->ResolutionsAvailable[1].Width  = g_pIpcamAll.onvif_st.vcode[1].w; 
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->ResolutionsAvailable[1].Height = g_pIpcamAll.onvif_st.vcode[1].h; 
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->GovLengthRange  = (struct tt__IntRange *)soap_malloc(soap, sizeof(struct tt__IntRange)); 
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->GovLengthRange->Min = 1; //dummy
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->GovLengthRange->Max = 30; //dummy
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->FrameRateRange  = (struct tt__IntRange *)soap_malloc(soap, sizeof(struct tt__IntRange)); 
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->FrameRateRange->Min = 8; //dummy
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->FrameRateRange->Max = 30; //dummy
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->EncodingIntervalRange  = (struct tt__IntRange *)soap_malloc(soap, sizeof(struct tt__IntRange)); 
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->EncodingIntervalRange->Min = 1; //dummy
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->EncodingIntervalRange->Max = 2; //dummy
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->__sizeH264ProfilesSupported = 2;
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->H264ProfilesSupported = (int *)soap_malloc(soap, 3 * sizeof(int));
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->H264ProfilesSupported[0] = 0; //Baseline = 0, Main = 1, High = 3}
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->H264ProfilesSupported[1] = 1;
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->H264ProfilesSupported[2] = 3;
		//ProfileIDC = 100 # Profile IDC (66=baseline, 77=main,
		//Z:\DM_365\IPNetCam\ipnc\av_capture\framework\alg\src\alg_vidEnc.c=pObj->params_h264.profileIdc = 100;

        	trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension->H264  = (struct tt__H264Options2 *)soap_malloc(soap, sizeof(struct tt__H264Options2)); 
		soap_default_tt__H264Options2(soap,trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension->H264 );
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension->H264->ResolutionsAvailable  = NULL;
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension->H264->GovLengthRange  = NULL;
		trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension->H264->FrameRateRange  = NULL;
        	trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension->H264->EncodingIntervalRange  = NULL;
	
        	trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension->H264->BitrateRange  = (struct tt__IntRange*)soap_malloc(soap, sizeof(struct tt__IntRange)); 
        	trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension->H264->BitrateRange->Min = 64; //dummy
        	trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension->H264->BitrateRange->Max = 12000; //dummy
	}
	DPRINTK("out \n");
	
	return SOAP_OK; 
}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioSourceConfigurationOptions(struct soap* soap, struct _trt__GetAudioSourceConfigurationOptions *trt__GetAudioSourceConfigurationOptions, struct _trt__GetAudioSourceConfigurationOptionsResponse *trt__GetAudioSourceConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioEncoderConfigurationOptions(struct soap* soap, struct _trt__GetAudioEncoderConfigurationOptions *trt__GetAudioEncoderConfigurationOptions, struct _trt__GetAudioEncoderConfigurationOptionsResponse *trt__GetAudioEncoderConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetMetadataConfigurationOptions(struct soap* soap, struct _trt__GetMetadataConfigurationOptions *trt__GetMetadataConfigurationOptions, struct _trt__GetMetadataConfigurationOptionsResponse *trt__GetMetadataConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioOutputConfigurationOptions(struct soap* soap, struct _trt__GetAudioOutputConfigurationOptions *trt__GetAudioOutputConfigurationOptions, struct _trt__GetAudioOutputConfigurationOptionsResponse *trt__GetAudioOutputConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioDecoderConfigurationOptions(struct soap* soap, struct _trt__GetAudioDecoderConfigurationOptions *trt__GetAudioDecoderConfigurationOptions, struct _trt__GetAudioDecoderConfigurationOptionsResponse *trt__GetAudioDecoderConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetGuaranteedNumberOfVideoEncoderInstances(struct soap* soap, struct _trt__GetGuaranteedNumberOfVideoEncoderInstances *trt__GetGuaranteedNumberOfVideoEncoderInstances, struct _trt__GetGuaranteedNumberOfVideoEncoderInstancesResponse *trt__GetGuaranteedNumberOfVideoEncoderInstancesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetStreamUri(struct soap* soap, struct _trt__GetStreamUri *trt__GetStreamUri, struct _trt__GetStreamUriResponse *trt__GetStreamUriResponse)
{	
	NET_IPV4 ip;
	ip.int32 = net_get_ifaddr(ETH_NAME);
	int i = 0;
	char _IPAddr[INFO_LENGTH];

	get_ipcam_all_info();
	g_pIpcamAll.onvif_st.__sizeProfiles = 2;

	for(i = 0; i < g_pIpcamAll.onvif_st.__sizeProfiles; i++)
	{
		DPRINTK(" %s -> %s\n",trt__GetStreamUri->ProfileToken, g_pIpcamAll.onvif_st.profile_st[i].profiletoken);
		
		if(strcmp(trt__GetStreamUri->ProfileToken, g_pIpcamAll.onvif_st.profile_st[i].profiletoken) != 0)
		{
			DPRINTK("__trt__GetStreamUri -> %d\n",i);
			continue;
		}
		else 
		{
			DPRINTK("__trt__GetStreamUri  -> %d\n",i);
			break;
		}
	}
	
	if(i == g_pIpcamAll.onvif_st.__sizeProfiles)
       {
                onvif_fault(soap,"ter:InvalidArgVal", "ter:NoProfile");
                return SOAP_FAULT;
       }
        else
        {
            sprintf(_IPAddr, "rtsp://%d.%d.%d.%d:%d/%s", ip.str[0], ip.str[1], ip.str[2], ip.str[3],
				g_pIpcamAll.rtsp_st.url_port[i],g_pIpcamAll.rtsp_st.stream_url[i]);
		
        }

	

        
        if(trt__GetStreamUri->StreamSetup != NULL)
        {
                if(trt__GetStreamUri->StreamSetup->Stream == 1)
                {
                        onvif_fault(soap,"ter:InvalidArgVal","ter:InvalidStreamSetup");
                        return SOAP_FAULT;
                }
        }

	/* Response */
	trt__GetStreamUriResponse->MediaUri = (struct tt__MediaUri *)soap_malloc(soap, sizeof(struct tt__MediaUri));
	soap_default_tt__MediaUri(soap,trt__GetStreamUriResponse->MediaUri );
	trt__GetStreamUriResponse->MediaUri->Uri = (char *)soap_malloc(soap, sizeof(char) * LARGE_INFO_LENGTH);
	strcpy(trt__GetStreamUriResponse->MediaUri->Uri,_IPAddr);
	trt__GetStreamUriResponse->MediaUri->InvalidAfterConnect = 0;
	trt__GetStreamUriResponse->MediaUri->InvalidAfterReboot = 0;
	trt__GetStreamUriResponse->MediaUri->Timeout = (char *)soap_malloc(soap, sizeof(char) * LARGE_INFO_LENGTH);
	strcpy(trt__GetStreamUriResponse->MediaUri->Timeout,"PT0H0M0.200S");

	

	return SOAP_OK; 
}

SOAP_FMAC5 int SOAP_FMAC6 __trt__StartMulticastStreaming(struct soap* soap, struct _trt__StartMulticastStreaming *trt__StartMulticastStreaming, struct _trt__StartMulticastStreamingResponse *trt__StartMulticastStreamingResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__StopMulticastStreaming(struct soap* soap, struct _trt__StopMulticastStreaming *trt__StopMulticastStreaming, struct _trt__StopMulticastStreamingResponse *trt__StopMulticastStreamingResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__SetSynchronizationPoint(struct soap* soap, struct _trt__SetSynchronizationPoint *trt__SetSynchronizationPoint, struct _trt__SetSynchronizationPointResponse *trt__SetSynchronizationPointResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetSnapshotUri(struct soap* soap, struct _trt__GetSnapshotUri *trt__GetSnapshotUri, struct _trt__GetSnapshotUriResponse *trt__GetSnapshotUriResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetServiceCapabilities_(struct soap* soap, struct _trt__GetServiceCapabilities *trt__GetServiceCapabilities, struct _trt__GetServiceCapabilitiesResponse *trt__GetServiceCapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoSources_(struct soap* soap, struct _trt__GetVideoSources *trt__GetVideoSources, struct _trt__GetVideoSourcesResponse *trt__GetVideoSourcesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioSources_(struct soap* soap, struct _trt__GetAudioSources *trt__GetAudioSources, struct _trt__GetAudioSourcesResponse *trt__GetAudioSourcesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioOutputs_(struct soap* soap, struct _trt__GetAudioOutputs *trt__GetAudioOutputs, struct _trt__GetAudioOutputsResponse *trt__GetAudioOutputsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__CreateProfile_(struct soap* soap, struct _trt__CreateProfile *trt__CreateProfile, struct _trt__CreateProfileResponse *trt__CreateProfileResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetProfile_(struct soap* soap, struct _trt__GetProfile *trt__GetProfile, struct _trt__GetProfileResponse *trt__GetProfileResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetProfiles_(struct soap* soap, struct _trt__GetProfiles *trt__GetProfiles, struct _trt__GetProfilesResponse *trt__GetProfilesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__AddVideoEncoderConfiguration_(struct soap* soap, struct _trt__AddVideoEncoderConfiguration *trt__AddVideoEncoderConfiguration, struct _trt__AddVideoEncoderConfigurationResponse *trt__AddVideoEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__AddVideoSourceConfiguration_(struct soap* soap, struct _trt__AddVideoSourceConfiguration *trt__AddVideoSourceConfiguration, struct _trt__AddVideoSourceConfigurationResponse *trt__AddVideoSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__AddAudioEncoderConfiguration_(struct soap* soap, struct _trt__AddAudioEncoderConfiguration *trt__AddAudioEncoderConfiguration, struct _trt__AddAudioEncoderConfigurationResponse *trt__AddAudioEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__AddAudioSourceConfiguration_(struct soap* soap, struct _trt__AddAudioSourceConfiguration *trt__AddAudioSourceConfiguration, struct _trt__AddAudioSourceConfigurationResponse *trt__AddAudioSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__AddPTZConfiguration_(struct soap* soap, struct _trt__AddPTZConfiguration *trt__AddPTZConfiguration, struct _trt__AddPTZConfigurationResponse *trt__AddPTZConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__AddVideoAnalyticsConfiguration_(struct soap* soap, struct _trt__AddVideoAnalyticsConfiguration *trt__AddVideoAnalyticsConfiguration, struct _trt__AddVideoAnalyticsConfigurationResponse *trt__AddVideoAnalyticsConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__AddMetadataConfiguration_(struct soap* soap, struct _trt__AddMetadataConfiguration *trt__AddMetadataConfiguration, struct _trt__AddMetadataConfigurationResponse *trt__AddMetadataConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__AddAudioOutputConfiguration_(struct soap* soap, struct _trt__AddAudioOutputConfiguration *trt__AddAudioOutputConfiguration, struct _trt__AddAudioOutputConfigurationResponse *trt__AddAudioOutputConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__AddAudioDecoderConfiguration_(struct soap* soap, struct _trt__AddAudioDecoderConfiguration *trt__AddAudioDecoderConfiguration, struct _trt__AddAudioDecoderConfigurationResponse *trt__AddAudioDecoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveVideoEncoderConfiguration_(struct soap* soap, struct _trt__RemoveVideoEncoderConfiguration *trt__RemoveVideoEncoderConfiguration, struct _trt__RemoveVideoEncoderConfigurationResponse *trt__RemoveVideoEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveVideoSourceConfiguration_(struct soap* soap, struct _trt__RemoveVideoSourceConfiguration *trt__RemoveVideoSourceConfiguration, struct _trt__RemoveVideoSourceConfigurationResponse *trt__RemoveVideoSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveAudioEncoderConfiguration_(struct soap* soap, struct _trt__RemoveAudioEncoderConfiguration *trt__RemoveAudioEncoderConfiguration, struct _trt__RemoveAudioEncoderConfigurationResponse *trt__RemoveAudioEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveAudioSourceConfiguration_(struct soap* soap, struct _trt__RemoveAudioSourceConfiguration *trt__RemoveAudioSourceConfiguration, struct _trt__RemoveAudioSourceConfigurationResponse *trt__RemoveAudioSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__RemovePTZConfiguration_(struct soap* soap, struct _trt__RemovePTZConfiguration *trt__RemovePTZConfiguration, struct _trt__RemovePTZConfigurationResponse *trt__RemovePTZConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveVideoAnalyticsConfiguration_(struct soap* soap, struct _trt__RemoveVideoAnalyticsConfiguration *trt__RemoveVideoAnalyticsConfiguration, struct _trt__RemoveVideoAnalyticsConfigurationResponse *trt__RemoveVideoAnalyticsConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveMetadataConfiguration_(struct soap* soap, struct _trt__RemoveMetadataConfiguration *trt__RemoveMetadataConfiguration, struct _trt__RemoveMetadataConfigurationResponse *trt__RemoveMetadataConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveAudioOutputConfiguration_(struct soap* soap, struct _trt__RemoveAudioOutputConfiguration *trt__RemoveAudioOutputConfiguration, struct _trt__RemoveAudioOutputConfigurationResponse *trt__RemoveAudioOutputConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveAudioDecoderConfiguration_(struct soap* soap, struct _trt__RemoveAudioDecoderConfiguration *trt__RemoveAudioDecoderConfiguration, struct _trt__RemoveAudioDecoderConfigurationResponse *trt__RemoveAudioDecoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__DeleteProfile_(struct soap* soap, struct _trt__DeleteProfile *trt__DeleteProfile, struct _trt__DeleteProfileResponse *trt__DeleteProfileResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoSourceConfigurations_(struct soap* soap, struct _trt__GetVideoSourceConfigurations *trt__GetVideoSourceConfigurations, struct _trt__GetVideoSourceConfigurationsResponse *trt__GetVideoSourceConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoEncoderConfigurations_(struct soap* soap, struct _trt__GetVideoEncoderConfigurations *trt__GetVideoEncoderConfigurations, struct _trt__GetVideoEncoderConfigurationsResponse *trt__GetVideoEncoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioSourceConfigurations_(struct soap* soap, struct _trt__GetAudioSourceConfigurations *trt__GetAudioSourceConfigurations, struct _trt__GetAudioSourceConfigurationsResponse *trt__GetAudioSourceConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioEncoderConfigurations_(struct soap* soap, struct _trt__GetAudioEncoderConfigurations *trt__GetAudioEncoderConfigurations, struct _trt__GetAudioEncoderConfigurationsResponse *trt__GetAudioEncoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoAnalyticsConfigurations_(struct soap* soap, struct _trt__GetVideoAnalyticsConfigurations *trt__GetVideoAnalyticsConfigurations, struct _trt__GetVideoAnalyticsConfigurationsResponse *trt__GetVideoAnalyticsConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetMetadataConfigurations_(struct soap* soap, struct _trt__GetMetadataConfigurations *trt__GetMetadataConfigurations, struct _trt__GetMetadataConfigurationsResponse *trt__GetMetadataConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioOutputConfigurations_(struct soap* soap, struct _trt__GetAudioOutputConfigurations *trt__GetAudioOutputConfigurations, struct _trt__GetAudioOutputConfigurationsResponse *trt__GetAudioOutputConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioDecoderConfigurations_(struct soap* soap, struct _trt__GetAudioDecoderConfigurations *trt__GetAudioDecoderConfigurations, struct _trt__GetAudioDecoderConfigurationsResponse *trt__GetAudioDecoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoSourceConfiguration_(struct soap* soap, struct _trt__GetVideoSourceConfiguration *trt__GetVideoSourceConfiguration, struct _trt__GetVideoSourceConfigurationResponse *trt__GetVideoSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoEncoderConfiguration_(struct soap* soap, struct _trt__GetVideoEncoderConfiguration *trt__GetVideoEncoderConfiguration, struct _trt__GetVideoEncoderConfigurationResponse *trt__GetVideoEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioSourceConfiguration_(struct soap* soap, struct _trt__GetAudioSourceConfiguration *trt__GetAudioSourceConfiguration, struct _trt__GetAudioSourceConfigurationResponse *trt__GetAudioSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioEncoderConfiguration_(struct soap* soap, struct _trt__GetAudioEncoderConfiguration *trt__GetAudioEncoderConfiguration, struct _trt__GetAudioEncoderConfigurationResponse *trt__GetAudioEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoAnalyticsConfiguration_(struct soap* soap, struct _trt__GetVideoAnalyticsConfiguration *trt__GetVideoAnalyticsConfiguration, struct _trt__GetVideoAnalyticsConfigurationResponse *trt__GetVideoAnalyticsConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetMetadataConfiguration_(struct soap* soap, struct _trt__GetMetadataConfiguration *trt__GetMetadataConfiguration, struct _trt__GetMetadataConfigurationResponse *trt__GetMetadataConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioOutputConfiguration_(struct soap* soap, struct _trt__GetAudioOutputConfiguration *trt__GetAudioOutputConfiguration, struct _trt__GetAudioOutputConfigurationResponse *trt__GetAudioOutputConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioDecoderConfiguration_(struct soap* soap, struct _trt__GetAudioDecoderConfiguration *trt__GetAudioDecoderConfiguration, struct _trt__GetAudioDecoderConfigurationResponse *trt__GetAudioDecoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleVideoEncoderConfigurations_(struct soap* soap, struct _trt__GetCompatibleVideoEncoderConfigurations *trt__GetCompatibleVideoEncoderConfigurations, struct _trt__GetCompatibleVideoEncoderConfigurationsResponse *trt__GetCompatibleVideoEncoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleVideoSourceConfigurations_(struct soap* soap, struct _trt__GetCompatibleVideoSourceConfigurations *trt__GetCompatibleVideoSourceConfigurations, struct _trt__GetCompatibleVideoSourceConfigurationsResponse *trt__GetCompatibleVideoSourceConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleAudioEncoderConfigurations_(struct soap* soap, struct _trt__GetCompatibleAudioEncoderConfigurations *trt__GetCompatibleAudioEncoderConfigurations, struct _trt__GetCompatibleAudioEncoderConfigurationsResponse *trt__GetCompatibleAudioEncoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleAudioSourceConfigurations_(struct soap* soap, struct _trt__GetCompatibleAudioSourceConfigurations *trt__GetCompatibleAudioSourceConfigurations, struct _trt__GetCompatibleAudioSourceConfigurationsResponse *trt__GetCompatibleAudioSourceConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleVideoAnalyticsConfigurations_(struct soap* soap, struct _trt__GetCompatibleVideoAnalyticsConfigurations *trt__GetCompatibleVideoAnalyticsConfigurations, struct _trt__GetCompatibleVideoAnalyticsConfigurationsResponse *trt__GetCompatibleVideoAnalyticsConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleMetadataConfigurations_(struct soap* soap, struct _trt__GetCompatibleMetadataConfigurations *trt__GetCompatibleMetadataConfigurations, struct _trt__GetCompatibleMetadataConfigurationsResponse *trt__GetCompatibleMetadataConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleAudioOutputConfigurations_(struct soap* soap, struct _trt__GetCompatibleAudioOutputConfigurations *trt__GetCompatibleAudioOutputConfigurations, struct _trt__GetCompatibleAudioOutputConfigurationsResponse *trt__GetCompatibleAudioOutputConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleAudioDecoderConfigurations_(struct soap* soap, struct _trt__GetCompatibleAudioDecoderConfigurations *trt__GetCompatibleAudioDecoderConfigurations, struct _trt__GetCompatibleAudioDecoderConfigurationsResponse *trt__GetCompatibleAudioDecoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__SetVideoSourceConfiguration_(struct soap* soap, struct _trt__SetVideoSourceConfiguration *trt__SetVideoSourceConfiguration, struct _trt__SetVideoSourceConfigurationResponse *trt__SetVideoSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__SetVideoEncoderConfiguration_(struct soap* soap, struct _trt__SetVideoEncoderConfiguration *trt__SetVideoEncoderConfiguration, struct _trt__SetVideoEncoderConfigurationResponse *trt__SetVideoEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__SetAudioSourceConfiguration_(struct soap* soap, struct _trt__SetAudioSourceConfiguration *trt__SetAudioSourceConfiguration, struct _trt__SetAudioSourceConfigurationResponse *trt__SetAudioSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__SetAudioEncoderConfiguration_(struct soap* soap, struct _trt__SetAudioEncoderConfiguration *trt__SetAudioEncoderConfiguration, struct _trt__SetAudioEncoderConfigurationResponse *trt__SetAudioEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__SetVideoAnalyticsConfiguration_(struct soap* soap, struct _trt__SetVideoAnalyticsConfiguration *trt__SetVideoAnalyticsConfiguration, struct _trt__SetVideoAnalyticsConfigurationResponse *trt__SetVideoAnalyticsConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__SetMetadataConfiguration_(struct soap* soap, struct _trt__SetMetadataConfiguration *trt__SetMetadataConfiguration, struct _trt__SetMetadataConfigurationResponse *trt__SetMetadataConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__SetAudioOutputConfiguration_(struct soap* soap, struct _trt__SetAudioOutputConfiguration *trt__SetAudioOutputConfiguration, struct _trt__SetAudioOutputConfigurationResponse *trt__SetAudioOutputConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__SetAudioDecoderConfiguration_(struct soap* soap, struct _trt__SetAudioDecoderConfiguration *trt__SetAudioDecoderConfiguration, struct _trt__SetAudioDecoderConfigurationResponse *trt__SetAudioDecoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoSourceConfigurationOptions_(struct soap* soap, struct _trt__GetVideoSourceConfigurationOptions *trt__GetVideoSourceConfigurationOptions, struct _trt__GetVideoSourceConfigurationOptionsResponse *trt__GetVideoSourceConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoEncoderConfigurationOptions_(struct soap* soap, struct _trt__GetVideoEncoderConfigurationOptions *trt__GetVideoEncoderConfigurationOptions, struct _trt__GetVideoEncoderConfigurationOptionsResponse *trt__GetVideoEncoderConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioSourceConfigurationOptions_(struct soap* soap, struct _trt__GetAudioSourceConfigurationOptions *trt__GetAudioSourceConfigurationOptions, struct _trt__GetAudioSourceConfigurationOptionsResponse *trt__GetAudioSourceConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioEncoderConfigurationOptions_(struct soap* soap, struct _trt__GetAudioEncoderConfigurationOptions *trt__GetAudioEncoderConfigurationOptions, struct _trt__GetAudioEncoderConfigurationOptionsResponse *trt__GetAudioEncoderConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetMetadataConfigurationOptions_(struct soap* soap, struct _trt__GetMetadataConfigurationOptions *trt__GetMetadataConfigurationOptions, struct _trt__GetMetadataConfigurationOptionsResponse *trt__GetMetadataConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioOutputConfigurationOptions_(struct soap* soap, struct _trt__GetAudioOutputConfigurationOptions *trt__GetAudioOutputConfigurationOptions, struct _trt__GetAudioOutputConfigurationOptionsResponse *trt__GetAudioOutputConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioDecoderConfigurationOptions_(struct soap* soap, struct _trt__GetAudioDecoderConfigurationOptions *trt__GetAudioDecoderConfigurationOptions, struct _trt__GetAudioDecoderConfigurationOptionsResponse *trt__GetAudioDecoderConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetGuaranteedNumberOfVideoEncoderInstances_(struct soap* soap, struct _trt__GetGuaranteedNumberOfVideoEncoderInstances *trt__GetGuaranteedNumberOfVideoEncoderInstances, struct _trt__GetGuaranteedNumberOfVideoEncoderInstancesResponse *trt__GetGuaranteedNumberOfVideoEncoderInstancesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetStreamUri_(struct soap* soap, struct _trt__GetStreamUri *trt__GetStreamUri, struct _trt__GetStreamUriResponse *trt__GetStreamUriResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__StartMulticastStreaming_(struct soap* soap, struct _trt__StartMulticastStreaming *trt__StartMulticastStreaming, struct _trt__StartMulticastStreamingResponse *trt__StartMulticastStreamingResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__StopMulticastStreaming_(struct soap* soap, struct _trt__StopMulticastStreaming *trt__StopMulticastStreaming, struct _trt__StopMulticastStreamingResponse *trt__StopMulticastStreamingResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__SetSynchronizationPoint_(struct soap* soap, struct _trt__SetSynchronizationPoint *trt__SetSynchronizationPoint, struct _trt__SetSynchronizationPointResponse *trt__SetSynchronizationPointResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trt__GetSnapshotUri_(struct soap* soap, struct _trt__GetSnapshotUri *trt__GetSnapshotUri, struct _trt__GetSnapshotUriResponse *trt__GetSnapshotUriResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trv__GetServiceCapabilities(struct soap* soap, struct _trv__GetServiceCapabilities *trv__GetServiceCapabilities, struct _trv__GetServiceCapabilitiesResponse *trv__GetServiceCapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trv__GetReceivers(struct soap* soap, struct _trv__GetReceivers *trv__GetReceivers, struct _trv__GetReceiversResponse *trv__GetReceiversResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trv__GetReceiver(struct soap* soap, struct _trv__GetReceiver *trv__GetReceiver, struct _trv__GetReceiverResponse *trv__GetReceiverResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trv__CreateReceiver(struct soap* soap, struct _trv__CreateReceiver *trv__CreateReceiver, struct _trv__CreateReceiverResponse *trv__CreateReceiverResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trv__DeleteReceiver(struct soap* soap, struct _trv__DeleteReceiver *trv__DeleteReceiver, struct _trv__DeleteReceiverResponse *trv__DeleteReceiverResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trv__ConfigureReceiver(struct soap* soap, struct _trv__ConfigureReceiver *trv__ConfigureReceiver, struct _trv__ConfigureReceiverResponse *trv__ConfigureReceiverResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trv__SetReceiverMode(struct soap* soap, struct _trv__SetReceiverMode *trv__SetReceiverMode, struct _trv__SetReceiverModeResponse *trv__SetReceiverModeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __trv__GetReceiverState(struct soap* soap, struct _trv__GetReceiverState *trv__GetReceiverState, struct _trv__GetReceiverStateResponse *trv__GetReceiverStateResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tse__GetServiceCapabilities(struct soap* soap, struct _tse__GetServiceCapabilities *tse__GetServiceCapabilities, struct _tse__GetServiceCapabilitiesResponse *tse__GetServiceCapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tse__GetRecordingSummary(struct soap* soap, struct _tse__GetRecordingSummary *tse__GetRecordingSummary, struct _tse__GetRecordingSummaryResponse *tse__GetRecordingSummaryResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tse__GetRecordingInformation(struct soap* soap, struct _tse__GetRecordingInformation *tse__GetRecordingInformation, struct _tse__GetRecordingInformationResponse *tse__GetRecordingInformationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tse__GetMediaAttributes(struct soap* soap, struct _tse__GetMediaAttributes *tse__GetMediaAttributes, struct _tse__GetMediaAttributesResponse *tse__GetMediaAttributesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tse__FindRecordings(struct soap* soap, struct _tse__FindRecordings *tse__FindRecordings, struct _tse__FindRecordingsResponse *tse__FindRecordingsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tse__GetRecordingSearchResults(struct soap* soap, struct _tse__GetRecordingSearchResults *tse__GetRecordingSearchResults, struct _tse__GetRecordingSearchResultsResponse *tse__GetRecordingSearchResultsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tse__FindEvents(struct soap* soap, struct _tse__FindEvents *tse__FindEvents, struct _tse__FindEventsResponse *tse__FindEventsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tse__GetEventSearchResults(struct soap* soap, struct _tse__GetEventSearchResults *tse__GetEventSearchResults, struct _tse__GetEventSearchResultsResponse *tse__GetEventSearchResultsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tse__FindPTZPosition(struct soap* soap, struct _tse__FindPTZPosition *tse__FindPTZPosition, struct _tse__FindPTZPositionResponse *tse__FindPTZPositionResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tse__GetPTZPositionSearchResults(struct soap* soap, struct _tse__GetPTZPositionSearchResults *tse__GetPTZPositionSearchResults, struct _tse__GetPTZPositionSearchResultsResponse *tse__GetPTZPositionSearchResultsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tse__GetSearchState(struct soap* soap, struct _tse__GetSearchState *tse__GetSearchState, struct _tse__GetSearchStateResponse *tse__GetSearchStateResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tse__EndSearch(struct soap* soap, struct _tse__EndSearch *tse__EndSearch, struct _tse__EndSearchResponse *tse__EndSearchResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tse__FindMetadata(struct soap* soap, struct _tse__FindMetadata *tse__FindMetadata, struct _tse__FindMetadataResponse *tse__FindMetadataResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tse__GetMetadataSearchResults(struct soap* soap, struct _tse__GetMetadataSearchResults *tse__GetMetadataSearchResults, struct _tse__GetMetadataSearchResultsResponse *tse__GetMetadataSearchResultsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}


SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoSourceConfigurations__(struct soap* soap, struct _trt__GetVideoSourceConfigurations *trt__GetVideoSourceConfigurations, struct _trt__GetVideoSourceConfigurationsResponse *trt__GetVideoSourceConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetVideoEncoderConfigurations__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoEncoderConfigurations__(struct soap* soap, struct _trt__GetVideoEncoderConfigurations *trt__GetVideoEncoderConfigurations, struct _trt__GetVideoEncoderConfigurationsResponse *trt__GetVideoEncoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetAudioSourceConfigurations__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioSourceConfigurations__(struct soap* soap, struct _trt__GetAudioSourceConfigurations *trt__GetAudioSourceConfigurations, struct _trt__GetAudioSourceConfigurationsResponse *trt__GetAudioSourceConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetAudioEncoderConfigurations__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioEncoderConfigurations__(struct soap* soap, struct _trt__GetAudioEncoderConfigurations *trt__GetAudioEncoderConfigurations, struct _trt__GetAudioEncoderConfigurationsResponse *trt__GetAudioEncoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetVideoAnalyticsConfigurations__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoAnalyticsConfigurations__(struct soap* soap, struct _trt__GetVideoAnalyticsConfigurations *trt__GetVideoAnalyticsConfigurations, struct _trt__GetVideoAnalyticsConfigurationsResponse *trt__GetVideoAnalyticsConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetMetadataConfigurations__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetMetadataConfigurations__(struct soap* soap, struct _trt__GetMetadataConfigurations *trt__GetMetadataConfigurations, struct _trt__GetMetadataConfigurationsResponse *trt__GetMetadataConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetAudioOutputConfigurations__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioOutputConfigurations__(struct soap* soap, struct _trt__GetAudioOutputConfigurations *trt__GetAudioOutputConfigurations, struct _trt__GetAudioOutputConfigurationsResponse *trt__GetAudioOutputConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetAudioDecoderConfigurations__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioDecoderConfigurations__(struct soap* soap, struct _trt__GetAudioDecoderConfigurations *trt__GetAudioDecoderConfigurations, struct _trt__GetAudioDecoderConfigurationsResponse *trt__GetAudioDecoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetVideoSourceConfiguration__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoSourceConfiguration__(struct soap* soap, struct _trt__GetVideoSourceConfiguration *trt__GetVideoSourceConfiguration, struct _trt__GetVideoSourceConfigurationResponse *trt__GetVideoSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetVideoEncoderConfiguration__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoEncoderConfiguration__(struct soap* soap, struct _trt__GetVideoEncoderConfiguration *trt__GetVideoEncoderConfiguration, struct _trt__GetVideoEncoderConfigurationResponse *trt__GetVideoEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetAudioSourceConfiguration__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioSourceConfiguration__(struct soap* soap, struct _trt__GetAudioSourceConfiguration *trt__GetAudioSourceConfiguration, struct _trt__GetAudioSourceConfigurationResponse *trt__GetAudioSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetAudioEncoderConfiguration__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioEncoderConfiguration__(struct soap* soap, struct _trt__GetAudioEncoderConfiguration *trt__GetAudioEncoderConfiguration, struct _trt__GetAudioEncoderConfigurationResponse *trt__GetAudioEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetVideoAnalyticsConfiguration__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoAnalyticsConfiguration__(struct soap* soap, struct _trt__GetVideoAnalyticsConfiguration *trt__GetVideoAnalyticsConfiguration, struct _trt__GetVideoAnalyticsConfigurationResponse *trt__GetVideoAnalyticsConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetMetadataConfiguration__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetMetadataConfiguration__(struct soap* soap, struct _trt__GetMetadataConfiguration *trt__GetMetadataConfiguration, struct _trt__GetMetadataConfigurationResponse *trt__GetMetadataConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetAudioOutputConfiguration__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioOutputConfiguration__(struct soap* soap, struct _trt__GetAudioOutputConfiguration *trt__GetAudioOutputConfiguration, struct _trt__GetAudioOutputConfigurationResponse *trt__GetAudioOutputConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetAudioDecoderConfiguration__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioDecoderConfiguration__(struct soap* soap, struct _trt__GetAudioDecoderConfiguration *trt__GetAudioDecoderConfiguration, struct _trt__GetAudioDecoderConfigurationResponse *trt__GetAudioDecoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetCompatibleVideoEncoderConfigurations__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleVideoEncoderConfigurations__(struct soap* soap, struct _trt__GetCompatibleVideoEncoderConfigurations *trt__GetCompatibleVideoEncoderConfigurations, struct _trt__GetCompatibleVideoEncoderConfigurationsResponse *trt__GetCompatibleVideoEncoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetCompatibleVideoSourceConfigurations__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleVideoSourceConfigurations__(struct soap* soap, struct _trt__GetCompatibleVideoSourceConfigurations *trt__GetCompatibleVideoSourceConfigurations, struct _trt__GetCompatibleVideoSourceConfigurationsResponse *trt__GetCompatibleVideoSourceConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetCompatibleAudioEncoderConfigurations__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleAudioEncoderConfigurations__(struct soap* soap, struct _trt__GetCompatibleAudioEncoderConfigurations *trt__GetCompatibleAudioEncoderConfigurations, struct _trt__GetCompatibleAudioEncoderConfigurationsResponse *trt__GetCompatibleAudioEncoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetCompatibleAudioSourceConfigurations__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleAudioSourceConfigurations__(struct soap* soap, struct _trt__GetCompatibleAudioSourceConfigurations *trt__GetCompatibleAudioSourceConfigurations, struct _trt__GetCompatibleAudioSourceConfigurationsResponse *trt__GetCompatibleAudioSourceConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetCompatibleVideoAnalyticsConfigurations__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleVideoAnalyticsConfigurations__(struct soap* soap, struct _trt__GetCompatibleVideoAnalyticsConfigurations *trt__GetCompatibleVideoAnalyticsConfigurations, struct _trt__GetCompatibleVideoAnalyticsConfigurationsResponse *trt__GetCompatibleVideoAnalyticsConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetCompatibleMetadataConfigurations__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleMetadataConfigurations__(struct soap* soap, struct _trt__GetCompatibleMetadataConfigurations *trt__GetCompatibleMetadataConfigurations, struct _trt__GetCompatibleMetadataConfigurationsResponse *trt__GetCompatibleMetadataConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetCompatibleAudioOutputConfigurations__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleAudioOutputConfigurations__(struct soap* soap, struct _trt__GetCompatibleAudioOutputConfigurations *trt__GetCompatibleAudioOutputConfigurations, struct _trt__GetCompatibleAudioOutputConfigurationsResponse *trt__GetCompatibleAudioOutputConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetCompatibleAudioDecoderConfigurations__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetCompatibleAudioDecoderConfigurations__(struct soap* soap, struct _trt__GetCompatibleAudioDecoderConfigurations *trt__GetCompatibleAudioDecoderConfigurations, struct _trt__GetCompatibleAudioDecoderConfigurationsResponse *trt__GetCompatibleAudioDecoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__SetVideoSourceConfiguration__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__SetVideoSourceConfiguration__(struct soap* soap, struct _trt__SetVideoSourceConfiguration *trt__SetVideoSourceConfiguration, struct _trt__SetVideoSourceConfigurationResponse *trt__SetVideoSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__SetVideoEncoderConfiguration__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__SetVideoEncoderConfiguration__(struct soap* soap, struct _trt__SetVideoEncoderConfiguration *trt__SetVideoEncoderConfiguration, struct _trt__SetVideoEncoderConfigurationResponse *trt__SetVideoEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__SetAudioSourceConfiguration__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__SetAudioSourceConfiguration__(struct soap* soap, struct _trt__SetAudioSourceConfiguration *trt__SetAudioSourceConfiguration, struct _trt__SetAudioSourceConfigurationResponse *trt__SetAudioSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__SetAudioEncoderConfiguration__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__SetAudioEncoderConfiguration__(struct soap* soap, struct _trt__SetAudioEncoderConfiguration *trt__SetAudioEncoderConfiguration, struct _trt__SetAudioEncoderConfigurationResponse *trt__SetAudioEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__SetVideoAnalyticsConfiguration__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__SetVideoAnalyticsConfiguration__(struct soap* soap, struct _trt__SetVideoAnalyticsConfiguration *trt__SetVideoAnalyticsConfiguration, struct _trt__SetVideoAnalyticsConfigurationResponse *trt__SetVideoAnalyticsConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__SetMetadataConfiguration__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__SetMetadataConfiguration__(struct soap* soap, struct _trt__SetMetadataConfiguration *trt__SetMetadataConfiguration, struct _trt__SetMetadataConfigurationResponse *trt__SetMetadataConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__SetAudioOutputConfiguration__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__SetAudioOutputConfiguration__(struct soap* soap, struct _trt__SetAudioOutputConfiguration *trt__SetAudioOutputConfiguration, struct _trt__SetAudioOutputConfigurationResponse *trt__SetAudioOutputConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__SetAudioDecoderConfiguration__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__SetAudioDecoderConfiguration__(struct soap* soap, struct _trt__SetAudioDecoderConfiguration *trt__SetAudioDecoderConfiguration, struct _trt__SetAudioDecoderConfigurationResponse *trt__SetAudioDecoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetVideoSourceConfigurationOptions__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoSourceConfigurationOptions__(struct soap* soap, struct _trt__GetVideoSourceConfigurationOptions *trt__GetVideoSourceConfigurationOptions, struct _trt__GetVideoSourceConfigurationOptionsResponse *trt__GetVideoSourceConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetVideoEncoderConfigurationOptions__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoEncoderConfigurationOptions__(struct soap* soap, struct _trt__GetVideoEncoderConfigurationOptions *trt__GetVideoEncoderConfigurationOptions, struct _trt__GetVideoEncoderConfigurationOptionsResponse *trt__GetVideoEncoderConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetAudioSourceConfigurationOptions__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioSourceConfigurationOptions__(struct soap* soap, struct _trt__GetAudioSourceConfigurationOptions *trt__GetAudioSourceConfigurationOptions, struct _trt__GetAudioSourceConfigurationOptionsResponse *trt__GetAudioSourceConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetAudioEncoderConfigurationOptions__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioEncoderConfigurationOptions__(struct soap* soap, struct _trt__GetAudioEncoderConfigurationOptions *trt__GetAudioEncoderConfigurationOptions, struct _trt__GetAudioEncoderConfigurationOptionsResponse *trt__GetAudioEncoderConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetMetadataConfigurationOptions__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetMetadataConfigurationOptions__(struct soap* soap, struct _trt__GetMetadataConfigurationOptions *trt__GetMetadataConfigurationOptions, struct _trt__GetMetadataConfigurationOptionsResponse *trt__GetMetadataConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetAudioOutputConfigurationOptions__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioOutputConfigurationOptions__(struct soap* soap, struct _trt__GetAudioOutputConfigurationOptions *trt__GetAudioOutputConfigurationOptions, struct _trt__GetAudioOutputConfigurationOptionsResponse *trt__GetAudioOutputConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetAudioDecoderConfigurationOptions__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioDecoderConfigurationOptions__(struct soap* soap, struct _trt__GetAudioDecoderConfigurationOptions *trt__GetAudioDecoderConfigurationOptions, struct _trt__GetAudioDecoderConfigurationOptionsResponse *trt__GetAudioDecoderConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetGuaranteedNumberOfVideoEncoderInstances__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetGuaranteedNumberOfVideoEncoderInstances__(struct soap* soap, struct _trt__GetGuaranteedNumberOfVideoEncoderInstances *trt__GetGuaranteedNumberOfVideoEncoderInstances, struct _trt__GetGuaranteedNumberOfVideoEncoderInstancesResponse *trt__GetGuaranteedNumberOfVideoEncoderInstancesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetStreamUri__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetStreamUri__(struct soap* soap, struct _trt__GetStreamUri *trt__GetStreamUri, struct _trt__GetStreamUriResponse *trt__GetStreamUriResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__StartMulticastStreaming__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__StartMulticastStreaming__(struct soap* soap, struct _trt__StartMulticastStreaming *trt__StartMulticastStreaming, struct _trt__StartMulticastStreamingResponse *trt__StartMulticastStreamingResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__StopMulticastStreaming__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__StopMulticastStreaming__(struct soap* soap, struct _trt__StopMulticastStreaming *trt__StopMulticastStreaming, struct _trt__StopMulticastStreamingResponse *trt__StopMulticastStreamingResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__SetSynchronizationPoint__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__SetSynchronizationPoint__(struct soap* soap, struct _trt__SetSynchronizationPoint *trt__SetSynchronizationPoint, struct _trt__SetSynchronizationPointResponse *trt__SetSynchronizationPointResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetSnapshotUri__' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetSnapshotUri__(struct soap* soap, struct _trt__GetSnapshotUri *trt__GetSnapshotUri, struct _trt__GetSnapshotUriResponse *trt__GetSnapshotUriResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__GetVideoSourceModes' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoSourceModes(struct soap* soap, struct _trt__GetVideoSourceModes *trt__GetVideoSourceModes, struct _trt__GetVideoSourceModesResponse *trt__GetVideoSourceModesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  /** Web service operation '__trt__SetVideoSourceMode' (returns SOAP_OK or error code) */
  SOAP_FMAC5 int SOAP_FMAC6 __trt__SetVideoSourceMode(struct soap* soap, struct _trt__SetVideoSourceMode *trt__SetVideoSourceMode, struct _trt__SetVideoSourceModeResponse *trt__SetVideoSourceModeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}



SOAP_FMAC5 int SOAP_FMAC6 __trt__GetOSDs(struct soap* soap, struct _trt__GetOSDs *trt__GetOSDs, struct _trt__GetOSDsResponse *trt__GetOSDsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

 SOAP_FMAC5 int SOAP_FMAC6 __trt__GetOSD(struct soap* soap, struct _trt__GetOSD *trt__GetOSD, struct _trt__GetOSDResponse *trt__GetOSDResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

 SOAP_FMAC5 int SOAP_FMAC6 __trt__GetOSDOptions(struct soap* soap, struct _trt__GetOSDOptions *trt__GetOSDOptions, struct _trt__GetOSDOptionsResponse *trt__GetOSDOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

 SOAP_FMAC5 int SOAP_FMAC6 __trt__SetOSD(struct soap* soap, struct _trt__SetOSD *trt__SetOSD, struct _trt__SetOSDResponse *trt__SetOSDResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

 SOAP_FMAC5 int SOAP_FMAC6 __trt__CreateOSD(struct soap* soap, struct _trt__CreateOSD *trt__CreateOSD, struct _trt__CreateOSDResponse *trt__CreateOSDResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

 SOAP_FMAC5 int SOAP_FMAC6 __trt__DeleteOSD(struct soap* soap, struct _trt__DeleteOSD *trt__DeleteOSD, struct _trt__DeleteOSDResponse *trt__DeleteOSDResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}


SOAP_FMAC5 int SOAP_FMAC6 __trt__AddVideoEncoderConfiguration__(struct soap* soap,  struct _trt__AddVideoEncoderConfiguration *trt__AddVideoEncoderConfiguration, struct _trt__AddVideoEncoderConfigurationResponse *trt__AddVideoEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__trt__AddVideoSourceConfiguration__' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __trt__AddVideoSourceConfiguration__(struct soap* soap,  struct _trt__AddVideoSourceConfiguration *trt__AddVideoSourceConfiguration, struct _trt__AddVideoSourceConfigurationResponse *trt__AddVideoSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__trt__AddAudioEncoderConfiguration__' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __trt__AddAudioEncoderConfiguration__(struct soap* soap,  struct _trt__AddAudioEncoderConfiguration *trt__AddAudioEncoderConfiguration, struct _trt__AddAudioEncoderConfigurationResponse *trt__AddAudioEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__trt__AddAudioSourceConfiguration__' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __trt__AddAudioSourceConfiguration__(struct soap* soap,  struct _trt__AddAudioSourceConfiguration *trt__AddAudioSourceConfiguration, struct _trt__AddAudioSourceConfigurationResponse *trt__AddAudioSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__trt__AddPTZConfiguration__' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __trt__AddPTZConfiguration__(struct soap* soap,  struct _trt__AddPTZConfiguration *trt__AddPTZConfiguration, struct _trt__AddPTZConfigurationResponse *trt__AddPTZConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__trt__AddVideoAnalyticsConfiguration__' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __trt__AddVideoAnalyticsConfiguration__(struct soap* soap,  struct _trt__AddVideoAnalyticsConfiguration *trt__AddVideoAnalyticsConfiguration, struct _trt__AddVideoAnalyticsConfigurationResponse *trt__AddVideoAnalyticsConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__trt__AddMetadataConfiguration__' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __trt__AddMetadataConfiguration__(struct soap* soap,  struct _trt__AddMetadataConfiguration *trt__AddMetadataConfiguration, struct _trt__AddMetadataConfigurationResponse *trt__AddMetadataConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__trt__AddAudioOutputConfiguration__' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __trt__AddAudioOutputConfiguration__(struct soap* soap,  struct _trt__AddAudioOutputConfiguration *trt__AddAudioOutputConfiguration, struct _trt__AddAudioOutputConfigurationResponse *trt__AddAudioOutputConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__trt__AddAudioDecoderConfiguration__' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __trt__AddAudioDecoderConfiguration__(struct soap* soap,  struct _trt__AddAudioDecoderConfiguration *trt__AddAudioDecoderConfiguration, struct _trt__AddAudioDecoderConfigurationResponse *trt__AddAudioDecoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__trt__RemoveVideoEncoderConfiguration__' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveVideoEncoderConfiguration__(struct soap* soap,  struct _trt__RemoveVideoEncoderConfiguration *trt__RemoveVideoEncoderConfiguration, struct _trt__RemoveVideoEncoderConfigurationResponse *trt__RemoveVideoEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__trt__RemoveVideoSourceConfiguration__' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveVideoSourceConfiguration__(struct soap* soap,  struct _trt__RemoveVideoSourceConfiguration *trt__RemoveVideoSourceConfiguration, struct _trt__RemoveVideoSourceConfigurationResponse *trt__RemoveVideoSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__trt__RemoveAudioEncoderConfiguration__' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveAudioEncoderConfiguration__(struct soap* soap,  struct _trt__RemoveAudioEncoderConfiguration *trt__RemoveAudioEncoderConfiguration, struct _trt__RemoveAudioEncoderConfigurationResponse *trt__RemoveAudioEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__trt__RemoveAudioSourceConfiguration__' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveAudioSourceConfiguration__(struct soap* soap,  struct _trt__RemoveAudioSourceConfiguration *trt__RemoveAudioSourceConfiguration, struct _trt__RemoveAudioSourceConfigurationResponse *trt__RemoveAudioSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__trt__RemovePTZConfiguration__' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __trt__RemovePTZConfiguration__(struct soap* soap,  struct _trt__RemovePTZConfiguration *trt__RemovePTZConfiguration, struct _trt__RemovePTZConfigurationResponse *trt__RemovePTZConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__trt__RemoveVideoAnalyticsConfiguration__' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveVideoAnalyticsConfiguration__(struct soap* soap,  struct _trt__RemoveVideoAnalyticsConfiguration *trt__RemoveVideoAnalyticsConfiguration, struct _trt__RemoveVideoAnalyticsConfigurationResponse *trt__RemoveVideoAnalyticsConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__trt__RemoveMetadataConfiguration__' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveMetadataConfiguration__(struct soap* soap,  struct _trt__RemoveMetadataConfiguration *trt__RemoveMetadataConfiguration, struct _trt__RemoveMetadataConfigurationResponse *trt__RemoveMetadataConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__trt__RemoveAudioOutputConfiguration__' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveAudioOutputConfiguration__(struct soap* soap,  struct _trt__RemoveAudioOutputConfiguration *trt__RemoveAudioOutputConfiguration, struct _trt__RemoveAudioOutputConfigurationResponse *trt__RemoveAudioOutputConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__trt__RemoveAudioDecoderConfiguration__' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __trt__RemoveAudioDecoderConfiguration__(struct soap* soap,  struct _trt__RemoveAudioDecoderConfiguration *trt__RemoveAudioDecoderConfiguration, struct _trt__RemoveAudioDecoderConfigurationResponse *trt__RemoveAudioDecoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__trt__DeleteProfile__' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __trt__DeleteProfile__(struct soap* soap,  struct _trt__DeleteProfile *trt__DeleteProfile, struct _trt__DeleteProfileResponse *trt__DeleteProfileResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}


/** Web service operation '__trt__GetServiceCapabilities__' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __trt__GetServiceCapabilities__(struct soap* soap, struct _trt__GetServiceCapabilities *trt__GetServiceCapabilities, struct _trt__GetServiceCapabilitiesResponse *trt__GetServiceCapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
   
SOAP_FMAC5 int SOAP_FMAC6 __trt__GetVideoSources__(struct soap* soap,  struct _trt__GetVideoSources *trt__GetVideoSources, struct _trt__GetVideoSourcesResponse *trt__GetVideoSourcesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__trt__GetAudioSources__' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioSources__(struct soap* soap,  struct _trt__GetAudioSources *trt__GetAudioSources, struct _trt__GetAudioSourcesResponse *trt__GetAudioSourcesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__trt__GetAudioOutputs__' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __trt__GetAudioOutputs__(struct soap* soap,  struct _trt__GetAudioOutputs *trt__GetAudioOutputs, struct _trt__GetAudioOutputsResponse *trt__GetAudioOutputsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__trt__CreateProfile__' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __trt__CreateProfile__(struct soap* soap,  struct _trt__CreateProfile *trt__CreateProfile, struct _trt__CreateProfileResponse *trt__CreateProfileResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
SOAP_FMAC5 int SOAP_FMAC6 __trt__GetProfile__(struct soap* soap,  struct _trt__GetProfile *trt__GetProfile, struct _trt__GetProfileResponse *trt__GetProfileResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
SOAP_FMAC5 int SOAP_FMAC6 __trt__GetProfiles__(struct soap* soap,  struct _trt__GetProfiles *trt__GetProfiles, struct _trt__GetProfilesResponse *trt__GetProfilesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}


  /** Web service operation '__tr2__GetServiceCapabilities' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetServiceCapabilities(struct soap* soap,  struct _tr2__GetServiceCapabilities *tr2__GetServiceCapabilities, struct _tr2__GetServiceCapabilitiesResponse *tr2__GetServiceCapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__CreateProfile' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__CreateProfile(struct soap* soap,  struct _tr2__CreateProfile *tr2__CreateProfile, struct _tr2__CreateProfileResponse *tr2__CreateProfileResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetProfiles' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetProfiles(struct soap* soap,  struct _tr2__GetProfiles *tr2__GetProfiles, struct _tr2__GetProfilesResponse *tr2__GetProfilesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__AddConfiguration' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__AddConfiguration(struct soap* soap,  struct _tr2__AddConfiguration *tr2__AddConfiguration, struct _tr2__AddConfigurationResponse *tr2__AddConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__RemoveConfiguration' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__RemoveConfiguration(struct soap* soap,  struct _tr2__RemoveConfiguration *tr2__RemoveConfiguration, struct _tr2__RemoveConfigurationResponse *tr2__RemoveConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__DeleteProfile' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__DeleteProfile(struct soap* soap,  struct _tr2__DeleteProfile *tr2__DeleteProfile, struct _tr2__DeleteProfileResponse *tr2__DeleteProfileResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetVideoSourceConfigurations' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetVideoSourceConfigurations(struct soap* soap,  struct tr2__GetConfiguration *tr2__GetVideoSourceConfigurations, struct _tr2__GetVideoSourceConfigurationsResponse *tr2__GetVideoSourceConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetVideoEncoderConfigurations' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetVideoEncoderConfigurations(struct soap* soap,  struct tr2__GetConfiguration *tr2__GetVideoEncoderConfigurations, struct _tr2__GetVideoEncoderConfigurationsResponse *tr2__GetVideoEncoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetAudioSourceConfigurations' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetAudioSourceConfigurations(struct soap* soap,  struct tr2__GetConfiguration *tr2__GetAudioSourceConfigurations, struct _tr2__GetAudioSourceConfigurationsResponse *tr2__GetAudioSourceConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetAudioEncoderConfigurations' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetAudioEncoderConfigurations(struct soap* soap,  struct tr2__GetConfiguration *tr2__GetAudioEncoderConfigurations, struct _tr2__GetAudioEncoderConfigurationsResponse *tr2__GetAudioEncoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetAnalyticsConfigurations' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetAnalyticsConfigurations(struct soap* soap,  struct tr2__GetConfiguration *tr2__GetAnalyticsConfigurations, struct _tr2__GetAnalyticsConfigurationsResponse *tr2__GetAnalyticsConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetMetadataConfigurations' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetMetadataConfigurations(struct soap* soap,  struct tr2__GetConfiguration *tr2__GetMetadataConfigurations, struct _tr2__GetMetadataConfigurationsResponse *tr2__GetMetadataConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetAudioOutputConfigurations' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetAudioOutputConfigurations(struct soap* soap,  struct tr2__GetConfiguration *tr2__GetAudioOutputConfigurations, struct _tr2__GetAudioOutputConfigurationsResponse *tr2__GetAudioOutputConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetAudioDecoderConfigurations' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetAudioDecoderConfigurations(struct soap* soap,  struct tr2__GetConfiguration *tr2__GetAudioDecoderConfigurations, struct _tr2__GetAudioDecoderConfigurationsResponse *tr2__GetAudioDecoderConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__SetVideoSourceConfiguration' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__SetVideoSourceConfiguration(struct soap* soap,  struct _tr2__SetVideoSourceConfiguration *tr2__SetVideoSourceConfiguration, struct tr2__SetConfigurationResponse *tr2__SetVideoSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__SetVideoEncoderConfiguration' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__SetVideoEncoderConfiguration(struct soap* soap,  struct _tr2__SetVideoEncoderConfiguration *tr2__SetVideoEncoderConfiguration, struct tr2__SetConfigurationResponse *tr2__SetVideoEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__SetAudioSourceConfiguration' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__SetAudioSourceConfiguration(struct soap* soap,  struct _tr2__SetAudioSourceConfiguration *tr2__SetAudioSourceConfiguration, struct tr2__SetConfigurationResponse *tr2__SetAudioSourceConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__SetAudioEncoderConfiguration' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__SetAudioEncoderConfiguration(struct soap* soap,  struct _tr2__SetAudioEncoderConfiguration *tr2__SetAudioEncoderConfiguration, struct tr2__SetConfigurationResponse *tr2__SetAudioEncoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__SetMetadataConfiguration' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__SetMetadataConfiguration(struct soap* soap,  struct _tr2__SetMetadataConfiguration *tr2__SetMetadataConfiguration, struct tr2__SetConfigurationResponse *tr2__SetMetadataConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__SetAudioOutputConfiguration' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__SetAudioOutputConfiguration(struct soap* soap,  struct _tr2__SetAudioOutputConfiguration *tr2__SetAudioOutputConfiguration, struct tr2__SetConfigurationResponse *tr2__SetAudioOutputConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__SetAudioDecoderConfiguration' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__SetAudioDecoderConfiguration(struct soap* soap,  struct _tr2__SetAudioDecoderConfiguration *tr2__SetAudioDecoderConfiguration, struct tr2__SetConfigurationResponse *tr2__SetAudioDecoderConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetVideoSourceConfigurationOptions' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetVideoSourceConfigurationOptions(struct soap* soap,  struct tr2__GetConfiguration *tr2__GetVideoSourceConfigurationOptions, struct _tr2__GetVideoSourceConfigurationOptionsResponse *tr2__GetVideoSourceConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetVideoEncoderConfigurationOptions' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetVideoEncoderConfigurationOptions(struct soap* soap,  struct tr2__GetConfiguration *tr2__GetVideoEncoderConfigurationOptions, struct _tr2__GetVideoEncoderConfigurationOptionsResponse *tr2__GetVideoEncoderConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetAudioSourceConfigurationOptions' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetAudioSourceConfigurationOptions(struct soap* soap,  struct tr2__GetConfiguration *tr2__GetAudioSourceConfigurationOptions, struct _tr2__GetAudioSourceConfigurationOptionsResponse *tr2__GetAudioSourceConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetAudioEncoderConfigurationOptions' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetAudioEncoderConfigurationOptions(struct soap* soap,  struct tr2__GetConfiguration *tr2__GetAudioEncoderConfigurationOptions, struct _tr2__GetAudioEncoderConfigurationOptionsResponse *tr2__GetAudioEncoderConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetMetadataConfigurationOptions' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetMetadataConfigurationOptions(struct soap* soap,  struct tr2__GetConfiguration *tr2__GetMetadataConfigurationOptions, struct _tr2__GetMetadataConfigurationOptionsResponse *tr2__GetMetadataConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetAudioOutputConfigurationOptions' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetAudioOutputConfigurationOptions(struct soap* soap,  struct tr2__GetConfiguration *tr2__GetAudioOutputConfigurationOptions, struct _tr2__GetAudioOutputConfigurationOptionsResponse *tr2__GetAudioOutputConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetAudioDecoderConfigurationOptions' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetAudioDecoderConfigurationOptions(struct soap* soap,  struct tr2__GetConfiguration *tr2__GetAudioDecoderConfigurationOptions, struct _tr2__GetAudioDecoderConfigurationOptionsResponse *tr2__GetAudioDecoderConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetVideoEncoderInstances' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetVideoEncoderInstances(struct soap* soap,  struct _tr2__GetVideoEncoderInstances *tr2__GetVideoEncoderInstances, struct _tr2__GetVideoEncoderInstancesResponse *tr2__GetVideoEncoderInstancesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetStreamUri' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetStreamUri(struct soap* soap,  struct _tr2__GetStreamUri *tr2__GetStreamUri, struct _tr2__GetStreamUriResponse *tr2__GetStreamUriResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__StartMulticastStreaming' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__StartMulticastStreaming(struct soap* soap,  struct tr2__StartStopMulticastStreaming *tr2__StartMulticastStreaming, struct tr2__SetConfigurationResponse *tr2__StartMulticastStreamingResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__StopMulticastStreaming' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__StopMulticastStreaming(struct soap* soap,  struct tr2__StartStopMulticastStreaming *tr2__StopMulticastStreaming, struct tr2__SetConfigurationResponse *tr2__StopMulticastStreamingResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__SetSynchronizationPoint' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__SetSynchronizationPoint(struct soap* soap,  struct _tr2__SetSynchronizationPoint *tr2__SetSynchronizationPoint, struct _tr2__SetSynchronizationPointResponse *tr2__SetSynchronizationPointResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetSnapshotUri' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetSnapshotUri(struct soap* soap,  struct _tr2__GetSnapshotUri *tr2__GetSnapshotUri, struct _tr2__GetSnapshotUriResponse *tr2__GetSnapshotUriResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetVideoSourceModes' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetVideoSourceModes(struct soap* soap,  struct _tr2__GetVideoSourceModes *tr2__GetVideoSourceModes, struct _tr2__GetVideoSourceModesResponse *tr2__GetVideoSourceModesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__SetVideoSourceMode' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__SetVideoSourceMode(struct soap* soap,  struct _tr2__SetVideoSourceMode *tr2__SetVideoSourceMode, struct _tr2__SetVideoSourceModeResponse *tr2__SetVideoSourceModeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetOSDs' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetOSDs(struct soap* soap,  struct _tr2__GetOSDs *tr2__GetOSDs, struct _tr2__GetOSDsResponse *tr2__GetOSDsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetOSDOptions' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetOSDOptions(struct soap* soap,  struct _tr2__GetOSDOptions *tr2__GetOSDOptions, struct _tr2__GetOSDOptionsResponse *tr2__GetOSDOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__SetOSD' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__SetOSD(struct soap* soap,  struct _tr2__SetOSD *tr2__SetOSD, struct tr2__SetConfigurationResponse *tr2__SetOSDResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__CreateOSD' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__CreateOSD(struct soap* soap,  struct _tr2__CreateOSD *tr2__CreateOSD, struct _tr2__CreateOSDResponse *tr2__CreateOSDResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__DeleteOSD' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__DeleteOSD(struct soap* soap,  struct _tr2__DeleteOSD *tr2__DeleteOSD, struct tr2__SetConfigurationResponse *tr2__DeleteOSDResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetMasks' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetMasks(struct soap* soap,  struct _tr2__GetMasks *tr2__GetMasks, struct _tr2__GetMasksResponse *tr2__GetMasksResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__GetMaskOptions' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__GetMaskOptions(struct soap* soap,  struct _tr2__GetMaskOptions *tr2__GetMaskOptions, struct _tr2__GetMaskOptionsResponse *tr2__GetMaskOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__SetMask' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__SetMask(struct soap* soap,  struct _tr2__SetMask *tr2__SetMask, struct tr2__SetConfigurationResponse *tr2__SetMaskResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__CreateMask' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__CreateMask(struct soap* soap,  struct _tr2__CreateMask *tr2__CreateMask, struct _tr2__CreateMaskResponse *tr2__CreateMaskResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tr2__DeleteMask' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tr2__DeleteMask(struct soap* soap,  struct _tr2__DeleteMask *tr2__DeleteMask, struct tr2__SetConfigurationResponse *tr2__DeleteMaskResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
   
 SOAP_FMAC5 int SOAP_FMAC6 __tev__PullMessages(struct soap* soap,  struct _tev__PullMessages *tev__PullMessages, struct _tev__PullMessagesResponse *tev__PullMessagesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tev__SetSynchronizationPoint' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tev__SetSynchronizationPoint(struct soap* soap,  struct _tev__SetSynchronizationPoint *tev__SetSynchronizationPoint, struct _tev__SetSynchronizationPointResponse *tev__SetSynchronizationPointResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tev__GetServiceCapabilities' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tev__GetServiceCapabilities(struct soap* soap,  struct _tev__GetServiceCapabilities *tev__GetServiceCapabilities, struct _tev__GetServiceCapabilitiesResponse *tev__GetServiceCapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tev__CreatePullPointSubscription' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tev__CreatePullPointSubscription(struct soap* soap,  struct _tev__CreatePullPointSubscription *tev__CreatePullPointSubscription, struct _tev__CreatePullPointSubscriptionResponse *tev__CreatePullPointSubscriptionResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tev__GetEventProperties' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tev__GetEventProperties(struct soap* soap,  struct _tev__GetEventProperties *tev__GetEventProperties, struct _tev__GetEventPropertiesResponse *tev__GetEventPropertiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tev__Renew' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tev__Renew(struct soap* soap,  struct _wsnt__Renew *wsnt__Renew, struct _wsnt__RenewResponse *wsnt__RenewResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tev__Unsubscribe' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tev__Unsubscribe(struct soap* soap,  struct _wsnt__Unsubscribe *wsnt__Unsubscribe, struct _wsnt__UnsubscribeResponse *wsnt__UnsubscribeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tev__Subscribe' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tev__Subscribe(struct soap* soap,  struct _wsnt__Subscribe *wsnt__Subscribe, struct _wsnt__SubscribeResponse *wsnt__SubscribeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tev__GetCurrentMessage' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tev__GetCurrentMessage(struct soap* soap,  struct _wsnt__GetCurrentMessage *wsnt__GetCurrentMessage, struct _wsnt__GetCurrentMessageResponse *wsnt__GetCurrentMessageResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tev__Notify' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tev__Notify(struct soap* soap,  struct _wsnt__Notify *wsnt__Notify);
 /** Web service operation '__tev__GetMessages' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tev__GetMessages(struct soap* soap,  struct _wsnt__GetMessages *wsnt__GetMessages, struct _wsnt__GetMessagesResponse *wsnt__GetMessagesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tev__DestroyPullPoint' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tev__DestroyPullPoint(struct soap* soap,  struct _wsnt__DestroyPullPoint *wsnt__DestroyPullPoint, struct _wsnt__DestroyPullPointResponse *wsnt__DestroyPullPointResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tev__Notify_' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tev__Notify_(struct soap* soap,  struct _wsnt__Notify *wsnt__Notify){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tev__CreatePullPoint' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tev__CreatePullPoint(struct soap* soap,  struct _wsnt__CreatePullPoint *wsnt__CreatePullPoint, struct _wsnt__CreatePullPointResponse *wsnt__CreatePullPointResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tev__Renew_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tev__Renew_(struct soap* soap,  struct _wsnt__Renew *wsnt__Renew, struct _wsnt__RenewResponse *wsnt__RenewResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tev__Unsubscribe_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tev__Unsubscribe_(struct soap* soap,  struct _wsnt__Unsubscribe *wsnt__Unsubscribe, struct _wsnt__UnsubscribeResponse *wsnt__UnsubscribeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tev__PauseSubscription' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tev__PauseSubscription(struct soap* soap,  struct _wsnt__PauseSubscription *wsnt__PauseSubscription, struct _wsnt__PauseSubscriptionResponse *wsnt__PauseSubscriptionResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tev__ResumeSubscription' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tev__ResumeSubscription(struct soap* soap,  struct _wsnt__ResumeSubscription *wsnt__ResumeSubscription, struct _wsnt__ResumeSubscriptionResponse *wsnt__ResumeSubscriptionResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
SOAP_FMAC5 int SOAP_FMAC6 __tev__Notify(struct soap* soap, struct _wsnt__Notify *wsnt__Notify){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tds__GetServices_(struct soap* soap,  struct _tds__GetServices *tds__GetServices, struct _tds__GetServicesResponse *tds__GetServicesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetServiceCapabilities_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetServiceCapabilities_(struct soap* soap,  struct _tds__GetServiceCapabilities *tds__GetServiceCapabilities, struct _tds__GetServiceCapabilitiesResponse *tds__GetServiceCapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetDeviceInformation_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetDeviceInformation_(struct soap* soap,  struct _tds__GetDeviceInformation *tds__GetDeviceInformation, struct _tds__GetDeviceInformationResponse *tds__GetDeviceInformationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetSystemDateAndTime_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetSystemDateAndTime_(struct soap* soap,  struct _tds__SetSystemDateAndTime *tds__SetSystemDateAndTime, struct _tds__SetSystemDateAndTimeResponse *tds__SetSystemDateAndTimeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetSystemDateAndTime_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetSystemDateAndTime_(struct soap* soap,  struct _tds__GetSystemDateAndTime *tds__GetSystemDateAndTime, struct _tds__GetSystemDateAndTimeResponse *tds__GetSystemDateAndTimeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetSystemFactoryDefault_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetSystemFactoryDefault_(struct soap* soap,  struct _tds__SetSystemFactoryDefault *tds__SetSystemFactoryDefault, struct _tds__SetSystemFactoryDefaultResponse *tds__SetSystemFactoryDefaultResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__UpgradeSystemFirmware_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__UpgradeSystemFirmware_(struct soap* soap,  struct _tds__UpgradeSystemFirmware *tds__UpgradeSystemFirmware, struct _tds__UpgradeSystemFirmwareResponse *tds__UpgradeSystemFirmwareResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SystemReboot_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SystemReboot_(struct soap* soap,  struct _tds__SystemReboot *tds__SystemReboot, struct _tds__SystemRebootResponse *tds__SystemRebootResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__RestoreSystem_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__RestoreSystem_(struct soap* soap,  struct _tds__RestoreSystem *tds__RestoreSystem, struct _tds__RestoreSystemResponse *tds__RestoreSystemResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetSystemBackup_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetSystemBackup_(struct soap* soap,  struct _tds__GetSystemBackup *tds__GetSystemBackup, struct _tds__GetSystemBackupResponse *tds__GetSystemBackupResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetSystemLog_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetSystemLog_(struct soap* soap,  struct _tds__GetSystemLog *tds__GetSystemLog, struct _tds__GetSystemLogResponse *tds__GetSystemLogResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetSystemSupportInformation_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetSystemSupportInformation_(struct soap* soap,  struct _tds__GetSystemSupportInformation *tds__GetSystemSupportInformation, struct _tds__GetSystemSupportInformationResponse *tds__GetSystemSupportInformationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetScopes_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetScopes_(struct soap* soap,  struct _tds__GetScopes *tds__GetScopes, struct _tds__GetScopesResponse *tds__GetScopesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetScopes_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetScopes_(struct soap* soap,  struct _tds__SetScopes *tds__SetScopes, struct _tds__SetScopesResponse *tds__SetScopesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__AddScopes_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__AddScopes_(struct soap* soap,  struct _tds__AddScopes *tds__AddScopes, struct _tds__AddScopesResponse *tds__AddScopesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__RemoveScopes_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__RemoveScopes_(struct soap* soap,  struct _tds__RemoveScopes *tds__RemoveScopes, struct _tds__RemoveScopesResponse *tds__RemoveScopesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetDiscoveryMode_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetDiscoveryMode_(struct soap* soap,  struct _tds__GetDiscoveryMode *tds__GetDiscoveryMode, struct _tds__GetDiscoveryModeResponse *tds__GetDiscoveryModeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetDiscoveryMode_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetDiscoveryMode_(struct soap* soap,  struct _tds__SetDiscoveryMode *tds__SetDiscoveryMode, struct _tds__SetDiscoveryModeResponse *tds__SetDiscoveryModeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetRemoteDiscoveryMode_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetRemoteDiscoveryMode_(struct soap* soap,  struct _tds__GetRemoteDiscoveryMode *tds__GetRemoteDiscoveryMode, struct _tds__GetRemoteDiscoveryModeResponse *tds__GetRemoteDiscoveryModeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetRemoteDiscoveryMode_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetRemoteDiscoveryMode_(struct soap* soap,  struct _tds__SetRemoteDiscoveryMode *tds__SetRemoteDiscoveryMode, struct _tds__SetRemoteDiscoveryModeResponse *tds__SetRemoteDiscoveryModeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/* Web service operation '__tds__GetDPAddresses_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetDPAddresses_(struct soap* soap,  struct _tds__GetDPAddresses *tds__GetDPAddresses, struct _tds__GetDPAddressesResponse *tds__GetDPAddressesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetEndpointReference_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetEndpointReference_(struct soap* soap,  struct _tds__GetEndpointReference *tds__GetEndpointReference, struct _tds__GetEndpointReferenceResponse *tds__GetEndpointReferenceResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetRemoteUser_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetRemoteUser_(struct soap* soap,  struct _tds__GetRemoteUser *tds__GetRemoteUser, struct _tds__GetRemoteUserResponse *tds__GetRemoteUserResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetRemoteUser_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetRemoteUser_(struct soap* soap,  struct _tds__SetRemoteUser *tds__SetRemoteUser, struct _tds__SetRemoteUserResponse *tds__SetRemoteUserResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetUsers_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetUsers_(struct soap* soap,  struct _tds__GetUsers *tds__GetUsers, struct _tds__GetUsersResponse *tds__GetUsersResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__CreateUsers_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__CreateUsers_(struct soap* soap,  struct _tds__CreateUsers *tds__CreateUsers, struct _tds__CreateUsersResponse *tds__CreateUsersResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__DeleteUsers_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__DeleteUsers_(struct soap* soap,  struct _tds__DeleteUsers *tds__DeleteUsers, struct _tds__DeleteUsersResponse *tds__DeleteUsersResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetUser_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetUser_(struct soap* soap,  struct _tds__SetUser *tds__SetUser, struct _tds__SetUserResponse *tds__SetUserResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetWsdlUrl_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetWsdlUrl_(struct soap* soap,  struct _tds__GetWsdlUrl *tds__GetWsdlUrl, struct _tds__GetWsdlUrlResponse *tds__GetWsdlUrlResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetCapabilities_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetCapabilities_(struct soap* soap,  struct _tds__GetCapabilities *tds__GetCapabilities, struct _tds__GetCapabilitiesResponse *tds__GetCapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetDPAddresses_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetDPAddresses_(struct soap* soap,  struct _tds__SetDPAddresses *tds__SetDPAddresses, struct _tds__SetDPAddressesResponse *tds__SetDPAddressesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetHostname_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetHostname_(struct soap* soap,  struct _tds__GetHostname *tds__GetHostname, struct _tds__GetHostnameResponse *tds__GetHostnameResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetHostname_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetHostname_(struct soap* soap,  struct _tds__SetHostname *tds__SetHostname, struct _tds__SetHostnameResponse *tds__SetHostnameResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetHostnameFromDHCP_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetHostnameFromDHCP_(struct soap* soap,  struct _tds__SetHostnameFromDHCP *tds__SetHostnameFromDHCP, struct _tds__SetHostnameFromDHCPResponse *tds__SetHostnameFromDHCPResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetDNS_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetDNS_(struct soap* soap,  struct _tds__GetDNS *tds__GetDNS, struct _tds__GetDNSResponse *tds__GetDNSResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetDNS_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetDNS_(struct soap* soap,  struct _tds__SetDNS *tds__SetDNS, struct _tds__SetDNSResponse *tds__SetDNSResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetNTP_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetNTP_(struct soap* soap,  struct _tds__GetNTP *tds__GetNTP, struct _tds__GetNTPResponse *tds__GetNTPResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetNTP_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetNTP_(struct soap* soap,  struct _tds__SetNTP *tds__SetNTP, struct _tds__SetNTPResponse *tds__SetNTPResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetDynamicDNS_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetDynamicDNS_(struct soap* soap,  struct _tds__GetDynamicDNS *tds__GetDynamicDNS, struct _tds__GetDynamicDNSResponse *tds__GetDynamicDNSResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetDynamicDNS_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetDynamicDNS_(struct soap* soap,  struct _tds__SetDynamicDNS *tds__SetDynamicDNS, struct _tds__SetDynamicDNSResponse *tds__SetDynamicDNSResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetNetworkInterfaces_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetNetworkInterfaces_(struct soap* soap,  struct _tds__GetNetworkInterfaces *tds__GetNetworkInterfaces, struct _tds__GetNetworkInterfacesResponse *tds__GetNetworkInterfacesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetNetworkInterfaces_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetNetworkInterfaces_(struct soap* soap,  struct _tds__SetNetworkInterfaces *tds__SetNetworkInterfaces, struct _tds__SetNetworkInterfacesResponse *tds__SetNetworkInterfacesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetNetworkProtocols_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetNetworkProtocols_(struct soap* soap,  struct _tds__GetNetworkProtocols *tds__GetNetworkProtocols, struct _tds__GetNetworkProtocolsResponse *tds__GetNetworkProtocolsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetNetworkProtocols_' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tds__SetNetworkProtocols_(struct soap* soap,  struct _tds__SetNetworkProtocols *tds__SetNetworkProtocols, struct _tds__SetNetworkProtocolsResponse *tds__SetNetworkProtocolsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tds__GetNetworkDefaultGateway_' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tds__GetNetworkDefaultGateway_(struct soap* soap,  struct _tds__GetNetworkDefaultGateway *tds__GetNetworkDefaultGateway, struct _tds__GetNetworkDefaultGatewayResponse *tds__GetNetworkDefaultGatewayResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tds__SetNetworkDefaultGateway_' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tds__SetNetworkDefaultGateway_(struct soap* soap,  struct _tds__SetNetworkDefaultGateway *tds__SetNetworkDefaultGateway, struct _tds__SetNetworkDefaultGatewayResponse *tds__SetNetworkDefaultGatewayResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tds__GetZeroConfiguration_' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tds__GetZeroConfiguration_(struct soap* soap,  struct _tds__GetZeroConfiguration *tds__GetZeroConfiguration, struct _tds__GetZeroConfigurationResponse *tds__GetZeroConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tds__SetZeroConfiguration_' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tds__SetZeroConfiguration_(struct soap* soap,  struct _tds__SetZeroConfiguration *tds__SetZeroConfiguration, struct _tds__SetZeroConfigurationResponse *tds__SetZeroConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tds__GetIPAddressFilter_' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tds__GetIPAddressFilter_(struct soap* soap,  struct _tds__GetIPAddressFilter *tds__GetIPAddressFilter, struct _tds__GetIPAddressFilterResponse *tds__GetIPAddressFilterResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tds__SetIPAddressFilter_' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tds__SetIPAddressFilter_(struct soap* soap,  struct _tds__SetIPAddressFilter *tds__SetIPAddressFilter, struct _tds__SetIPAddressFilterResponse *tds__SetIPAddressFilterResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tds__AddIPAddressFilter_' (returns SOAP_OK or error code) */
 SOAP_FMAC5 int SOAP_FMAC6 __tds__AddIPAddressFilter_(struct soap* soap,  struct _tds__AddIPAddressFilter *tds__AddIPAddressFilter, struct _tds__AddIPAddressFilterResponse *tds__AddIPAddressFilterResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tds__RemoveIPAddressFilter_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__RemoveIPAddressFilter_(struct soap* soap,  struct _tds__RemoveIPAddressFilter *tds__RemoveIPAddressFilter, struct _tds__RemoveIPAddressFilterResponse *tds__RemoveIPAddressFilterResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetAccessPolicy_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetAccessPolicy_(struct soap* soap,  struct _tds__GetAccessPolicy *tds__GetAccessPolicy, struct _tds__GetAccessPolicyResponse *tds__GetAccessPolicyResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetAccessPolicy_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetAccessPolicy_(struct soap* soap,  struct _tds__SetAccessPolicy *tds__SetAccessPolicy, struct _tds__SetAccessPolicyResponse *tds__SetAccessPolicyResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__CreateCertificate_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__CreateCertificate_(struct soap* soap,  struct _tds__CreateCertificate *tds__CreateCertificate, struct _tds__CreateCertificateResponse *tds__CreateCertificateResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetCertificates_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetCertificates_(struct soap* soap,  struct _tds__GetCertificates *tds__GetCertificates, struct _tds__GetCertificatesResponse *tds__GetCertificatesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetCertificatesStatus_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetCertificatesStatus_(struct soap* soap,  struct _tds__GetCertificatesStatus *tds__GetCertificatesStatus, struct _tds__GetCertificatesStatusResponse *tds__GetCertificatesStatusResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetCertificatesStatus_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetCertificatesStatus_(struct soap* soap,  struct _tds__SetCertificatesStatus *tds__SetCertificatesStatus, struct _tds__SetCertificatesStatusResponse *tds__SetCertificatesStatusResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__DeleteCertificates_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__DeleteCertificates_(struct soap* soap,  struct _tds__DeleteCertificates *tds__DeleteCertificates, struct _tds__DeleteCertificatesResponse *tds__DeleteCertificatesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetPkcs10Request_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetPkcs10Request_(struct soap* soap,  struct _tds__GetPkcs10Request *tds__GetPkcs10Request, struct _tds__GetPkcs10RequestResponse *tds__GetPkcs10RequestResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__LoadCertificates_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__LoadCertificates_(struct soap* soap,  struct _tds__LoadCertificates *tds__LoadCertificates, struct _tds__LoadCertificatesResponse *tds__LoadCertificatesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetClientCertificateMode_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetClientCertificateMode_(struct soap* soap,  struct _tds__GetClientCertificateMode *tds__GetClientCertificateMode, struct _tds__GetClientCertificateModeResponse *tds__GetClientCertificateModeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetClientCertificateMode_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetClientCertificateMode_(struct soap* soap,  struct _tds__SetClientCertificateMode *tds__SetClientCertificateMode, struct _tds__SetClientCertificateModeResponse *tds__SetClientCertificateModeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetRelayOutputs_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetRelayOutputs_(struct soap* soap,  struct _tds__GetRelayOutputs *tds__GetRelayOutputs, struct _tds__GetRelayOutputsResponse *tds__GetRelayOutputsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetRelayOutputSettings_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetRelayOutputSettings_(struct soap* soap,  struct _tds__SetRelayOutputSettings *tds__SetRelayOutputSettings, struct _tds__SetRelayOutputSettingsResponse *tds__SetRelayOutputSettingsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetRelayOutputState_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetRelayOutputState_(struct soap* soap,  struct _tds__SetRelayOutputState *tds__SetRelayOutputState, struct _tds__SetRelayOutputStateResponse *tds__SetRelayOutputStateResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SendAuxiliaryCommand_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SendAuxiliaryCommand_(struct soap* soap,  struct _tds__SendAuxiliaryCommand *tds__SendAuxiliaryCommand, struct _tds__SendAuxiliaryCommandResponse *tds__SendAuxiliaryCommandResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetCACertificates_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetCACertificates_(struct soap* soap,  struct _tds__GetCACertificates *tds__GetCACertificates, struct _tds__GetCACertificatesResponse *tds__GetCACertificatesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__LoadCertificateWithPrivateKey_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__LoadCertificateWithPrivateKey_(struct soap* soap,  struct _tds__LoadCertificateWithPrivateKey *tds__LoadCertificateWithPrivateKey, struct _tds__LoadCertificateWithPrivateKeyResponse *tds__LoadCertificateWithPrivateKeyResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetCertificateInformation_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetCertificateInformation_(struct soap* soap,  struct _tds__GetCertificateInformation *tds__GetCertificateInformation, struct _tds__GetCertificateInformationResponse *tds__GetCertificateInformationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__LoadCACertificates_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__LoadCACertificates_(struct soap* soap,  struct _tds__LoadCACertificates *tds__LoadCACertificates, struct _tds__LoadCACertificatesResponse *tds__LoadCACertificatesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__CreateDot1XConfiguration_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__CreateDot1XConfiguration_(struct soap* soap,  struct _tds__CreateDot1XConfiguration *tds__CreateDot1XConfiguration, struct _tds__CreateDot1XConfigurationResponse *tds__CreateDot1XConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetDot1XConfiguration_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetDot1XConfiguration_(struct soap* soap,  struct _tds__SetDot1XConfiguration *tds__SetDot1XConfiguration, struct _tds__SetDot1XConfigurationResponse *tds__SetDot1XConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetDot1XConfiguration_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetDot1XConfiguration_(struct soap* soap,  struct _tds__GetDot1XConfiguration *tds__GetDot1XConfiguration, struct _tds__GetDot1XConfigurationResponse *tds__GetDot1XConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetDot1XConfigurations_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetDot1XConfigurations_(struct soap* soap,  struct _tds__GetDot1XConfigurations *tds__GetDot1XConfigurations, struct _tds__GetDot1XConfigurationsResponse *tds__GetDot1XConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__DeleteDot1XConfiguration_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__DeleteDot1XConfiguration_(struct soap* soap,  struct _tds__DeleteDot1XConfiguration *tds__DeleteDot1XConfiguration, struct _tds__DeleteDot1XConfigurationResponse *tds__DeleteDot1XConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetDot11Capabilities_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetDot11Capabilities_(struct soap* soap,  struct _tds__GetDot11Capabilities *tds__GetDot11Capabilities, struct _tds__GetDot11CapabilitiesResponse *tds__GetDot11CapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetDot11Status_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetDot11Status_(struct soap* soap,  struct _tds__GetDot11Status *tds__GetDot11Status, struct _tds__GetDot11StatusResponse *tds__GetDot11StatusResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__ScanAvailableDot11Networks_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__ScanAvailableDot11Networks_(struct soap* soap,  struct _tds__ScanAvailableDot11Networks *tds__ScanAvailableDot11Networks, struct _tds__ScanAvailableDot11NetworksResponse *tds__ScanAvailableDot11NetworksResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetSystemUris_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetSystemUris_(struct soap* soap,  struct _tds__GetSystemUris *tds__GetSystemUris, struct _tds__GetSystemUrisResponse *tds__GetSystemUrisResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__StartFirmwareUpgrade_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__StartFirmwareUpgrade_(struct soap* soap,  struct _tds__StartFirmwareUpgrade *tds__StartFirmwareUpgrade, struct _tds__StartFirmwareUpgradeResponse *tds__StartFirmwareUpgradeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__StartSystemRestore_' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__StartSystemRestore_(struct soap* soap,  struct _tds__StartSystemRestore *tds__StartSystemRestore, struct _tds__StartSystemRestoreResponse *tds__StartSystemRestoreResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetStorageConfigurations' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetStorageConfigurations(struct soap* soap,  struct _tds__GetStorageConfigurations *tds__GetStorageConfigurations, struct _tds__GetStorageConfigurationsResponse *tds__GetStorageConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__CreateStorageConfiguration' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__CreateStorageConfiguration(struct soap* soap,  struct _tds__CreateStorageConfiguration *tds__CreateStorageConfiguration, struct _tds__CreateStorageConfigurationResponse *tds__CreateStorageConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetStorageConfiguration' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetStorageConfiguration(struct soap* soap,  struct _tds__GetStorageConfiguration *tds__GetStorageConfiguration, struct _tds__GetStorageConfigurationResponse *tds__GetStorageConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetStorageConfiguration' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetStorageConfiguration(struct soap* soap,  struct _tds__SetStorageConfiguration *tds__SetStorageConfiguration, struct _tds__SetStorageConfigurationResponse *tds__SetStorageConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__DeleteStorageConfiguration' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__DeleteStorageConfiguration(struct soap* soap,  struct _tds__DeleteStorageConfiguration *tds__DeleteStorageConfiguration, struct _tds__DeleteStorageConfigurationResponse *tds__DeleteStorageConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__GetGeoLocation' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__GetGeoLocation(struct soap* soap,  struct _tds__GetGeoLocation *tds__GetGeoLocation, struct _tds__GetGeoLocationResponse *tds__GetGeoLocationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__SetGeoLocation' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__SetGeoLocation(struct soap* soap,  struct _tds__SetGeoLocation *tds__SetGeoLocation, struct _tds__SetGeoLocationResponse *tds__SetGeoLocationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tds__DeleteGeoLocation' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tds__DeleteGeoLocation(struct soap* soap,  struct _tds__DeleteGeoLocation *tds__DeleteGeoLocation, struct _tds__DeleteGeoLocationResponse *tds__DeleteGeoLocationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  

SOAP_FMAC5 int SOAP_FMAC6 __tan__GetSupportedRules(struct soap* soap, struct _tan__GetSupportedRules *tan__GetSupportedRules, struct _tan__GetSupportedRulesResponse *tan__GetSupportedRulesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tan__CreateRules' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tan__CreateRules(struct soap* soap, struct _tan__CreateRules *tan__CreateRules, struct _tan__CreateRulesResponse *tan__CreateRulesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tan__DeleteRules' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tan__DeleteRules(struct soap* soap, struct _tan__DeleteRules *tan__DeleteRules, struct _tan__DeleteRulesResponse *tan__DeleteRulesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tan__GetRules' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tan__GetRules(struct soap* soap, struct _tan__GetRules *tan__GetRules, struct _tan__GetRulesResponse *tan__GetRulesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tan__GetRuleOptions' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tan__GetRuleOptions(struct soap* soap, struct _tan__GetRuleOptions *tan__GetRuleOptions, struct _tan__GetRuleOptionsResponse *tan__GetRuleOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tan__ModifyRules' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tan__ModifyRules(struct soap* soap, struct _tan__ModifyRules *tan__ModifyRules, struct _tan__ModifyRulesResponse *tan__ModifyRulesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tan__GetServiceCapabilities' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tan__GetServiceCapabilities(struct soap* soap, struct _tan__GetServiceCapabilities *tan__GetServiceCapabilities, struct _tan__GetServiceCapabilitiesResponse *tan__GetServiceCapabilitiesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tan__GetSupportedAnalyticsModules' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tan__GetSupportedAnalyticsModules(struct soap* soap, struct _tan__GetSupportedAnalyticsModules *tan__GetSupportedAnalyticsModules, struct _tan__GetSupportedAnalyticsModulesResponse *tan__GetSupportedAnalyticsModulesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tan__CreateAnalyticsModules' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tan__CreateAnalyticsModules(struct soap* soap, struct _tan__CreateAnalyticsModules *tan__CreateAnalyticsModules, struct _tan__CreateAnalyticsModulesResponse *tan__CreateAnalyticsModulesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tan__DeleteAnalyticsModules' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tan__DeleteAnalyticsModules(struct soap* soap, struct _tan__DeleteAnalyticsModules *tan__DeleteAnalyticsModules, struct _tan__DeleteAnalyticsModulesResponse *tan__DeleteAnalyticsModulesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tan__GetAnalyticsModules' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tan__GetAnalyticsModules(struct soap* soap, struct _tan__GetAnalyticsModules *tan__GetAnalyticsModules, struct _tan__GetAnalyticsModulesResponse *tan__GetAnalyticsModulesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tan__GetAnalyticsModuleOptions' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tan__GetAnalyticsModuleOptions(struct soap* soap, struct _tan__GetAnalyticsModuleOptions *tan__GetAnalyticsModuleOptions, struct _tan__GetAnalyticsModuleOptionsResponse *tan__GetAnalyticsModuleOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tan__ModifyAnalyticsModules' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tan__ModifyAnalyticsModules(struct soap* soap, struct _tan__ModifyAnalyticsModules *tan__ModifyAnalyticsModules, struct _tan__ModifyAnalyticsModulesResponse *tan__ModifyAnalyticsModulesResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
  

SOAP_FMAC5 int SOAP_FMAC6 __tev__Seek(struct soap* soap, struct _tev__Seek *tev__Seek, struct _tev__SeekResponse *tev__SeekResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
 /** Web service operation '__tev__Unsubscribe' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tev__Unsubscribe__(struct soap* soap, struct _wsnt__Unsubscribe *wsnt__Unsubscribe, struct _wsnt__UnsubscribeResponse *wsnt__UnsubscribeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}


SOAP_FMAC5 int SOAP_FMAC6 __timg__GetPresets(struct soap* soap, struct _timg__GetPresets *timg__GetPresets, struct _timg__GetPresetsResponse *timg__GetPresetsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
SOAP_FMAC5 int SOAP_FMAC6 __timg__GetCurrentPreset(struct soap* soap, struct _timg__GetCurrentPreset *timg__GetCurrentPreset, struct _timg__GetCurrentPresetResponse *timg__GetCurrentPresetResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
SOAP_FMAC5 int SOAP_FMAC6 __timg__SetCurrentPreset(struct soap* soap, struct _timg__SetCurrentPreset *timg__SetCurrentPreset, struct _timg__SetCurrentPresetResponse *timg__SetCurrentPresetResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}


SOAP_FMAC5 int SOAP_FMAC6 __tmd__GetRelayOutputOptions(struct soap* soap, struct _tmd__GetRelayOutputOptions *tmd__GetRelayOutputOptions, struct _tmd__GetRelayOutputOptionsResponse *tmd__GetRelayOutputOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
SOAP_FMAC5 int SOAP_FMAC6 __tmd__GetDigitalInputConfigurationOptions(struct soap* soap, struct _tmd__GetDigitalInputConfigurationOptions *tmd__GetDigitalInputConfigurationOptions, struct _tmd__GetDigitalInputConfigurationOptionsResponse *tmd__GetDigitalInputConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
SOAP_FMAC5 int SOAP_FMAC6 __tmd__SetDigitalInputConfigurations(struct soap* soap, struct _tmd__SetDigitalInputConfigurations *tmd__SetDigitalInputConfigurations, struct _tmd__SetDigitalInputConfigurationsResponse *tmd__SetDigitalInputConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
   
SOAP_FMAC5 int SOAP_FMAC6 __tmd__GetSerialPorts(struct soap* soap, struct _tmd__GetSerialPorts *tmd__GetSerialPorts, struct _tmd__GetSerialPortsResponse *tmd__GetSerialPortsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tmd__GetSerialPortConfiguration' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tmd__GetSerialPortConfiguration(struct soap* soap, struct _tmd__GetSerialPortConfiguration *tmd__GetSerialPortConfiguration, struct _tmd__GetSerialPortConfigurationResponse *tmd__GetSerialPortConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tmd__SetSerialPortConfiguration' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tmd__SetSerialPortConfiguration(struct soap* soap, struct _tmd__SetSerialPortConfiguration *tmd__SetSerialPortConfiguration, struct _tmd__SetSerialPortConfigurationResponse *tmd__SetSerialPortConfigurationResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tmd__GetSerialPortConfigurationOptions' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tmd__GetSerialPortConfigurationOptions(struct soap* soap, struct _tmd__GetSerialPortConfigurationOptions *tmd__GetSerialPortConfigurationOptions, struct _tmd__GetSerialPortConfigurationOptionsResponse *tmd__GetSerialPortConfigurationOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}

SOAP_FMAC5 int SOAP_FMAC6 __tmd__SendReceiveSerialCommand(struct soap* soap, struct _tmd__SendReceiveSerialCommand *tmd__SendReceiveSerialCommand, struct _tmd__SendReceiveSerialCommandResponse *tmd__SendReceiveSerialCommandResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
SOAP_FMAC5 int SOAP_FMAC6 __tmd__GetDigitalInputs(struct soap* soap, struct _tmd__GetDigitalInputs *tmd__GetDigitalInputs, struct _tmd__GetDigitalInputsResponse *tmd__GetDigitalInputsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}


SOAP_FMAC5 int SOAP_FMAC6 __tptz__GeoMove(struct soap* soap, struct _tptz__GeoMove *tptz__GeoMove, struct _tptz__GeoMoveResponse *tptz__GeoMoveResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tptz__Stop' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tptz__GetPresetTours(struct soap* soap, struct _tptz__GetPresetTours *tptz__GetPresetTours, struct _tptz__GetPresetToursResponse *tptz__GetPresetToursResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tptz__GetPresetTour' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tptz__GetPresetTour(struct soap* soap, struct _tptz__GetPresetTour *tptz__GetPresetTour, struct _tptz__GetPresetTourResponse *tptz__GetPresetTourResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tptz__GetPresetTourOptions' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tptz__GetPresetTourOptions(struct soap* soap, struct _tptz__GetPresetTourOptions *tptz__GetPresetTourOptions, struct _tptz__GetPresetTourOptionsResponse *tptz__GetPresetTourOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tptz__CreatePresetTour' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tptz__CreatePresetTour(struct soap* soap, struct _tptz__CreatePresetTour *tptz__CreatePresetTour, struct _tptz__CreatePresetTourResponse *tptz__CreatePresetTourResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tptz__ModifyPresetTour' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tptz__ModifyPresetTour(struct soap* soap, struct _tptz__ModifyPresetTour *tptz__ModifyPresetTour, struct _tptz__ModifyPresetTourResponse *tptz__ModifyPresetTourResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tptz__OperatePresetTour' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tptz__OperatePresetTour(struct soap* soap, struct _tptz__OperatePresetTour *tptz__OperatePresetTour, struct _tptz__OperatePresetTourResponse *tptz__OperatePresetTourResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tptz__RemovePresetTour' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tptz__RemovePresetTour(struct soap* soap, struct _tptz__RemovePresetTour *tptz__RemovePresetTour, struct _tptz__RemovePresetTourResponse *tptz__RemovePresetTourResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
/** Web service operation '__tptz__GetCompatibleConfigurations' (returns SOAP_OK or error code) */
SOAP_FMAC5 int SOAP_FMAC6 __tptz__GetCompatibleConfigurations(struct soap* soap, struct _tptz__GetCompatibleConfigurations *tptz__GetCompatibleConfigurations, struct _tptz__GetCompatibleConfigurationsResponse *tptz__GetCompatibleConfigurationsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}


SOAP_FMAC5 int SOAP_FMAC6 __trc__GetRecordingOptions(struct soap* soap, struct _trc__GetRecordingOptions *trc__GetRecordingOptions, struct _trc__GetRecordingOptionsResponse *trc__GetRecordingOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
SOAP_FMAC5 int SOAP_FMAC6 __trc__ExportRecordedData(struct soap* soap, struct _trc__ExportRecordedData *trc__ExportRecordedData, struct _trc__ExportRecordedDataResponse *trc__ExportRecordedDataResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
SOAP_FMAC5 int SOAP_FMAC6 __trc__StopExportRecordedData(struct soap* soap, struct _trc__StopExportRecordedData *trc__StopExportRecordedData, struct _trc__StopExportRecordedDataResponse *trc__StopExportRecordedDataResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
SOAP_FMAC5 int SOAP_FMAC6 __trc__GetExportRecordedDataState(struct soap* soap, struct _trc__GetExportRecordedDataState *trc__GetExportRecordedDataState, struct _trc__GetExportRecordedDataStateResponse *trc__GetExportRecordedDataStateResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}


SOAP_FMAC5 int SOAP_FMAC6 __tanae__GetAnalyticsModuleOptions(struct soap*soap, struct _tan__GetAnalyticsModuleOptions *tan__GetAnalyticsModuleOptions, struct _tan__GetAnalyticsModuleOptionsResponse *tan__GetAnalyticsModuleOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
SOAP_FMAC5 int SOAP_FMAC6 __tanre__GetRuleOptions(struct soap*soap, struct _tan__GetRuleOptions *tan__GetRuleOptions, struct _tan__GetRuleOptionsResponse *tan__GetRuleOptionsResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
SOAP_FMAC5 int SOAP_FMAC6 __tetpps__Seek(struct soap*soap, struct _tev__Seek *tev__Seek, struct _tev__SeekResponse *tev__SeekResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}
SOAP_FMAC5 int SOAP_FMAC6 __tetpps__Unsubscribe(struct soap*soap, struct _wsnt__Unsubscribe *wsnt__Unsubscribe, struct _wsnt__UnsubscribeResponse *wsnt__UnsubscribeResponse){TRACE("%s in \n",__FUNCTION__);TRACE("%s out \n",__FUNCTION__);return 0;}



char *cert = NULL;
char *rsa_privk = NULL, *rsa_pubk = NULL;

/* The secret HMAC key is shared between client and server */
static char hmac_key[16] = /* Dummy HMAC key for signature test */
{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };

/* The secret triple DES secret key shared between client and server for message encryption */
static char des_key[20] = /* Dummy 160-bit triple DES key for encryption test */
{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 };


static const void *token_handler(struct soap *soap, int alg, const char *keyname, int *keylen)
{ 
printf("use token_handler\n");
	switch (alg)
  { case SOAP_SMD_VRFY_DSA_SHA1:
    case SOAP_SMD_VRFY_RSA_SHA1:
      return (const void*)cert; /* signature verification with public cert */
    case SOAP_SMD_HMAC_SHA1:
      *keylen = sizeof(hmac_key);
      return (const void*)hmac_key; /* signature verification with secret key */
    case SOAP_MEC_ENV_DEC_DES_CBC:
      /* should inquire keyname (contains key name or subject name/key id) */
      return (const void*)rsa_privk; /* envelope decryption with private key */
    case SOAP_MEC_DEC_DES_CBC:
      /* should inquire keyname (contains key name or subject name/key id) */
      *keylen = sizeof(des_key);
      return (const void*)des_key; /* decryption with secret key */
  }
  return NULL; /* fail */
}

int FindSTR( char  * byhtml, char *cfind, int nStartPos)
{
	int i;
	long length;
	char * tmphtml;
	char * temfind ;

	
	if( byhtml == NULL || cfind == NULL)
		return -1;

	tmphtml =(char *) byhtml + nStartPos;

	temfind = cfind;

	if(nStartPos < 0)
		return -1;

	length = strlen(tmphtml); 
	
	for( i = 0 ; i < length; i++)
	{		

		if( *tmphtml != *temfind )
		{
			tmphtml++;

			continue;
		}

		while( *tmphtml == *temfind )
		{
			//printf(" %c.%c|",*tmphtml,*temfind);
			tmphtml++;
			temfind++;

			if( *temfind == (char)NULL ) // 找到了。
			{			
				return nStartPos + i;
			}
		}

		//printf("\n");	
		
		if( *temfind == (char)NULL ) // 找到了。
		{			
			return nStartPos + i;
		}
		else
		{	// 从与temfind首字节相同的那一位的后一位重新开始找，
			temfind = cfind;
			tmphtml = (char *)byhtml + nStartPos + i + 1;
		}
	}

	return -1;
}


// start 是指 '<' 所在的位置，end 指 '>'的位置，此函数取两个位置之间的那部分字符串。
int GetTmpLink(char * byhtml, char * temp, int start, int end,int max_num)
{
	int i;
	
	if(  byhtml == NULL ||  temp == NULL)
		return -1;

	if( end - start > max_num )
	{		
		temp[0] = (char)NULL;
		return -1;
	}	

	for(i = start + 1; i < end ; i++)
	{
		*temp = byhtml[i];

		temp++;
	}

	*temp = (char)NULL; // 结束符。

	return 1;
}

void onvif_url_ip_check_and_change(char * ip, char * url)
{
	int i = 0,j=0,k= 0;;
	int rel;
	char tmp_url[256] = {0};
	char tmp_url2[256] = {0};
	
	i = FindSTR(url,ip,0);
	if( i >= 0 )
		return ;

	i = FindSTR(url,"//",0);
	j = FindSTR(url,"/",i+3);
	if( j >= 0 && i >= 0 &&  i < j )
	{
		rel = GetTmpLink(url,tmp_url,i + strlen("//")-1,j,200);
		if( rel < 0)
		{
			
		}else
		{
			strcpy(tmp_url2,url);
			DPRINTK("get url %s\n",tmp_url);
			k = FindSTR(tmp_url,":",0);
			if( k< 0 )
			{
				tmp_url2[i+2] = 0;				
				sprintf(url,"%s%s%s",tmp_url2,ip,&tmp_url2[j]);
			}else
			{
				tmp_url2[i+2] = 0;				
				sprintf(url,"%s%s%s%s",tmp_url2,ip,&tmp_url[k],&tmp_url2[j]);
			}

			DPRINTK("create new url %s\n",url);			
			
		}
	}
}

int start_my_server(int port)
{
    int m, s; /* master and slave sockets */
    
    struct soap *soap;

    /* create context */
    soap = soap_new();
   // soap_register_plugin(soap, soap_wsa);
    /* register wsse plugin */
    //soap_register_plugin_arg(soap, soap_wsse, token_handler);
    //soap_set_omode(soap, SOAP_XML_INDENT); 
    //soap_set_omode(soap, SOAP_XML_CANONICAL);
   //  soap_set_namespaces(soap, namespaces); 	
    

    { 
        m = soap_bind(soap, NULL, port, 100);
        if (m < 0)
        {
            soap_print_fault(soap, stderr);
            exit(-1);
        }
        
        fprintf(stderr, "Socket connection successful: master socket = %d port=%d\n", m,port);
       while (soap_valid_socket(soap_accept(soap)))
	{
	   // soap_wsse_verify_auto(soap, SOAP_SMD_NONE, NULL, 0);
	    if (soap_serve(soap))
	  	{
	  		//soap_wsse_delete_Security(soap);
	    		soap_print_fault(soap, stderr);
	    		soap_print_fault_location(soap, stderr);
	  	}
		soap_destroy(soap);
		soap_end(soap);
	}
	   
	soap_print_fault(soap, stderr);
       exit(1);
    }
	

	return 1;
}

//#define USE_MODE_SERVER 


int OnvifAddPtz(IPCAM_INFO * pIpcamInfo);
int OnvifPtzMove(IPCAM_INFO * pIpcamInfo,float x,float y, float zoom_x);
int OnvifPtzStop(IPCAM_INFO * pIpcamInfo,int iPanTilt,int iZoom);
int OnvifPtzSetPreset(IPCAM_INFO * pIpcamInfo,char * pPresetName);
int OnvifPtzRemovePreset(IPCAM_INFO * pIpcamInfo,char * pPresetName);
int OnvifPtzGotoPreset(IPCAM_INFO * pIpcamInfo,char * pPresetName);

int net_recv_enc_data(void * point,char * buf,int length,int cmd)
{
	NET_MODULE_ST * pst = (NET_MODULE_ST *)point;
	BUF_FRAMEHEADER * pheadr = NULL;
	BUF_FRAMEHEADER fheadr;
	NET_COMMAND * pcmd = NULL;
	int chan = 0;		
	IPCAM_INFO * tmpinfo;
	
	if( cmd == NET_DVR_NET_COMMAND )
	{		
		pcmd = (NET_COMMAND *)buf;

		switch(pcmd->command)
		{
			case GET_CHAN_NUM:				
				break;	
			case IPCAM_CMD_GET_ALL_INFO:				
				memcpy(&g_pIpcamAll,buf+sizeof(NET_COMMAND),sizeof(IPCAM_ALL_INFO_ST));
				//g_pIpcamAll.net_st.
				get_ipcam_info_once = 1;
			
				g_pIpcamAll.onvif_st.__sizeProfiles = 2;
				break;				
			default:break;
		}
		
	}	

	
	return 1;
}

int check_double_url(char * url)
{
	int i = 0;

	for( i = 5; i < strlen(url);i++)
	{
		if( url[i] == ' ' )
		{
			DPRINTK("url %s cut!\n",url);
			url[i] = NULL;
			break;
		}
	}

	return 1;
}



int OnvifGetIpAndPort(char * ip,int *port,char * url)
{
	char tmp_ip[100] ={0};
	char tmp_port[100] = {0};
	char tmp_path[100] = {0};
	char tmp_url[100] ={0};
	int len = 0;
	int i = 0;
	int ip_pos =0,port_pos=0;

	check_double_url(url);

	strcpy(tmp_url,url);

	len = strlen(tmp_url);
	for( i = 0; i < len ; i++)
	{
		if( i > 1 )
		{
			if( tmp_url[i] == '/' && tmp_url[i-1] == 0 )
			{
				ip_pos = i + 1;

				tmp_url[i] = 0;
				tmp_url[i-1] = 0;
			}
		}
		if( tmp_url[i] == ':' && tmp_url[i+1] != '/' )
		{
			tmp_url[i] = 0;

			port_pos = i+1;
		}

		if( tmp_url[i] == '/' )
			tmp_url[i] = 0;
	}

	//DPRINTK("%d %d\n",ip_pos,port_pos);


	if( ip_pos != 0 )
		strcpy(ip,&tmp_url[ip_pos]);

	if( port_pos != 0 )
	{
		strcpy(tmp_port,&tmp_url[port_pos]);
		*port = atoi(tmp_port);
	}else
	{
		*port = 80;
	}


	if( ip_pos == 0 )
		return -1;	

	return 1;
}


int OnvifEditIpcamInfo(IPCAM_INFO * pIpcamInfo)
{
	IPCAM_INFO tmpinfo;
	int i;
	int big_enc_id = 0;
	int small_big_enc_id = 0;
	int width = 0;
	int chan = 0;

	for( i=0; i < pIpcamInfo->have_rtsp_num; i++)
	{
		//DPRINTK("width=%d %d %d\n",width,pIpcamInfo->width[i],i);
		
		if( pIpcamInfo->width[i] > width )
		{
			width = pIpcamInfo->width[i];
			big_enc_id = i;
		}
	}

	width =  pIpcamInfo->width[big_enc_id];

	for( i=0; i < pIpcamInfo->have_rtsp_num; i++)
	{
		if( big_enc_id == i )
			continue;

		//DPRINTK("width=%d %d %d\n",width,pIpcamInfo->width[i],i);
		
		if( pIpcamInfo->width[i] < width )
		{
			width = pIpcamInfo->width[i];
			small_big_enc_id = i;
		}
	}

	DPRINTK("big=%d small=%d\n",big_enc_id,small_big_enc_id);

	memcpy(&tmpinfo,pIpcamInfo,sizeof(tmpinfo));

	chan = 0;
	pIpcamInfo->width[chan] = tmpinfo.width[big_enc_id];
	pIpcamInfo->height[chan] = tmpinfo.height[big_enc_id];
	pIpcamInfo->bitstream[chan] = tmpinfo.bitstream[big_enc_id];
	pIpcamInfo->code[chan] = tmpinfo.code[big_enc_id];
	pIpcamInfo->gop[chan] = tmpinfo.gop[big_enc_id];
	pIpcamInfo->framerate[chan] = tmpinfo.framerate[big_enc_id];
	strcpy(pIpcamInfo->token[chan], tmpinfo.token[big_enc_id]);
	strcpy(pIpcamInfo->url[chan], tmpinfo.url[big_enc_id]);

	chan = 1;
	pIpcamInfo->width[chan] = tmpinfo.width[small_big_enc_id];
	pIpcamInfo->height[chan] = tmpinfo.height[small_big_enc_id];
	pIpcamInfo->bitstream[chan] = tmpinfo.bitstream[small_big_enc_id];
	pIpcamInfo->code[chan] = tmpinfo.code[small_big_enc_id];
	pIpcamInfo->gop[chan] = tmpinfo.gop[small_big_enc_id];
	pIpcamInfo->framerate[chan] = tmpinfo.framerate[small_big_enc_id];
	strcpy(pIpcamInfo->token[chan], tmpinfo.token[small_big_enc_id]);
	strcpy(pIpcamInfo->url[chan], tmpinfo.url[small_big_enc_id]);

	pIpcamInfo->have_rtsp_num = 2;

	if( small_big_enc_id == big_enc_id )
	{
		pIpcamInfo->have_rtsp_num = 1;
		pIpcamInfo->url[1][0] = 0;
	}	

	return 1;	
}

void OnvifEditCapbilityUrl(char * url)
{
	char tmp_url[100];
	int i = 0;
	int len = 0;
	int change = 0;

	if( url == NULL )
		return;

	strcpy(tmp_url,url);

	for( i = 0; i < strlen(tmp_url);i++)
	{
		if( tmp_url[i] == '.' )
		{
			if(tmp_url[i+1] == '0' && tmp_url[i+2] != '.')
			{
				//DPRINTK("change:%s ->\n",tmp_url);
				strcpy(&tmp_url[i+1],&tmp_url[i+2]);
				//DPRINTK("to:%s\n",tmp_url);

				change = 1;

				i--;
			}
		}

		if( i > 12)
		{
			if( tmp_url[i] == '/' )
				break;
		}
	}


	if( change )
	{	
		DPRINTK("change:%s -> %s\n",url,tmp_url);
		strcpy(url,tmp_url);
	}
	
}



#define USE_NVR_URL (1)

int OnvifGetIpcamAllInfo(IPCAM_INFO * pIpcamInfo)
{
	int ret;	

try_again:


	
	
	return 1;

failed:
	
	return -1;
}


void SendIpcamInfo()
{
	char send_buf[MN_MAX_BUF_SIZE];
	NET_COMMAND g_cmd;
	int send_len = 0;

	g_cmd.command = SET_IPCAM_INFO;
	g_cmd.play_fileId = pCommuniteToOnvif->chan_num;;

	memcpy(send_buf,(char*)&g_cmd,sizeof(g_cmd));
	memcpy(send_buf+sizeof(g_cmd),(char*)&stIpcamInfo_save[0],sizeof(IPCAM_INFO)*MAX_CHANNEL);
	send_len = sizeof(g_cmd) + sizeof(IPCAM_INFO)*MAX_CHANNEL;

	NM_net_send_data(pCommuniteToOnvif,(char*)send_buf,send_len,NET_DVR_NET_COMMAND);	
			
}

void SendMotionChan(int chan,int flag)
{
	char send_buf[MN_MAX_BUF_SIZE];
	NET_COMMAND g_cmd;
	int send_len = 0;

	g_cmd.command = MOTION_ALARM;
	g_cmd.play_fileId = chan;
	g_cmd.length = flag;

	memcpy(send_buf,(char*)&g_cmd,sizeof(g_cmd));	
	send_len = sizeof(g_cmd);

	if( pCommuniteToOnvif != NULL )
		NM_net_send_data(pCommuniteToOnvif,(char*)send_buf,send_len,NET_DVR_NET_COMMAND);	
}

int get_sys_time(struct timeval *tv,char * p)
{
	gettimeofday( tv, NULL );

	//DPRINTK("time = %d delay time %d ",tv->tv_sec, get_delay_sec_time());

	//tv->tv_sec += get_delay_sec_time();

	//DPRINTK("change time %d\n",tv->tv_sec);

	
	return 1;
}




#define MAX_PROBE_RECV_NUM (8126)

int onvif_recv_probe_info(IPCAM_INFO * pIpcamInfo)
{	
	char buf[MAX_PROBE_RECV_NUM];
	struct sockaddr_in serv_addr,addr;
	int bd;
	int ret;
	int len;
	int sockfd;
	int i;
	char client_ip[256];
	int CamHexId = 0;		
	char * pVersion = NULL;
	int wait_count = 0;
	char ipcam_ip[100];
	int ipcam_port=0;
	int endSearch = 0;

	pIpcamInfo->recognize_enable_motion_detect_ipcam = 0;
	
	sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
	if (sockfd == -1) 
	{ 
		DPRINTK("Error: socket"); 
		return; 
	} 
	memset(&addr, 0, sizeof(addr)); 
	addr.sin_family = AF_INET; 
	addr.sin_port = htons(3702); 
	addr.sin_addr.s_addr = htonl(INADDR_ANY);//inet_addr(host_name);//htonl(INADDR_ANY);// 

	bd = bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));


	{
		struct  linger  linger_set;
		struct timeval stTimeoutVal;		
		linger_set.l_onoff = 1;
		linger_set.l_linger = 0;

	    if (setsockopt( sockfd, SOL_SOCKET, SO_LINGER , (char *)&linger_set, sizeof(struct  linger) ) < 0 )
	    {
			printf( "setsockopt SO_LINGER  error\n" );
		}

		stTimeoutVal.tv_sec = 2;
		stTimeoutVal.tv_usec = 100;
		if ( setsockopt( sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&stTimeoutVal, sizeof(struct timeval) ) < 0 )
	    {
			printf( "setsockopt SO_SNDTIMEO error\n" );
		}
			 	
		if ( setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&stTimeoutVal, sizeof(struct timeval) ) < 0 )
	    {
			printf( "setsockopt SO_RCVTIMEO error\n" );
		}
	}


	DPRINTK("recv boardcast thread start!!\n");

	while(1)
	{
		len = sizeof(serv_addr); 
		memset(&serv_addr, 0, len);
		memset(buf,0,MAX_PROBE_RECV_NUM);	
		
		ret = recvfrom(sockfd,buf,sizeof(buf),0,(struct sockaddr*)&serv_addr,(socklen_t*)&len); 

		if( ret > 0 )
		{			
			//printf("RecvieIP %s buf=%s\n",inet_ntoa(serv_addr.sin_addr),buf);		
			int i = 0;
			int j = 0;
			int rel = 0;
			char tmp_url[256] = {0};
			i = FindSTR(buf,"<XAddrs>",0);
			j = FindSTR(buf,"</XAddrs>",0);
			if( j >= 0 && i >= 0 &&  i < j )
			{
				rel = GetTmpLink(buf,tmp_url,i + strlen("<XAddrs>")-1,j,200);
				if( rel < 0)
				{
					
				}else
				{
					DPRINTK("get url %s\n",tmp_url);
					if( OnvifGetIpAndPort(ipcam_ip,&ipcam_port,tmp_url) > 0 )
					{
						if( strcmp(pIpcamInfo->ip,ipcam_ip) == 0 )
						{
							pIpcamInfo->port = ipcam_port;
							endSearch = 1;

							{
								int z;
								z = FindSTR(buf,"RS7507H",0);

								if( z >= 0 )
									pIpcamInfo->recognize_enable_motion_detect_ipcam = 1;
							}
							break;
						}
					}
					
				}
			}else
			{
					i = FindSTR(buf,"<d:XAddrs>",0);
					j = FindSTR(buf,"</d:XAddrs>",0);
					if( j >= 0 && i >= 0 &&  i < j )
					{
						rel = GetTmpLink(buf,tmp_url,i + strlen("<d:XAddrs>")-1,j,200);
						if( rel < 0)
						{
							
						}else
						{
							DPRINTK("get url %s\n",tmp_url);
							if( OnvifGetIpAndPort(ipcam_ip,&ipcam_port,tmp_url) > 0 )
							{
								if( strcmp(pIpcamInfo->ip,ipcam_ip) == 0 )
								{
									pIpcamInfo->port = ipcam_port;
									endSearch = 1;

									{
										int z;
										z = FindSTR(buf,"RS7507H",0);

										if( z >= 0 )
											pIpcamInfo->recognize_enable_motion_detect_ipcam = 1;
									}
									
									break;
								}
							}
							
						}
					}
			}

			
		}else
		{
			DPRINTK("time out!\n");
			break;
		}

	}


	if( sockfd > 0)
	{
		close(sockfd);
		sockfd = 0;
	}


	if( endSearch == 1 )
		return 1;

	return -1;
	
}

int insert_ip_array(char * ip)
{	
	int i;	

	if( auto_check_ip_num >= MAX_CHANNEL )
		return 0;
	
	for(i = 0 ; i < auto_check_ip_num; i++)
	{		
		
		if( strcmp(auto_check_dvr_ip_array[i],ip) == 0 )		
			return 1;				
	}
	
	sprintf(auto_check_dvr_ip_array[auto_check_ip_num],"%s",ip);

	DPRINTK("Get %d  id=%s in array \n",auto_check_ip_num,
		auto_check_dvr_ip_array[auto_check_ip_num]);
	
	auto_check_ip_num++;
	
	return 1;
}


int onvif_recv_probe_info_get_ipcam_array()
{	
	char buf[MAX_PROBE_RECV_NUM];
	struct sockaddr_in serv_addr,addr;
	int bd;
	int ret;
	int len;
	int sockfd;
	int i;
	char client_ip[256];
	int CamHexId = 0;		
	char * pVersion = NULL;
	int wait_count = 0;
	char ipcam_ip[100];
	int ipcam_port=0;
	int endSearch = 0;

	
	sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
	if (sockfd == -1) 
	{ 
		DPRINTK("Error: socket"); 
		return; 
	} 
	memset(&addr, 0, sizeof(addr)); 
	addr.sin_family = AF_INET; 
	addr.sin_port = htons(3702); 
	addr.sin_addr.s_addr = htonl(INADDR_ANY);//inet_addr(host_name);//htonl(INADDR_ANY);// 

	bd = bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));


	{
		struct  linger  linger_set;
		struct timeval stTimeoutVal;		
		linger_set.l_onoff = 1;
		linger_set.l_linger = 0;

	    if (setsockopt( sockfd, SOL_SOCKET, SO_LINGER , (char *)&linger_set, sizeof(struct  linger) ) < 0 )
	    {
			printf( "setsockopt SO_LINGER  error\n" );
		}

		stTimeoutVal.tv_sec = 2;
		stTimeoutVal.tv_usec = 100;
		if ( setsockopt( sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&stTimeoutVal, sizeof(struct timeval) ) < 0 )
	    {
			printf( "setsockopt SO_SNDTIMEO error\n" );
		}
			 	
		if ( setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&stTimeoutVal, sizeof(struct timeval) ) < 0 )
	    {
			printf( "setsockopt SO_RCVTIMEO error\n" );
		}
	}


	DPRINTK("recv boardcast thread start!!\n");

	while(1)
	{
		len = sizeof(serv_addr); 
		memset(&serv_addr, 0, len);
		memset(buf,0,MAX_PROBE_RECV_NUM);	
		
		ret = recvfrom(sockfd,buf,sizeof(buf),0,(struct sockaddr*)&serv_addr,(socklen_t*)&len); 

		if( ret > 0 )
		{			
			//printf("RecvieIP %s buf=%s\n",inet_ntoa(serv_addr.sin_addr),buf);		
			int i = 0;
			int j = 0;
			int rel = 0;
			char tmp_url[256] = {0};
			i = FindSTR(buf,"<XAddrs>",0);
			j = FindSTR(buf,"</XAddrs>",0);
			if( j >= 0 && i >= 0 &&  i < j )
			{
				rel = GetTmpLink(buf,tmp_url,i + strlen("<XAddrs>")-1,j,200);
				if( rel < 0)
				{
					
				}else
				{
					//DPRINTK("get url %s\n",tmp_url);
					rel = FindSTR(inet_ntoa(serv_addr.sin_addr),host_gw_ip,0);
					if( rel >= 0 )
						insert_ip_array(inet_ntoa(serv_addr.sin_addr));
					else
						DPRINTK("ip not same area: %s\n",inet_ntoa(serv_addr.sin_addr));
					
				}
			}else
			{
					i = FindSTR(buf,"<d:XAddrs>",0);
					j = FindSTR(buf,"</d:XAddrs>",0);
					if( j >= 0 && i >= 0 &&  i < j )
					{
						rel = GetTmpLink(buf,tmp_url,i + strlen("<d:XAddrs>")-1,j,200);
						if( rel < 0)
						{
							
						}else
						{
							//DPRINTK("get url %s\n",tmp_url);
							rel = FindSTR(inet_ntoa(serv_addr.sin_addr),host_gw_ip,0);
							if( rel >= 0 )
								insert_ip_array(inet_ntoa(serv_addr.sin_addr));
							else
								DPRINTK("ip not same area: %s\n",inet_ntoa(serv_addr.sin_addr));
						}
					}
			}

			
		}else
		{
			DPRINTK("time out!\n");
			break;
		}

	}


	if( sockfd > 0)
	{
		close(sockfd);
		sockfd = 0;
	}


	if( endSearch == 1 )
		return 1;

	return -1;
	
}



int nvr_send_udp(char * buf)
{
	char * host_name = "239.255.255.250";	
	char * pVersion = NULL;
	int so_broadcast;
	char server_ip[100];
	char mac_id[100];
	int mac_hex_id = 0;
	int * pmac_hex_id = NULL;
	struct sockaddr_in serv_addr,addr;; 

	int sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
	if (sockfd == -1) 
	{ 
		perror("Error: socket"); 
		return -1; 
	}

	memset(&addr, 0, sizeof(addr)); 
	addr.sin_family = AF_INET; 
	addr.sin_port = htons(3702); 
	addr.sin_addr.s_addr = htonl(INADDR_ANY);//inet_addr(host_name);//htonl(INADDR_ANY);// 

	bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
	
	memset(&serv_addr, 0, sizeof(serv_addr)); 
	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(3702); 
	serv_addr.sin_addr.s_addr = inet_addr(host_name); 
	so_broadcast = 1;
	
	if( setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &so_broadcast,sizeof(so_broadcast)) < 0 )
	{
		DPRINTK("SO_BROADCAST ERROR!");
		return -1;
	}		

	sendto(sockfd,buf,strlen(buf),0,(struct sockaddr*)&serv_addr,sizeof(struct sockaddr_in));		
	
	if( sockfd > 0)
	{
		close(sockfd);
		sockfd = 0;
	}	

	return 1;
}




int nvr_send_udp2(char * ip,char * buf)
{
	char * host_name = "239.255.255.250";	
	char * pVersion = NULL;
	int so_broadcast;
	char server_ip[100];
	char mac_id[100];
	int mac_hex_id = 0;
	int * pmac_hex_id = NULL;
	struct sockaddr_in serv_addr,addr;; 

	int sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
	if (sockfd == -1) 
	{ 
		perror("Error: socket"); 
		return -1; 
	}

	memset(&addr, 0, sizeof(addr)); 
	addr.sin_family = AF_INET; 
	addr.sin_port = htons(3702); 
	addr.sin_addr.s_addr = htonl(INADDR_ANY);//inet_addr(host_name);//htonl(INADDR_ANY);// 

	bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
	
	memset(&serv_addr, 0, sizeof(serv_addr)); 
	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(3702); 
	serv_addr.sin_addr.s_addr = inet_addr(ip); 
	so_broadcast = 1;

	//DPRINTK("ip:%s\n",ip);
	
	if( setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &so_broadcast,sizeof(so_broadcast)) < 0 )
	{
		DPRINTK("SO_BROADCAST ERROR!");
		return -1;
	}		

	sendto(sockfd,buf,strlen(buf),0,(struct sockaddr*)&serv_addr,sizeof(struct sockaddr_in));		
	
	if( sockfd > 0)
	{
		close(sockfd);
		sockfd = 0;
	}	

	return 1;
}




int OnvifGetIPcamConnectPort2(IPCAM_INFO * pIpcamInfo)
{

//	pro_msg[]  = <?xml version="1.0" encoding="utf-8"?><Envelope xmlns:dn="http://www.onvif.org/ver10/network/wsdl" xmlns="http://www.w3.org/2003/05/soap-envelope"><Header><wsa:MessageID xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing">uuid:23e694dd-ca61-4f76-b286-401cbd7c6f0b</wsa:MessageID><wsa:To xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing">urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To><wsa:Action xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing">http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</wsa:Action></Header><Body><Probe xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns="http://schemas.xmlsoap.org/ws/2005/04/discovery"><Types>dn:NetworkVideoTransmitter</Types><Scopes /></Probe></Body></Envelope>

	char pro_msg[] = {
	0x3c, 0x3f, 0x78, 0x6d, 0x6c, 0x20, 0x76, 0x65, 
	0x72, 0x73, 0x69, 0x6f, 0x6e, 0x3d, 0x22, 0x31, 
	0x2e, 0x30, 0x22, 0x20, 0x65, 0x6e, 0x63, 0x6f, 
	0x64, 0x69, 0x6e, 0x67, 0x3d, 0x22, 0x75, 0x74, 
	0x66, 0x2d, 0x38, 0x22, 0x3f, 0x3e, 0x3c, 0x45, 
	0x6e, 0x76, 0x65, 0x6c, 0x6f, 0x70, 0x65, 0x20, 
	0x78, 0x6d, 0x6c, 0x6e, 0x73, 0x3a, 0x64, 0x6e, 
	0x3d, 0x22, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 
	0x2f, 0x77, 0x77, 0x77, 0x2e, 0x6f, 0x6e, 0x76, 
	0x69, 0x66, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 0x76, 
	0x65, 0x72, 0x31, 0x30, 0x2f, 0x6e, 0x65, 0x74, 
	0x77, 0x6f, 0x72, 0x6b, 0x2f, 0x77, 0x73, 0x64, 
	0x6c, 0x22, 0x20, 0x78, 0x6d, 0x6c, 0x6e, 0x73, 
	0x3d, 0x22, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 
	0x2f, 0x77, 0x77, 0x77, 0x2e, 0x77, 0x33, 0x2e, 
	0x6f, 0x72, 0x67, 0x2f, 0x32, 0x30, 0x30, 0x33, 
	0x2f, 0x30, 0x35, 0x2f, 0x73, 0x6f, 0x61, 0x70, 
	0x2d, 0x65, 0x6e, 0x76, 0x65, 0x6c, 0x6f, 0x70, 
	0x65, 0x22, 0x3e, 0x3c, 0x48, 0x65, 0x61, 0x64, 
	0x65, 0x72, 0x3e, 0x3c, 0x77, 0x73, 0x61, 0x3a, 
	0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x49, 
	0x44, 0x20, 0x78, 0x6d, 0x6c, 0x6e, 0x73, 0x3a, 
	0x77, 0x73, 0x61, 0x3d, 0x22, 0x68, 0x74, 0x74, 
	0x70, 0x3a, 0x2f, 0x2f, 0x73, 0x63, 0x68, 0x65, 
	0x6d, 0x61, 0x73, 0x2e, 0x78, 0x6d, 0x6c, 0x73, 
	0x6f, 0x61, 0x70, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 
	0x77, 0x73, 0x2f, 0x32, 0x30, 0x30, 0x34, 0x2f, 
	0x30, 0x38, 0x2f, 0x61, 0x64, 0x64, 0x72, 0x65, 
	0x73, 0x73, 0x69, 0x6e, 0x67, 0x22, 0x3e, 0x75, 
	0x75, 0x69, 0x64, 0x3a, 0x32, 0x33, 0x65, 0x36, 
	0x39, 0x34, 0x64, 0x64, 0x2d, 0x63, 0x61, 0x36, 
	0x31, 0x2d, 0x34, 0x66, 0x37, 0x36, 0x2d, 0x62, 
	0x32, 0x38, 0x36, 0x2d, 0x34, 0x30, 0x31, 0x63, 
	0x62, 0x64, 0x37, 0x63, 0x36, 0x66, 0x30, 0x62, 
	0x3c, 0x2f, 0x77, 0x73, 0x61, 0x3a, 0x4d, 0x65, 
	0x73, 0x73, 0x61, 0x67, 0x65, 0x49, 0x44, 0x3e, 
	0x3c, 0x77, 0x73, 0x61, 0x3a, 0x54, 0x6f, 0x20, 
	0x78, 0x6d, 0x6c, 0x6e, 0x73, 0x3a, 0x77, 0x73, 
	0x61, 0x3d, 0x22, 0x68, 0x74, 0x74, 0x70, 0x3a, 
	0x2f, 0x2f, 0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 
	0x73, 0x2e, 0x78, 0x6d, 0x6c, 0x73, 0x6f, 0x61, 
	0x70, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 0x77, 0x73, 
	0x2f, 0x32, 0x30, 0x30, 0x34, 0x2f, 0x30, 0x38, 
	0x2f, 0x61, 0x64, 0x64, 0x72, 0x65, 0x73, 0x73, 
	0x69, 0x6e, 0x67, 0x22, 0x3e, 0x75, 0x72, 0x6e, 
	0x3a, 0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 0x73, 
	0x2d, 0x78, 0x6d, 0x6c, 0x73, 0x6f, 0x61, 0x70, 
	0x2d, 0x6f, 0x72, 0x67, 0x3a, 0x77, 0x73, 0x3a, 
	0x32, 0x30, 0x30, 0x35, 0x3a, 0x30, 0x34, 0x3a, 
	0x64, 0x69, 0x73, 0x63, 0x6f, 0x76, 0x65, 0x72, 
	0x79, 0x3c, 0x2f, 0x77, 0x73, 0x61, 0x3a, 0x54, 
	0x6f, 0x3e, 0x3c, 0x77, 0x73, 0x61, 0x3a, 0x41, 
	0x63, 0x74, 0x69, 0x6f, 0x6e, 0x20, 0x78, 0x6d, 
	0x6c, 0x6e, 0x73, 0x3a, 0x77, 0x73, 0x61, 0x3d, 
	0x22, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 
	0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 0x73, 0x2e, 
	0x78, 0x6d, 0x6c, 0x73, 0x6f, 0x61, 0x70, 0x2e, 
	0x6f, 0x72, 0x67, 0x2f, 0x77, 0x73, 0x2f, 0x32, 
	0x30, 0x30, 0x34, 0x2f, 0x30, 0x38, 0x2f, 0x61, 
	0x64, 0x64, 0x72, 0x65, 0x73, 0x73, 0x69, 0x6e, 
	0x67, 0x22, 0x3e, 0x68, 0x74, 0x74, 0x70, 0x3a, 
	0x2f, 0x2f, 0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 
	0x73, 0x2e, 0x78, 0x6d, 0x6c, 0x73, 0x6f, 0x61, 
	0x70, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 0x77, 0x73, 
	0x2f, 0x32, 0x30, 0x30, 0x35, 0x2f, 0x30, 0x34, 
	0x2f, 0x64, 0x69, 0x73, 0x63, 0x6f, 0x76, 0x65, 
	0x72, 0x79, 0x2f, 0x50, 0x72, 0x6f, 0x62, 0x65, 
	0x3c, 0x2f, 0x77, 0x73, 0x61, 0x3a, 0x41, 0x63, 
	0x74, 0x69, 0x6f, 0x6e, 0x3e, 0x3c, 0x2f, 0x48, 
	0x65, 0x61, 0x64, 0x65, 0x72, 0x3e, 0x3c, 0x42, 
	0x6f, 0x64, 0x79, 0x3e, 0x3c, 0x50, 0x72, 0x6f, 
	0x62, 0x65, 0x20, 0x78, 0x6d, 0x6c, 0x6e, 0x73, 
	0x3a, 0x78, 0x73, 0x69, 0x3d, 0x22, 0x68, 0x74, 
	0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x77, 0x77, 0x77, 
	0x2e, 0x77, 0x33, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 
	0x32, 0x30, 0x30, 0x31, 0x2f, 0x58, 0x4d, 0x4c, 
	0x53, 0x63, 0x68, 0x65, 0x6d, 0x61, 0x2d, 0x69, 
	0x6e, 0x73, 0x74, 0x61, 0x6e, 0x63, 0x65, 0x22, 
	0x20, 0x78, 0x6d, 0x6c, 0x6e, 0x73, 0x3a, 0x78, 
	0x73, 0x64, 0x3d, 0x22, 0x68, 0x74, 0x74, 0x70, 
	0x3a, 0x2f, 0x2f, 0x77, 0x77, 0x77, 0x2e, 0x77, 
	0x33, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 0x32, 0x30, 
	0x30, 0x31, 0x2f, 0x58, 0x4d, 0x4c, 0x53, 0x63, 
	0x68, 0x65, 0x6d, 0x61, 0x22, 0x20, 0x78, 0x6d, 
	0x6c, 0x6e, 0x73, 0x3d, 0x22, 0x68, 0x74, 0x74, 
	0x70, 0x3a, 0x2f, 0x2f, 0x73, 0x63, 0x68, 0x65, 
	0x6d, 0x61, 0x73, 0x2e, 0x78, 0x6d, 0x6c, 0x73, 
	0x6f, 0x61, 0x70, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 
	0x77, 0x73, 0x2f, 0x32, 0x30, 0x30, 0x35, 0x2f, 
	0x30, 0x34, 0x2f, 0x64, 0x69, 0x73, 0x63, 0x6f, 
	0x76, 0x65, 0x72, 0x79, 0x22, 0x3e, 0x3c, 0x54, 
	0x79, 0x70, 0x65, 0x73, 0x3e, 0x64, 0x6e, 0x3a, 
	0x4e, 0x65, 0x74, 0x77, 0x6f, 0x72, 0x6b, 0x56, 
	0x69, 0x64, 0x65, 0x6f, 0x54, 0x72, 0x61, 0x6e, 
	0x73, 0x6d, 0x69, 0x74, 0x74, 0x65, 0x72, 0x3c, 
	0x2f, 0x54, 0x79, 0x70, 0x65, 0x73, 0x3e, 0x3c, 
	0x53, 0x63, 0x6f, 0x70, 0x65, 0x73, 0x20, 0x2f, 
	0x3e, 0x3c, 0x2f, 0x50, 0x72, 0x6f, 0x62, 0x65, 
	0x3e, 0x3c, 0x2f, 0x42, 0x6f, 0x64, 0x79, 0x3e, 
	0x3c, 0x2f, 0x45, 0x6e, 0x76, 0x65, 0x6c, 0x6f, 
	0x70, 0x65, 0x3e ,0x00};

	pro_msg[0xec] = pro_msg[0xec] + 1;
	nvr_send_udp(pro_msg);
	pro_msg[0xec] = pro_msg[0xec] - 1;
	nvr_send_udp(pro_msg);	

	return onvif_recv_probe_info(pIpcamInfo);
}



int OnvifGetIPcamConnectPort3(IPCAM_INFO * pIpcamInfo)
{

//	pro_msg[]  = <?xml version="1.0" encoding="utf-8"?><Envelope xmlns:dn="http://www.onvif.org/ver10/network/wsdl" xmlns="http://www.w3.org/2003/05/soap-envelope"><Header><wsa:MessageID xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing">uuid:23e694dd-ca61-4f76-b286-401cbd7c6f0b</wsa:MessageID><wsa:To xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing">urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To><wsa:Action xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing">http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</wsa:Action></Header><Body><Probe xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns="http://schemas.xmlsoap.org/ws/2005/04/discovery"><Types>dn:NetworkVideoTransmitter</Types><Scopes /></Probe></Body></Envelope>

char pro_msg[] = {
0x3c, 0x3f, 0x78, 0x6d, 0x6c, 0x20, 0x76, 0x65, 
0x72, 0x73, 0x69, 0x6f, 0x6e, 0x3d, 0x22, 0x31, 
0x2e, 0x30, 0x22, 0x20, 0x65, 0x6e, 0x63, 0x6f, 
0x64, 0x69, 0x6e, 0x67, 0x3d, 0x22, 0x75, 0x74, 
0x66, 0x2d, 0x38, 0x22, 0x3f, 0x3e, 0x3c, 0x45, 
0x6e, 0x76, 0x65, 0x6c, 0x6f, 0x70, 0x65, 0x20, 
0x78, 0x6d, 0x6c, 0x6e, 0x73, 0x3a, 0x64, 0x6e, 
0x3d, 0x22, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 
0x2f, 0x77, 0x77, 0x77, 0x2e, 0x6f, 0x6e, 0x76, 
0x69, 0x66, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 0x76, 
0x65, 0x72, 0x31, 0x30, 0x2f, 0x6e, 0x65, 0x74, 
0x77, 0x6f, 0x72, 0x6b, 0x2f, 0x77, 0x73, 0x64, 
0x6c, 0x22, 0x20, 0x78, 0x6d, 0x6c, 0x6e, 0x73, 
0x3d, 0x22, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 
0x2f, 0x77, 0x77, 0x77, 0x2e, 0x77, 0x33, 0x2e, 
0x6f, 0x72, 0x67, 0x2f, 0x32, 0x30, 0x30, 0x33, 
0x2f, 0x30, 0x35, 0x2f, 0x73, 0x6f, 0x61, 0x70, 
0x2d, 0x65, 0x6e, 0x76, 0x65, 0x6c, 0x6f, 0x70, 
0x65, 0x22, 0x3e, 0x3c, 0x48, 0x65, 0x61, 0x64, 
0x65, 0x72, 0x3e, 0x3c, 0x77, 0x73, 0x61, 0x3a, 
0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x49, 
0x44, 0x20, 0x78, 0x6d, 0x6c, 0x6e, 0x73, 0x3a, 
0x77, 0x73, 0x61, 0x3d, 0x22, 0x68, 0x74, 0x74, 
0x70, 0x3a, 0x2f, 0x2f, 0x73, 0x63, 0x68, 0x65, 
0x6d, 0x61, 0x73, 0x2e, 0x78, 0x6d, 0x6c, 0x73, 
0x6f, 0x61, 0x70, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 
0x77, 0x73, 0x2f, 0x32, 0x30, 0x30, 0x34, 0x2f, 
0x30, 0x38, 0x2f, 0x61, 0x64, 0x64, 0x72, 0x65, 
0x73, 0x73, 0x69, 0x6e, 0x67, 0x22, 0x3e, 0x75, 
0x75, 0x69, 0x64, 0x3a, 0x66, 0x65, 0x33, 0x38, 
0x34, 0x31, 0x65, 0x37, 0x2d, 0x33, 0x36, 0x35, 
0x66, 0x2d, 0x34, 0x36, 0x64, 0x64, 0x2d, 0x39, 
0x32, 0x39, 0x39, 0x2d, 0x30, 0x36, 0x64, 0x30, 
0x38, 0x66, 0x63, 0x37, 0x61, 0x33, 0x33, 0x38, 
0x3c, 0x2f, 0x77, 0x73, 0x61, 0x3a, 0x4d, 0x65, 
0x73, 0x73, 0x61, 0x67, 0x65, 0x49, 0x44, 0x3e, 
0x3c, 0x77, 0x73, 0x61, 0x3a, 0x54, 0x6f, 0x20, 
0x78, 0x6d, 0x6c, 0x6e, 0x73, 0x3a, 0x77, 0x73, 
0x61, 0x3d, 0x22, 0x68, 0x74, 0x74, 0x70, 0x3a, 
0x2f, 0x2f, 0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 
0x73, 0x2e, 0x78, 0x6d, 0x6c, 0x73, 0x6f, 0x61, 
0x70, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 0x77, 0x73, 
0x2f, 0x32, 0x30, 0x30, 0x34, 0x2f, 0x30, 0x38, 
0x2f, 0x61, 0x64, 0x64, 0x72, 0x65, 0x73, 0x73, 
0x69, 0x6e, 0x67, 0x22, 0x3e, 0x75, 0x72, 0x6e, 
0x3a, 0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 0x73, 
0x2d, 0x78, 0x6d, 0x6c, 0x73, 0x6f, 0x61, 0x70, 
0x2d, 0x6f, 0x72, 0x67, 0x3a, 0x77, 0x73, 0x3a, 
0x32, 0x30, 0x30, 0x35, 0x3a, 0x30, 0x34, 0x3a, 
0x64, 0x69, 0x73, 0x63, 0x6f, 0x76, 0x65, 0x72, 
0x79, 0x3c, 0x2f, 0x77, 0x73, 0x61, 0x3a, 0x54, 
0x6f, 0x3e, 0x3c, 0x77, 0x73, 0x61, 0x3a, 0x41, 
0x63, 0x74, 0x69, 0x6f, 0x6e, 0x20, 0x78, 0x6d, 
0x6c, 0x6e, 0x73, 0x3a, 0x77, 0x73, 0x61, 0x3d, 
0x22, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 
0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 0x73, 0x2e, 
0x78, 0x6d, 0x6c, 0x73, 0x6f, 0x61, 0x70, 0x2e, 
0x6f, 0x72, 0x67, 0x2f, 0x77, 0x73, 0x2f, 0x32, 
0x30, 0x30, 0x34, 0x2f, 0x30, 0x38, 0x2f, 0x61, 
0x64, 0x64, 0x72, 0x65, 0x73, 0x73, 0x69, 0x6e, 
0x67, 0x22, 0x3e, 0x68, 0x74, 0x74, 0x70, 0x3a, 
0x2f, 0x2f, 0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 
0x73, 0x2e, 0x78, 0x6d, 0x6c, 0x73, 0x6f, 0x61, 
0x70, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 0x77, 0x73, 
0x2f, 0x32, 0x30, 0x30, 0x35, 0x2f, 0x30, 0x34, 
0x2f, 0x64, 0x69, 0x73, 0x63, 0x6f, 0x76, 0x65, 
0x72, 0x79, 0x2f, 0x50, 0x72, 0x6f, 0x62, 0x65, 
0x3c, 0x2f, 0x77, 0x73, 0x61, 0x3a, 0x41, 0x63, 
0x74, 0x69, 0x6f, 0x6e, 0x3e, 0x3c, 0x2f, 0x48, 
0x65, 0x61, 0x64, 0x65, 0x72, 0x3e, 0x3c, 0x42, 
0x6f, 0x64, 0x79, 0x3e, 0x3c, 0x50, 0x72, 0x6f, 
0x62, 0x65, 0x20, 0x78, 0x6d, 0x6c, 0x6e, 0x73, 
0x3a, 0x78, 0x73, 0x69, 0x3d, 0x22, 0x68, 0x74, 
0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x77, 0x77, 0x77, 
0x2e, 0x77, 0x33, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 
0x32, 0x30, 0x30, 0x31, 0x2f, 0x58, 0x4d, 0x4c, 
0x53, 0x63, 0x68, 0x65, 0x6d, 0x61, 0x2d, 0x69, 
0x6e, 0x73, 0x74, 0x61, 0x6e, 0x63, 0x65, 0x22, 
0x20, 0x78, 0x6d, 0x6c, 0x6e, 0x73, 0x3a, 0x78, 
0x73, 0x64, 0x3d, 0x22, 0x68, 0x74, 0x74, 0x70, 
0x3a, 0x2f, 0x2f, 0x77, 0x77, 0x77, 0x2e, 0x77, 
0x33, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 0x32, 0x30, 
0x30, 0x31, 0x2f, 0x58, 0x4d, 0x4c, 0x53, 0x63, 
0x68, 0x65, 0x6d, 0x61, 0x22, 0x20, 0x78, 0x6d, 
0x6c, 0x6e, 0x73, 0x3d, 0x22, 0x68, 0x74, 0x74, 
0x70, 0x3a, 0x2f, 0x2f, 0x73, 0x63, 0x68, 0x65, 
0x6d, 0x61, 0x73, 0x2e, 0x78, 0x6d, 0x6c, 0x73, 
0x6f, 0x61, 0x70, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 
0x77, 0x73, 0x2f, 0x32, 0x30, 0x30, 0x35, 0x2f, 
0x30, 0x34, 0x2f, 0x64, 0x69, 0x73, 0x63, 0x6f, 
0x76, 0x65, 0x72, 0x79, 0x22, 0x3e, 0x3c, 0x54, 
0x79, 0x70, 0x65, 0x73, 0x3e, 0x64, 0x6e, 0x3a, 
0x4e, 0x65, 0x74, 0x77, 0x6f, 0x72, 0x6b, 0x56, 
0x69, 0x64, 0x65, 0x6f, 0x54, 0x72, 0x61, 0x6e, 
0x73, 0x6d, 0x69, 0x74, 0x74, 0x65, 0x72, 0x3c, 
0x2f, 0x54, 0x79, 0x70, 0x65, 0x73, 0x3e, 0x3c, 
0x53, 0x63, 0x6f, 0x70, 0x65, 0x73, 0x20, 0x2f, 
0x3e, 0x3c, 0x2f, 0x50, 0x72, 0x6f, 0x62, 0x65, 
0x3e, 0x3c, 0x2f, 0x42, 0x6f, 0x64, 0x79, 0x3e, 
0x3c, 0x2f, 0x45, 0x6e, 0x76, 0x65, 0x6c, 0x6f, 
0x70, 0x65, 0x3e, 0x00};

	pro_msg[0xec] = pro_msg[0xec] + 1;
	nvr_send_udp2(pIpcamInfo->ip,pro_msg);
	pro_msg[0xec] = pro_msg[0xec] - 1;
	nvr_send_udp2(pIpcamInfo->ip,pro_msg);

	return onvif_recv_probe_info(pIpcamInfo);
}


static int read_interface(char *interface, int *ifindex, u_int32_t *addr, unsigned char *arp)
{
 int fd;
 struct ifreq ifr;
 struct sockaddr_in *our_ip;

 memset(&ifr, 0, sizeof(struct ifreq));
 if((fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) >= 0) {
 ifr.ifr_addr.sa_family = AF_INET;
 strcpy(ifr.ifr_name, interface);

 /*this is not execute*/
 if (addr) {  
 if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
 our_ip = (struct sockaddr_in *) &ifr.ifr_addr;
 *addr = our_ip->sin_addr.s_addr;
 DPRINTK( "%s (our ip) = %s", ifr.ifr_name, inet_ntoa(our_ip->sin_addr));
 } else {
 DPRINTK("SIOCGIFADDR failed, is the interface up and configured?: %s",  
 strerror(errno));
 return -1;
 }
 }

 if (ioctl(fd, SIOCGIFINDEX, &ifr) == 0) {
DPRINTK( "adapter index %d", ifr.ifr_ifindex);
 *ifindex = ifr.ifr_ifindex;
 } else {
DPRINTK( "SIOCGIFINDEX failed!: %s", strerror(errno));
 return -1;
 }
 if (ioctl(fd, SIOCGIFHWADDR, &ifr) == 0) {
 memcpy(arp, ifr.ifr_hwaddr.sa_data, 6);
DPRINTK( "adapter hardware address %02x:%02x:%02x:%02x:%02x:%02x",
 arp[0], arp[1], arp[2], arp[3], arp[4], arp[5]);
 } else {
DPRINTK( "SIOCGIFHWADDR failed!: %s", strerror(errno));
 return -1;
 }
 }
 else {
 DPRINTK( "socket failed!: %s", strerror(errno));
 return -1;
 }
 close(fd);
 return 0;
}

int ipcam_get_host_ip(char * dev,unsigned char * host_ip)
{
	struct server_config_t source_config;
	struct in_addr temp;

	 if (read_interface(dev, &source_config.ifindex,&source_config.server, source_config.arp) < 0)
	 {	 	
		DPRINTK("get %s info error!\n", dev);
		return -1;
	 }		

	 memcpy(((u_int *) host_ip),&source_config.server ,sizeof(source_config.server));

	return 1;
}

int GetOnvifAvailableIpcam()
{
	char pro_msg[] = {
0x3c, 0x3f, 0x78, 0x6d, 0x6c, 0x20, 0x76, 0x65, 
0x72, 0x73, 0x69, 0x6f, 0x6e, 0x3d, 0x22, 0x31, 
0x2e, 0x30, 0x22, 0x20, 0x65, 0x6e, 0x63, 0x6f, 
0x64, 0x69, 0x6e, 0x67, 0x3d, 0x22, 0x75, 0x74, 
0x66, 0x2d, 0x38, 0x22, 0x3f, 0x3e, 0x3c, 0x45, 
0x6e, 0x76, 0x65, 0x6c, 0x6f, 0x70, 0x65, 0x20, 
0x78, 0x6d, 0x6c, 0x6e, 0x73, 0x3a, 0x64, 0x6e, 
0x3d, 0x22, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 
0x2f, 0x77, 0x77, 0x77, 0x2e, 0x6f, 0x6e, 0x76, 
0x69, 0x66, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 0x76, 
0x65, 0x72, 0x31, 0x30, 0x2f, 0x6e, 0x65, 0x74, 
0x77, 0x6f, 0x72, 0x6b, 0x2f, 0x77, 0x73, 0x64, 
0x6c, 0x22, 0x20, 0x78, 0x6d, 0x6c, 0x6e, 0x73, 
0x3d, 0x22, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 
0x2f, 0x77, 0x77, 0x77, 0x2e, 0x77, 0x33, 0x2e, 
0x6f, 0x72, 0x67, 0x2f, 0x32, 0x30, 0x30, 0x33, 
0x2f, 0x30, 0x35, 0x2f, 0x73, 0x6f, 0x61, 0x70, 
0x2d, 0x65, 0x6e, 0x76, 0x65, 0x6c, 0x6f, 0x70, 
0x65, 0x22, 0x3e, 0x3c, 0x48, 0x65, 0x61, 0x64, 
0x65, 0x72, 0x3e, 0x3c, 0x77, 0x73, 0x61, 0x3a, 
0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x49, 
0x44, 0x20, 0x78, 0x6d, 0x6c, 0x6e, 0x73, 0x3a, 
0x77, 0x73, 0x61, 0x3d, 0x22, 0x68, 0x74, 0x74, 
0x70, 0x3a, 0x2f, 0x2f, 0x73, 0x63, 0x68, 0x65, 
0x6d, 0x61, 0x73, 0x2e, 0x78, 0x6d, 0x6c, 0x73, 
0x6f, 0x61, 0x70, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 
0x77, 0x73, 0x2f, 0x32, 0x30, 0x30, 0x34, 0x2f, 
0x30, 0x38, 0x2f, 0x61, 0x64, 0x64, 0x72, 0x65, 
0x73, 0x73, 0x69, 0x6e, 0x67, 0x22, 0x3e, 0x75, 
0x75, 0x69, 0x64, 0x3a, 0x66, 0x65, 0x33, 0x38, 
0x34, 0x31, 0x65, 0x37, 0x2d, 0x33, 0x36, 0x35, 
0x66, 0x2d, 0x34, 0x36, 0x64, 0x64, 0x2d, 0x39, 
0x32, 0x39, 0x39, 0x2d, 0x30, 0x36, 0x64, 0x30, 
0x38, 0x66, 0x63, 0x37, 0x61, 0x33, 0x33, 0x38, 
0x3c, 0x2f, 0x77, 0x73, 0x61, 0x3a, 0x4d, 0x65, 
0x73, 0x73, 0x61, 0x67, 0x65, 0x49, 0x44, 0x3e, 
0x3c, 0x77, 0x73, 0x61, 0x3a, 0x54, 0x6f, 0x20, 
0x78, 0x6d, 0x6c, 0x6e, 0x73, 0x3a, 0x77, 0x73, 
0x61, 0x3d, 0x22, 0x68, 0x74, 0x74, 0x70, 0x3a, 
0x2f, 0x2f, 0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 
0x73, 0x2e, 0x78, 0x6d, 0x6c, 0x73, 0x6f, 0x61, 
0x70, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 0x77, 0x73, 
0x2f, 0x32, 0x30, 0x30, 0x34, 0x2f, 0x30, 0x38, 
0x2f, 0x61, 0x64, 0x64, 0x72, 0x65, 0x73, 0x73, 
0x69, 0x6e, 0x67, 0x22, 0x3e, 0x75, 0x72, 0x6e, 
0x3a, 0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 0x73, 
0x2d, 0x78, 0x6d, 0x6c, 0x73, 0x6f, 0x61, 0x70, 
0x2d, 0x6f, 0x72, 0x67, 0x3a, 0x77, 0x73, 0x3a, 
0x32, 0x30, 0x30, 0x35, 0x3a, 0x30, 0x34, 0x3a, 
0x64, 0x69, 0x73, 0x63, 0x6f, 0x76, 0x65, 0x72, 
0x79, 0x3c, 0x2f, 0x77, 0x73, 0x61, 0x3a, 0x54, 
0x6f, 0x3e, 0x3c, 0x77, 0x73, 0x61, 0x3a, 0x41, 
0x63, 0x74, 0x69, 0x6f, 0x6e, 0x20, 0x78, 0x6d, 
0x6c, 0x6e, 0x73, 0x3a, 0x77, 0x73, 0x61, 0x3d, 
0x22, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 
0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 0x73, 0x2e, 
0x78, 0x6d, 0x6c, 0x73, 0x6f, 0x61, 0x70, 0x2e, 
0x6f, 0x72, 0x67, 0x2f, 0x77, 0x73, 0x2f, 0x32, 
0x30, 0x30, 0x34, 0x2f, 0x30, 0x38, 0x2f, 0x61, 
0x64, 0x64, 0x72, 0x65, 0x73, 0x73, 0x69, 0x6e, 
0x67, 0x22, 0x3e, 0x68, 0x74, 0x74, 0x70, 0x3a, 
0x2f, 0x2f, 0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 
0x73, 0x2e, 0x78, 0x6d, 0x6c, 0x73, 0x6f, 0x61, 
0x70, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 0x77, 0x73, 
0x2f, 0x32, 0x30, 0x30, 0x35, 0x2f, 0x30, 0x34, 
0x2f, 0x64, 0x69, 0x73, 0x63, 0x6f, 0x76, 0x65, 
0x72, 0x79, 0x2f, 0x50, 0x72, 0x6f, 0x62, 0x65, 
0x3c, 0x2f, 0x77, 0x73, 0x61, 0x3a, 0x41, 0x63, 
0x74, 0x69, 0x6f, 0x6e, 0x3e, 0x3c, 0x2f, 0x48, 
0x65, 0x61, 0x64, 0x65, 0x72, 0x3e, 0x3c, 0x42, 
0x6f, 0x64, 0x79, 0x3e, 0x3c, 0x50, 0x72, 0x6f, 
0x62, 0x65, 0x20, 0x78, 0x6d, 0x6c, 0x6e, 0x73, 
0x3a, 0x78, 0x73, 0x69, 0x3d, 0x22, 0x68, 0x74, 
0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x77, 0x77, 0x77, 
0x2e, 0x77, 0x33, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 
0x32, 0x30, 0x30, 0x31, 0x2f, 0x58, 0x4d, 0x4c, 
0x53, 0x63, 0x68, 0x65, 0x6d, 0x61, 0x2d, 0x69, 
0x6e, 0x73, 0x74, 0x61, 0x6e, 0x63, 0x65, 0x22, 
0x20, 0x78, 0x6d, 0x6c, 0x6e, 0x73, 0x3a, 0x78, 
0x73, 0x64, 0x3d, 0x22, 0x68, 0x74, 0x74, 0x70, 
0x3a, 0x2f, 0x2f, 0x77, 0x77, 0x77, 0x2e, 0x77, 
0x33, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 0x32, 0x30, 
0x30, 0x31, 0x2f, 0x58, 0x4d, 0x4c, 0x53, 0x63, 
0x68, 0x65, 0x6d, 0x61, 0x22, 0x20, 0x78, 0x6d, 
0x6c, 0x6e, 0x73, 0x3d, 0x22, 0x68, 0x74, 0x74, 
0x70, 0x3a, 0x2f, 0x2f, 0x73, 0x63, 0x68, 0x65, 
0x6d, 0x61, 0x73, 0x2e, 0x78, 0x6d, 0x6c, 0x73, 
0x6f, 0x61, 0x70, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 
0x77, 0x73, 0x2f, 0x32, 0x30, 0x30, 0x35, 0x2f, 
0x30, 0x34, 0x2f, 0x64, 0x69, 0x73, 0x63, 0x6f, 
0x76, 0x65, 0x72, 0x79, 0x22, 0x3e, 0x3c, 0x54, 
0x79, 0x70, 0x65, 0x73, 0x3e, 0x64, 0x6e, 0x3a, 
0x4e, 0x65, 0x74, 0x77, 0x6f, 0x72, 0x6b, 0x56, 
0x69, 0x64, 0x65, 0x6f, 0x54, 0x72, 0x61, 0x6e, 
0x73, 0x6d, 0x69, 0x74, 0x74, 0x65, 0x72, 0x3c, 
0x2f, 0x54, 0x79, 0x70, 0x65, 0x73, 0x3e, 0x3c, 
0x53, 0x63, 0x6f, 0x70, 0x65, 0x73, 0x20, 0x2f, 
0x3e, 0x3c, 0x2f, 0x50, 0x72, 0x6f, 0x62, 0x65, 
0x3e, 0x3c, 0x2f, 0x42, 0x6f, 0x64, 0x79, 0x3e, 
0x3c, 0x2f, 0x45, 0x6e, 0x76, 0x65, 0x6c, 0x6f, 
0x70, 0x65, 0x3e, 0x00};

	pro_msg[0xec] = pro_msg[0xec] + 1;
	nvr_send_udp2("255.255.255.255",pro_msg);
	pro_msg[0xec] = pro_msg[0xec] - 1;
	nvr_send_udp2("255.255.255.255",pro_msg);
	pro_msg[0xec] = pro_msg[0xec] + 3;
	nvr_send_udp2("255.255.255.255",pro_msg);

	return onvif_recv_probe_info_get_ipcam_array();
}

void SendScanOnvifArray()
{
	char send_buf[MN_MAX_BUF_SIZE];
	NET_COMMAND g_cmd;
	int send_len = 0;

	g_cmd.command = SCAN_ONVIF_IPCAM;
	g_cmd.play_fileId = auto_check_ip_num;

	memcpy(send_buf,(char*)&g_cmd,sizeof(g_cmd));
	memcpy(send_buf+sizeof(g_cmd),(char*)&auto_check_dvr_ip_array[0],50*MAX_CHANNEL);
	send_len = sizeof(g_cmd) + 50*MAX_CHANNEL;

	NM_net_send_data(pCommuniteToOnvif,(char*)send_buf,send_len,NET_DVR_NET_COMMAND);	
			
}



int GetOnvifAvailableIpcamArray()
{
	int i =0;		
	unsigned char my_ip[50] = "";
	
	for(i = 0 ; i < MAX_CHANNEL; i++)
	{
		auto_check_dvr_ip_array[i][0] = NULL;
	}

	auto_check_ip_num = 0;

	host_gw_ip[0] = NULL;

	if( ipcam_get_host_ip("eth0",my_ip) < 0 )
	{
		DPRINTK("ipcam_get_host_ip get error!\n");		
		return ;
	}

	if( my_ip[0] != NULL )
	{
		sprintf(host_gw_ip,"%d.%d.%d.%d",my_ip[0],my_ip[1],my_ip[2],my_ip[3]);
	}

	if( host_gw_ip[0] != 0 )
	{
		int length = 0;
		
		length = strlen(host_gw_ip);
		for( i = length -1; i > 0; i--)
		{
			if( host_gw_ip[i] == '.' )
			{
				host_gw_ip[i] = 0;
				break;
			}
		}
	}

	DPRINTK("get host ip : %s\n",host_gw_ip);


	GetOnvifAvailableIpcam();	

	{
		int tmp_id;
		int tmp_id2;
		int i,j;
		char tmp_buf[100];

		if( auto_check_ip_num > 0 )
		{
			for( i = 0; i < auto_check_ip_num; i++)
			{
				int pot_num = 0;
				for(j = 0; j < strlen(auto_check_dvr_ip_array[i]);j++)
				{
					if( auto_check_dvr_ip_array[i][j] == '.' )
					{
						pot_num++;
						if( pot_num == 3 )
						{
							strcpy(tmp_buf,&auto_check_dvr_ip_array[i][j+1]);
							//DPRINTK("tmp_buf=%s\n",tmp_buf);
							sprintf(auto_check_dvr_ip_array[i],"%0.8d",atoi(tmp_buf));
							//DPRINTK("tmp_buf2=%s\n",auto_check_dvr_ip_array[i]);
							break;
						}
					}
				}
			}
		}

		//排序
		if( auto_check_ip_num > 1 )
		{
			for( i = 0; i < auto_check_ip_num - 1; i++)
			{
				for( j = i+1; j < auto_check_ip_num; j++)
				{
					sscanf(auto_check_dvr_ip_array[i],"%X",&tmp_id);
					sscanf(auto_check_dvr_ip_array[j],"%X",&tmp_id2);

					if( tmp_id > tmp_id2 )
					{
						strcpy(tmp_buf,auto_check_dvr_ip_array[i]);
						strcpy(auto_check_dvr_ip_array[i],auto_check_dvr_ip_array[j]);
						strcpy(auto_check_dvr_ip_array[j],tmp_buf);
					}
				}
			}				
		}

		for( i = 0; i < auto_check_ip_num; i++)
		{	
			auto_check_dvr_ip_array[i][0] = 'V';
			DPRINTK("ip array=%s\n",auto_check_dvr_ip_array[i]);
		}
	}

	SendScanOnvifArray();

	return 1;
}



int start_udp_server()
{
/*
     struct soap * soap;
     struct ip_mreq mcast;
	
     

	for(;;)
	{ 
		soap = soap_new();
		
		soap_init2(soap, SOAP_IO_UDP|SOAP_IO_FLUSH, SOAP_IO_UDP|SOAP_IO_FLUSH);
		//soap_init1(soap, SOAP_IO_UDP);

		//soap_register_plugin(soap, soap_wsa);
	    // soap_set_namespaces(&soap, namespaces); 		
		
		if(!soap_valid_socket(soap_bind(soap, NULL, 3702, 100)))
		{ 
			soap_print_fault(&soap, stderr);
			exit(1);
		}
		
		mcast.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
		mcast.imr_interface.s_addr = htonl(INADDR_ANY);
		if(setsockopt(soap->master, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mcast, sizeof(mcast)) < 0)
		{
			printf("setsockopt error!\n");
			return 0;
		}	
		printf("Accepting requests...\n");
		if(soap_serve(soap))
			soap_print_fault(soap, stderr);

		DPRINTK("1\n");
		soap_destroy(soap);
		DPRINTK("2\n");
		soap_end(soap);	
		DPRINTK("3\n");
		soap_done(soap);
		DPRINTK("4\n");
		free(soap);
		DPRINTK("5\n");

		usleep(4000);
	}	*/


	struct soap soap;
	struct ip_mreq mcast;
	
	soap_init2(&soap, SOAP_IO_UDP|SOAP_IO_FLUSH, SOAP_IO_UDP|SOAP_IO_FLUSH);
	
     //soap_set_namespaces(&soap, namespaces); 
	//soap_register_plugin(soap, soap_wsa);
	
	
	if(!soap_valid_socket(soap_bind(&soap, NULL, 3702, 10)))
	{ 
		soap_print_fault(&soap, stderr);
		exit(1);
	}
	
	mcast.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
	mcast.imr_interface.s_addr = htonl(INADDR_ANY);
	if(setsockopt(soap.master, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mcast, sizeof(mcast)) < 0)
	{
		printf("setsockopt error!\n");
		return 0;
	}	

	for(;;)
	{ 
		printf("Accepting requests...\n");
		if(soap_serve(&soap))
			soap_print_fault(&soap, stderr);
	
		soap_destroy(&soap);
		soap_end(&soap);

		usleep(4000);
	}
	
	soap_done(&soap);

	return 1;
}


void * OnvifUdpServerFunc(void * value)
{
	
	start_udp_server();
	pthread_detach(pthread_self());
}

void * OnvifRecvMotionFunc(void * value)
{
	
	while(1)
	{
		sleep(5);
		// 5 seconds , refress new data.
		if(dbg_test)
		{
		}
		else
			get_ipcam_all_info();
	}
	
	pthread_detach(pthread_self());
}

int OnvifCommandMode(char * arg1,char * arg2,char * arg3,char * arg4,char * arg5,char * arg6)
{
	char cmd_line[1024];
	int ret = 1;
	int i;

	sprintf(cmd_line,"/tmp/onvifdevice %s %s %s %s %s %s",
		arg1,arg2,arg3,arg4,arg5,arg6);

	i = FindSTR(cmd_line,"(",0);
	if( i > 0 )
	{
		cmd_line[i] = NULL;
	}

	DPRINTK("%s\n",cmd_line);
	
	ret = system(cmd_line);
	

	return ret;
}


#define  ALARM_TIME_0 (550)
#define  ALARM_TIME_1  (ALARM_TIME_0 - 50 )

void * OnvifMotionSendFunc(void * value)
{	

	pthread_detach(pthread_self());
}

int Save_GetOnvifInfo()
{
	char * save_buf = NULL;
	FILE * fp = NULL;
	int rel;

	save_buf = stIpcamInfo_save;

	fp = fopen("/root/onvif_data","wb");
	if( fp == NULL)
		return -1;		

	rel = fwrite(save_buf,1,sizeof(IPCAM_INFO)*MAX_CHANNEL,fp);

	if( rel != sizeof(IPCAM_INFO)*MAX_CHANNEL)
	{
		DPRINTK("fwrite error!\n");
		fclose(fp);
		fp = NULL;
		return -1;
	}


	fclose(fp);
	fp = NULL;

	system("sync");
	
	return 1;
}

int Load_GetOnvifInfo(char * buf)
{
	char * load_buf = NULL;
	FILE * fp = NULL;
	char file_name[50];
	int rel ;
	int length;

	strcpy(file_name,"/root/onvif_data");

	load_buf = buf;

	fp = fopen(file_name,"rb");
	if( !fp )
	{
		DPRINTK("open %s error!\n",file_name);
		return -1;
	}

	rel = fseek(fp, 0, SEEK_END);
	if ( rel < 0 )
	{					
		fclose(fp);		
		DPRINTK("fseek %s error!\n",file_name);
		return -1;
	}					

	length = ftell(fp);

	if( length != sizeof(IPCAM_INFO)*MAX_CHANNEL )
	{
		fclose(fp);		
		DPRINTK(" %s is size %d %d error!\n",file_name,length, sizeof(IPCAM_INFO)*MAX_CHANNEL);
		return -1;
	}

	rewind(fp);	

	rel = fread(load_buf,1,length,fp);
	if( rel != length )
	{		
		DPRINTK("fread %s error!\n",file_name);
		fclose(fp);
		return -1;
	}

	fclose(fp);
	fp = NULL;
	
	return 1;
}


void OnvifPtzResetRreset(IPCAM_INFO * pIpcamInfo)
{
	int chan;
	int i;
	
	chan = pIpcamInfo->cam_no;

	for( i = 0;i < MAX_PTZ_PRESET_NUM;i++)
	{
		ptz_preset_array[chan][i].Name[0] = 0;
		ptz_preset_array[chan][i].Token[0] = 0;
	}
}

int  OnvifIsPtzPresetGet(IPCAM_INFO * pIpcamInfo )
{
	int chan;
	int i;

	chan = pIpcamInfo->cam_no;

	for( i = 0; i < 10;i++)
	{
		
		if( ptz_preset_array[chan][i].Token[0] != 0 )
		{			
			return 1;
		}	

	}

	return 0;
}


int set_p2p_client_socket(int * client_socket,int client_port,int delaytime)
{
	struct  linger  linger_set;
//	int nNetTimeout;
	struct timeval stTimeoutVal;
//	struct sockaddr_in server_addr;
	int i;
	int keepalive = 1;
	int keepIdle = 30;
	int keepInterval = 3;
	int keepCount = 3;
	
	if( -1 == (*client_socket =  socket(PF_INET,SOCK_STREAM,0)))
	{
		DPRINTK( "DVR_NET_Initialize socket initialize error\n" );
		//return DVR_ERR_SOCKET_INIT;
		return ERR_NET_SET_ACCEPT_SOCKET;
	}

	//DPRINTK(" client_socket = %d\n",*client_socket);

	i = 1;

	if (setsockopt(*client_socket, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(int)) < 0) 
	{		
		close( *client_socket );
		*client_socket = 0;
		DPRINTK("setsockopt SO_REUSEADDR error \n");		
		return ERR_NET_SET_ACCEPT_SOCKET;	
	}	

/*
	if(setsockopt( *client_socket,SOL_TCP,TCP_KEEPIDLE,(void *)&keepIdle,sizeof(keepIdle)) < -1)
	{
		DPRINTK( "setsockopt TCP_KEEPIDLE error\n" );
	}

	
	if(setsockopt( *client_socket,SOL_TCP,TCP_KEEPINTVL,(void *)&keepInterval,sizeof(keepInterval)) < -1)
	{
		DPRINTK( "setsockopt TCP_KEEPINTVL error\n" );
	}

	
	if(setsockopt( *client_socket,SOL_TCP,TCP_KEEPCNT,(void *)&keepCount,sizeof(keepCount)) < -1)
	{
		DPRINTK( "setsockopt TCP_KEEPCNT error\n" );
	}*/

	linger_set.l_onoff = 1;
	linger_set.l_linger = 5;
    if (setsockopt( *client_socket, SOL_SOCKET, SO_LINGER , (char *)&linger_set, sizeof(struct  linger) ) < 0 )
    {
		close( *client_socket );
		*client_socket = 0;
		DPRINTK( "pNetSocket->sockSrvfd  SO_LINGER  error\n" );
		return ERR_NET_SET_ACCEPT_SOCKET;
	}


	  if( delaytime != 0 )
	  {
		stTimeoutVal.tv_sec = delaytime;
		stTimeoutVal.tv_usec = 0;
		if ( setsockopt( *client_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&stTimeoutVal, sizeof(struct timeval) ) < 0 )
	       {
	      		close( *client_socket );
			*client_socket = 0;
			DPRINTK( "setsockopt SO_SNDTIMEO error\n" );
			return ERR_NET_SET_ACCEPT_SOCKET;
		}
			 	
		if ( setsockopt( *client_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&stTimeoutVal, sizeof(struct timeval) ) < 0 )
	       {
	       	close( *client_socket );
			*client_socket = 0;
			DPRINTK( "setsockopt SO_RCVTIMEO error\n" );
			return ERR_NET_SET_ACCEPT_SOCKET;
		}
	  }

/*
	memset(&server_addr, 0, sizeof(struct sockaddr_in)); 
	server_addr.sin_family=AF_INET; 
	server_addr.sin_addr.s_addr=htonl(INADDR_ANY); 
	//inet_aton("192.168.1.122", &server_ctrl->server_addr.sin_addr);
	server_addr.sin_port=htons(client_port); 

	if ( -1 == (bind(*client_socket,(struct sockaddr *)(&server_addr),
		sizeof(struct sockaddr))))
	{
		close( *client_socket );
		*client_socket = 0;
		DPRINTK( "DVR_NET_Initialize socket bind error\n" );
		//return DVR_ERR_SOCKET_BIND;
		return ERR_NET_SET_ACCEPT_SOCKET;
	}	
*/

	return 1;
	   
}


static int socket_send_data( int idSock, char* pBuf, int ulLen )
{
	int lSendLen;
	int lStotalLen;
	int lRecLen;
	char* pData = pBuf;

	lSendLen = ulLen;
	lStotalLen = lSendLen;
	lRecLen = 0;

	if ( idSock <= 0 )
	{
		return -1;
	}	

	
	
	while( lRecLen < lStotalLen )
	{
		
		lSendLen = send( idSock, pData, lStotalLen - lRecLen, 0 );			
		if ( lSendLen <= 0 )
		{
			DPRINTK("lSendLen=%d\n",lSendLen);
			return -1;
		}
		lRecLen += lSendLen;
		pData += lSendLen;
	}

	

	return 1;
}


static int socket_read_data( int idSock, char* pBuf, int ulLen )
{
	int ulReadLen = 0;
	int iRet = 0;

	if ( idSock <= 0 )
	{
		return -1;
	}	

	while( ulReadLen < ulLen )
	{		
		iRet = recv( idSock, pBuf, ulLen - ulReadLen, 0 );
		if ( iRet <= 0 )
		{
			DPRINTK("ulReadLen=%d fd=%d\n",iRet,idSock);
			return -1;
		}
		ulReadLen += iRet;
		pBuf += iRet;
	}	
	
	return 1;
}


int   get_ip_addr(const   char   *name,   char   *ipaddr)    
{    

	  int   sockfd;    

	  struct   ifreq ifr;    

	  struct   sockaddr_in *sin;    

	  struct   hostent *addr;    

	     

	  if   (name   ==   NULL   ||   ipaddr   ==   NULL)    

	  {    
		  TRACE("there   has   illegal   parameters!\n");    

		  return   -1;    
	  }    

	  if   ((sockfd   =   socket(AF_INET,   SOCK_DGRAM,   0))   <   0)    

	  {    

		  TRACE("socket   error\n");  
		  return   -1;    

	  }    

	     

	  //get   the   IP   address   in   address   structure    

	  memset(&ifr,   0,   sizeof(ifr));    

	  strncpy(ifr.ifr_name,   name,   sizeof(ifr.ifr_name)-1);    

	  if   (ioctl(sockfd,   SIOCGIFADDR,   &ifr)   <   0)    

	  {    

		  TRACE("ioctl   SIOCGIFADDR   error\n");  
		  return   -1;    

	  }        

	  sin   =   (struct   sockaddr_in*)&ifr.ifr_addr;    

	  strcpy(ipaddr,   inet_ntoa(sin->sin_addr));         

	  return   1;    

  }    


int TestFunc()
{
	int socket_fd;
	int rel;
	struct sockaddr_in addr;
	char readbuf[2000] ="";
	char sendbuf[1000] = "POST /onvif/ptz_service HTTP/1.1 Host: 192.168.0.103 Content-Type: application/soap+xml; charset=utf-8 Content-Length: 513 <?xml version=\"1.0\" encoding=\"utf-8\"?><s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\"><s:Body xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\"><ContinuousMove  xmlns=\"http://www.onvif.org/ver20/ptz/wsdl\"><ProfileToken>MainStreamTooken</ProfileToken><Velocity><PanTilt x=\"0.5\" y=\"0\"  space=\"http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace\" xmlns=\"http://www.onvif.org/ver10/schema\"/></Velocity></ContinuousMove></s:Body></s:Envelope>";
	
		
	rel = set_p2p_client_socket(&socket_fd,SVR_CONNECT_PORT,30);
	if( rel < 0 )
		return rel;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("192.168.0.103");
       addr.sin_port = htons(8000);	

	   
	if (connect(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) 
	{
		perror(strerror(errno));
		
	        if (errno == EINPROGRESS) 
			{			
	             fprintf(stderr, "connect timeout\n");	
			}  
			
			DPRINTK("can't connect\n");		 
			
	        return -1;
        }

	DPRINTK("send data\n");

	rel = socket_send_data(socket_fd,sendbuf,strlen(sendbuf)+1);
	
	if( rel < 0 )
	{		
		perror(strerror(errno));
		DPRINTK("p2p_login send date error!\n");		
		return ERR_NET_DATE;
	}

	
	DPRINTK("recv data\n");

	rel = socket_read_data(socket_fd,readbuf,2000);
	if( rel < 0 )
	{		
		DPRINTK("p2p_login send date error!\n");
		//close_dvr_svr_socket();
		DPRINTK("%s",readbuf);
		return ERR_NET_DATE;
	}

	close(socket_fd);

	
}

int insert_cmd_buf(BUF_FRAMEHEADER * header,char * buf,CYCLE_BUFFER * cycle_enc_buf)
{
	int ret;
	int length;
	int count = 0;

	if( header->iLength >= (int)(cycle_enc_buf->size) )
	{
		return -1;
	}

	if( fifo_enable_insert(cycle_enc_buf) == 0 )
		return -1;

	fifo_lock_buf(cycle_enc_buf);		
	
	ret = fifo_put(cycle_enc_buf,header,sizeof(BUF_FRAMEHEADER));
	if( ret != sizeof(BUF_FRAMEHEADER) )
	{
		DPRINTK("%d insert error!\n",ret);		
		goto err;
	}

	length = header->iLength;

	ret = fifo_put(cycle_enc_buf,buf,length);
	if( ret != length )
	{
		DPRINTK("%d %d insert error!\n",ret,length);		
		goto err;
	}

	
	fifo_unlock_buf(cycle_enc_buf);	

	return 1;

err:
	fifo_reset(cycle_enc_buf);
	fifo_unlock_buf(cycle_enc_buf);

	return -1;
}

int get_cmd_buf(BUF_FRAMEHEADER * header,char * buf,CYCLE_BUFFER *cycle_enc_buf)
{
	int ret;
	int length;

	if( fifo_enable_get(cycle_enc_buf) <= 0 )
		return 0;
	

	fifo_read_lock_buf(cycle_enc_buf);
	
	ret = fifo_get(cycle_enc_buf,header,sizeof(BUF_FRAMEHEADER));
	if( ret != sizeof(BUF_FRAMEHEADER) )
	{
		DPRINTK("%d get error!\n",ret);		
		goto err2;
	}

	length = header->iLength;	

	//DPRINTK(" ret=%d  length=%d\n",ret,length);

	ret = fifo_get(cycle_enc_buf,buf,length);
	if( ret != length )
	{
		DPRINTK("%d %d get error! %d\n",ret,length,sizeof(BUF_FRAMEHEADER));		
		goto err2;
	}
	fifo_read_unlock_buf(cycle_enc_buf);

	return 1;

err2:
	fifo_read_unlock_buf(cycle_enc_buf);
	
	fifo_lock_buf(cycle_enc_buf);
	fifo_reset(cycle_enc_buf);
	fifo_unlock_buf(cycle_enc_buf);

	return -1;
}

void cmd_thread(void * value)
{	
	int save_buf_num = 0;
	int i;
	int write_count = 0;
	int rel;
	int chan;
	BUF_FRAMEHEADER header;
	NET_COMMAND * pcmd = NULL;
	IPCAM_INFO * tmpinfo;
	char buf[102400];
	CYCLE_BUFFER * cyc_buf = NULL;
	cyc_buf = (CYCLE_BUFFER *)value;
	
	DPRINTK(" pid = %d  %x\n",getpid(),cyc_buf);

	while(1)
	{	
		
		rel = get_cmd_buf(&header,buf,cyc_buf);
		if( rel > 0 )
		{			
			if( cyc_buf->cam_no == 1)
			DPRINTK("buf_num=%d\n",fifo_num(cyc_buf));
		
			pcmd = (NET_COMMAND *)buf;

			switch(pcmd->command)
			{
				case NET_CAM_ENC_SET:
					//DPRINTK("NET_CAM_ENC_SET %d\n",pcmd->length/100);
					
					
								
					break;
				case SET_IPCAM_TIME:
						//DPRINTK("SET_IPCAM_TIME  [%d]\n",pcmd->play_fileId);
						
					 break;
				case PTZ_MOVE:
					
					chan = pcmd->length;
					
					break;
				case PTZ_STOP:
					DPRINTK("PTZ_STOP  [%d] %d %d [%d]\n",pcmd->length,pcmd->page,pcmd->play_fileId,cyc_buf->cam_no);
					chan = pcmd->length;
					
					break;	
				case PTZ_SET_PRESET:
					DPRINTK("PTZ_SET_PRESET  [%d] %d %d\n",pcmd->length,pcmd->page,pcmd->play_fileId);
					chan = pcmd->length;
					
					break;
				case PTZ_REMOVE_PRESET:
					DPRINTK("PTZ_REMOVE_PRESET  [%d] %d %d\n",pcmd->length,pcmd->page,pcmd->play_fileId);
					chan = pcmd->length;
					
					break;
				case PTZ_GOTO_PRESET:
					DPRINTK("PTZ_GOTO_PRESET  [%d] %d %d\n",pcmd->length,pcmd->page,pcmd->play_fileId);
					chan = pcmd->length;
					
					break;
				case SET_IPCAM_IMAGE_PARAMETER:
					chan = pcmd->length;
					
					break;
				default:break;
			}
		
		}else if(rel ==0 )
		{
			usleep(10000);
		}else
		{
		}

		usleep(10);		
	}

	pthread_detach(pthread_self());
}


typedef struct st_socket_onvif{
	int socket_fd;
	char ip[100];
}ST_SOCKET_ONVIF;





static int onvif_client_socket_set(int * client_socket,int client_port,int delaytime,int create_socket,int connect_mode)
{
	struct  linger  linger_set;
	struct timeval stTimeoutVal;

	int i;
	int keepalive = 1;
	int keepIdle = 10;
	int keepInterval = 3;
	int keepCount = 3;



/*
	linger_set.l_onoff = 0;
	linger_set.l_linger = 0;

    if (setsockopt( *client_socket, SOL_SOCKET, SO_LINGER , (char *)&linger_set, sizeof(struct  linger) ) < 0 )
    {
		printf( "setsockopt SO_LINGER  error\n" );
		return ERR_NET_SET_ACCEPT_SOCKET;	
	}
	

	
    if (setsockopt(*client_socket, SOL_SOCKET, SO_KEEPALIVE, (char *)&keepalive, sizeof(int) ) < 0 )
    {
		printf( "setsockopt SO_KEEPALIVE error\n" );
		return ERR_NET_SET_ACCEPT_SOCKET;	
	}

	
	if(setsockopt( *client_socket,SOL_TCP,TCP_KEEPIDLE,(void *)&keepIdle,sizeof(keepIdle)) < -1)
	{
		printf( "setsockopt TCP_KEEPIDLE error\n" );
		return ERR_NET_SET_ACCEPT_SOCKET;	
	}

	
	if(setsockopt( *client_socket,SOL_TCP,TCP_KEEPINTVL,(void *)&keepInterval,sizeof(keepInterval)) < -1)
	{
		printf( "setsockopt TCP_KEEPINTVL error\n" );
		return ERR_NET_SET_ACCEPT_SOCKET;	
	}

	
	if(setsockopt( *client_socket,SOL_TCP,TCP_KEEPCNT,(void *)&keepCount,sizeof(keepCount)) < -1)
	{
		printf( "setsockopt TCP_KEEPCNT error\n" );
		return ERR_NET_SET_ACCEPT_SOCKET;	
	}
*/


	i = 1;

	if (setsockopt(*client_socket, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(int)) < 0) 
	{		
		close( *client_socket );
		*client_socket = 0;
		DPRINTK("setsockopt SO_REUSEADDR error \n");		
		return ERR_NET_SET_ACCEPT_SOCKET;	
	}		


	  if( delaytime != 0 )
	  {
		stTimeoutVal.tv_sec = delaytime;
		stTimeoutVal.tv_usec = 0;
		if ( setsockopt( *client_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&stTimeoutVal, sizeof(struct timeval) ) < 0 )
	       {
	      		close( *client_socket );
			*client_socket = 0;
			printf( "setsockopt SO_SNDTIMEO error\n" );
			return ERR_NET_SET_ACCEPT_SOCKET;
		}
			 	
		if ( setsockopt( *client_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&stTimeoutVal, sizeof(struct timeval) ) < 0 )
	       {
	       	close( *client_socket );
			*client_socket = 0;
			printf( "setsockopt SO_RCVTIMEO error\n" );
			return ERR_NET_SET_ACCEPT_SOCKET;
		}
	  }



	return 1;
	   
}


int tcp_srv_listen(int port)
{
	int listen_fd = 0;
	struct sockaddr_in adTemp;
	struct sockaddr_un uadTemp;
	struct  linger  linger_set;
	int i,sin_size;
	struct  timeval timeout;
	int ret;
	int tmp_fd = 0;	
	struct sockaddr_in server_addr;
	fd_set watch_set;
	int result;
	int chan =0;
	
	
	if( -1 == (listen_fd =  socket(PF_INET,SOCK_STREAM,0)))
	{
		DPRINTK( "Socket initialize error\n" );
			
		goto error_end;
	}

	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(int)) < 0) 
	{		
		
		DPRINTK("setsockopt SO_REUSEADDR error \n");		
		goto error_end;	
	}	

	linger_set.l_onoff = 1;
	linger_set.l_linger = 0;
    if (setsockopt( listen_fd, SOL_SOCKET, SO_LINGER , (char *)&linger_set, sizeof(struct  linger) ) < 0 )
    {
		
		DPRINTK( "pNetSocket->sockSrvfd  SO_LINGER  error\n" );
		goto error_end;
	}

	memset(&server_addr, 0, sizeof(struct sockaddr_in)); 
	server_addr.sin_family=AF_INET; 
	server_addr.sin_addr.s_addr=htonl(INADDR_ANY);	
	server_addr.sin_port=htons(port); 

	if ( -1 == (bind(listen_fd,(struct sockaddr *)(&server_addr),
			sizeof(struct sockaddr))))
	{		
		DPRINTK( "DVR_NET_Initialize socket bind error\n" );
		goto error_end;
	}

	if ( -1 == (listen(listen_fd,30)))
	{
	
		DPRINTK( "DVR_NET_Initialize socket listen error\n" );
		goto error_end;
	}

	sin_size = sizeof(struct sockaddr_in);

	DPRINTK("listen start\n");

	while(1)
	{
		FD_ZERO(&watch_set); 
 		FD_SET(listen_fd,&watch_set);
		timeout.tv_sec = 1;
		timeout.tv_usec = 1000;
		
		ret  = select( listen_fd + 1, &watch_set, NULL, NULL, &timeout );
		if ( ret < 0 )
		{
			DPRINTK( "listen socket select error! %d\n" );
			goto error_end;
		}

		if ( ret > 0 && FD_ISSET( listen_fd, &watch_set ) )
		{			
			tmp_fd = accept( listen_fd,(struct sockaddr *)&adTemp, &sin_size);

			if( tmp_fd > 0 )
			{
				ST_SOCKET_ONVIF st_socket;
				//DPRINTK("ip:%s come\n",inet_ntoa(adTemp.sin_addr));

				onvif_client_socket_set(&tmp_fd,0,5,0,NET_MODE_NORMAL);

				st_socket.socket_fd = tmp_fd;
				strcpy(st_socket.ip , inet_ntoa(adTemp.sin_addr));
				create_net_process(&st_socket);
			}

			
		}
	}

error_end:

	return -1;
}



static int net_process_socket_read_data( int idSock, char* pBuf, int ulLen )
{
	int ulReadLen = 0;
	int iRet = 0;

	if ( idSock <= 0 )
	{
		return -1;
	}	
		
	iRet = recv( idSock, pBuf, ulLen - ulReadLen, 0 );
	if ( iRet <= 0 )
	{
		//DPRINTK("ulReadLen=%d\n",iRet);
		//DPRINTK("%s\n", strerror(errno));		
            
		return -1;
	}	
	
	return iRet;
}

static int net_process_socket_send_data( int idSock, char* pBuf, int ulLen )
{
	int ulReadLen = 0;
	int iRet = 0;

	if ( idSock <= 0 )
	{
		return -1;
	}
		
	iRet = send( idSock, pBuf, ulLen - ulReadLen, 0 );
	if ( iRet < 0 )
	{
		DPRINTK("send=%d\n",iRet);
		DPRINTK("%s\n", strerror(errno));	
            
		return -1;
	}	
	
	return iRet;
}

int get_chan_by_ip(char * ip)
{
	int i;
	for( i = 0; i < MAX_CHANNEL; i++)
	{
		if( stIpcamInfo_save[i].support_motion_detect == 1 &&  stIpcamInfo_save[i].getinfo == 1)
		{
			if( strcmp(ip,stIpcamInfo_save[i].ip) == 0 )
				return i;
		}
	}

	return -1;
}


#define MAX_RECV_NUM (100000)

void * Net_Process(void * value)
{
	int socket_fd = 0;
	fd_set watch_set;
	struct  timeval timeout;
	int ret;
	ST_SOCKET_ONVIF st_socket;	
	ST_SOCKET_ONVIF * p_socket;
	char send_ok[5000]= "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nServer: Microsoft-HTTPAPI/1.0\r\nDate: Mon, 12 May 2013 08:29:20 GMT\r\nConnection: close\r\n\r\n";
	char recv_buf[MAX_RECV_NUM];
	char html_buf[MAX_RECV_NUM];
	int write_pos = 0;
	
	memset(html_buf,0x00,MAX_RECV_NUM);
	
	st_socket = *(ST_SOCKET_ONVIF*)value;

//	DPRINTK("in fd=%d ip:%s\n",st_socket.socket_fd,st_socket.ip);

	socket_fd = st_socket.socket_fd;

	p_socket = (ST_SOCKET_ONVIF*)value;
	p_socket->socket_fd = 0;


	while( 1)
	{
		FD_ZERO(&watch_set); 
 		FD_SET(socket_fd,&watch_set);
		timeout.tv_sec = 5;
		timeout.tv_usec = 1000;		

		ret  = select( socket_fd + 1, &watch_set, NULL, NULL, &timeout );
		if ( ret < 0 )
		{
			DPRINTK( "recv socket select error!\n" );
			goto error_end;
		}

		if ( ret > 0 && FD_ISSET( socket_fd, &watch_set ) )
		{
			memset(recv_buf,0x00,MAX_RECV_NUM);			
			ret =net_process_socket_read_data(socket_fd,recv_buf,MAX_RECV_NUM);			
			if( ret < 0 )
			{					
				//DPRINTK("Net_Process recv error!\n");
				goto error_end;
			}else
			{
				if( ret == 0 )
				{
					//DPRINTK("fd=%d: read 0\n",socket_fd);
				}
				
				{						
					int i = 0,j = 0;		
					int chan=0;

					if( write_pos + ret >= MAX_RECV_NUM )
					{
						DPRINTK("read data = %d > MAX_RECV_NUM\n",write_pos + ret);						
						goto error_end;
					}else
					{
						memcpy(html_buf +write_pos ,recv_buf,ret);
						write_pos += ret;
					}

					i = FindSTR(html_buf,
						"PropertyOperation=\"Changed\"",0);

					if( i < 0 )
					{
						i = FindSTR(html_buf,
							"PropertyOperation=\"Initialized\"",0);
					}

					if( i >= 0 )
					{						
						i = FindSTR(html_buf,"Name=\"MotionActive\"",0);

						if( i > 0 )
						{
							j = FindSTR(html_buf,"Value=\"1\"",i);

							if( j > 0 )
							{
								DPRINTK("%s[%d]: motion start\n",st_socket.ip,socket_fd);
								 chan= get_chan_by_ip(st_socket.ip);
								if( chan >= 0 )
								{
									onvif_motion_flag[chan] = 1;
									SendMotionChan( chan,1);
								}
							}

							j = FindSTR(html_buf,"Value=\"0\"",i);

							if( j > 0 )
							{
								DPRINTK("%s[%d]: motion stop\n",st_socket.ip,socket_fd);
								 chan= get_chan_by_ip(st_socket.ip);							
								if( chan >= 0 )
								{
									onvif_motion_flag[chan]  =  0;
									SendMotionChan(chan,0); 
								}
							}

							
						}
					}

					i = FindSTR(html_buf,
						"</SOAP-ENV:Envelope>",0);

					if( i > 0 )
					{
						//printf("\n\n%s\n\n",html_buf);
						net_process_socket_send_data(socket_fd,send_ok,strlen(send_ok));
						goto error_end;
					}					
					
				}
			}
		}	

		if( ret == 0 )
		{
			//DPRINTK("fd=%d time out\n",socket_fd);
			goto error_end;
		}
	
		
	}


error_end:
	
	close(socket_fd);
//	DPRINTK("out fd=%d ip:%s\n",st_socket.socket_fd,st_socket.ip);
	pthread_detach(pthread_self());
	
	return ;
}

int create_net_process(ST_SOCKET_ONVIF * st_socket_fd)
{
	int ret;
	pthread_t net_process_th;	
	ret = pthread_create(&net_process_th,NULL,Net_Process,(void*)st_socket_fd);
	if ( 0 != ret )
	{		
		DPRINTK("create Net_Process th error\n");
		perror(strerror(errno));
		return -1;
	}

	while(st_socket_fd->socket_fd != 0)
	{
		DPRINTK("wait thread create\n");
		usleep(100);
	}
	

	return 1;
}

int command_mode(int argc, char * argv[])
{



	
	return 1;
}

void usage()
{
	printf(" usage:\n");
	printf(" app_name ip port cam_chan \n");
	printf(" onvif_server 127.0.0.1 16100\n");
	return ;
}

int  get_ipcam_all_info()
{
	NET_COMMAND g_cmd;
	int count = 0;

	memset(&g_cmd,0,sizeof(NET_COMMAND));
	
	g_cmd.command = IPCAM_CMD_GET_ALL_INFO;

	get_ipcam_info_once = 0;

	Net_dvr_send_cam_data(&g_cmd,sizeof(g_cmd),pCommuniteToOnvif);

	while(get_ipcam_info_once == 0 )
	{
		usleep(100000);
		count++;

		if( count > 20 )
		{
			DPRINTK("not get ipcam all info! exit\n");
			exit(0);
		}
	}
}

int nvr_drv_get_file_data(char* file_name,char * buf,int max_len)
{
	FILE *fp=NULL;
	long fileOffset = 0;
	int rel;


	fp = fopen(file_name,"rb");
	if( fp == NULL )
	{
		DPRINTK(" open %s error!\n",file_name);
		goto get_err;
	}

	rel = fseek(fp,0L,SEEK_END);
	if( rel != 0 )
	{
		DPRINTK("fseek error!!\n");
		goto get_err;
	}

	fileOffset = ftell(fp);
	if( fileOffset == -1 )
	{
		DPRINTK(" ftell error!\n");
		goto get_err;
	}

	DPRINTK(" fileOffset = %ld\n",fileOffset);

	rewind(fp);	

	/* if minihttp alive than kill it */
	if( fileOffset > 0 )
	{	
		rel = fread(buf,1,max_len,fp);
		if( rel <= 0 )
		{
			DPRINTK(" fread Error!\n");
			goto get_err;
		}	
		
	}

	fclose(fp);

	return rel;

get_err:
	if( fp )
	   fclose(fp);

	return -1;
}

int check_have_sd()
{
	char buf_tmp1[1000];
	int read_num = 0;
	int ret = 1;
	
	system("cat /proc/partitions| grep mmcblk0 |grep -v grep > /ramdisk/disk_info");
	read_num = nvr_drv_get_file_data("/ramdisk/disk_info",buf_tmp1,1000);
	if( read_num <= 0 )	
		ret = 0;
	
	system("rm -rf /ramdisk/disk_info");

	return ret;
}



int main(int argc, char * argv[])
{
	pthread_t onvif_th;	
	pthread_t onvif_motion_th;	
	pthread_t onvif_motion_th2;		
	NET_COMMAND g_cmd;
	LISTEN_ST listen_st;
	int i;
	pthread_t id_encode;
	char ip[50] = {0};
	int port = 16100;
	int onvif_server_port = 18222;
	int cam = 0x01;
	int chan = 0;
	int ret = 0;
	char rtsp_url[50];
	int count = 0;

	if( argc != 3 )
	{
		usage();
		return -1;
	}

      strcpy(ip,argv[1]);
      port = atoi(argv[2]);

		

	if(dbg_test)
	{
		IPCAM_ALL_INFO_ST * pConfig = get_ipcam_config_st();
		memcpy(&g_pIpcamAll,pConfig,sizeof(IPCAM_ALL_INFO_ST));	
		//onvif_server_port = g_pIpcamAll.onvif_server_port;
		onvif_server_port = g_pIpcamAll.onvif_server_port = port;
	}else
	{
		pCommuniteToOnvif = NM_ini_st(BUILD_MODE_ONCE,NET_SEND_DIRECT_MODE,0x100000/8,net_recv_enc_data);
		if( pCommuniteToOnvif  == NULL )
		{					
			DPRINTK("create pCommuniteToOnvif error!\n");
			exit(0);
		}

		ret = NM_connect(pCommuniteToOnvif,ip,port);
		if( ret < 0 )
		{
			DPRINTK("NM_connect %s:%d error!\n",ip,port);
			return -1;
		}

		get_ipcam_all_info();

		
		onvif_server_port = g_pIpcamAll.onvif_server_port;
	}

	

	ret = pthread_create(&onvif_th,NULL,OnvifUdpServerFunc,(void*)NULL);
	if ( 0 != ret )
	{		
		DPRINTK("create rtsp_connect th error\n");
		perror(strerror(errno));
		exit(0);
	}


	ret = pthread_create(&onvif_motion_th,NULL,OnvifRecvMotionFunc,(void*)NULL);
	if ( 0 != ret )
	{		
		DPRINTK("create OnvifRecvMotionFunc th error\n");
		perror(strerror(errno));
		exit(0);
	} 
	
	
	ret = pthread_create(&onvif_motion_th2,NULL,OnvifMotionSendFunc,(void*)NULL);
	if ( 0 != ret )
	{		
		DPRINTK("create OnvifMotionSendFunc th error\n");
		perror(strerror(errno));
		exit(0);
	}

	start_printf_send_th(28001);

	start_my_server(onvif_server_port);


	DPRINTK("out \n");
}




int main_search(int argc, char * argv[])
{
	pthread_t onvif_th;	
	pthread_t onvif_motion_th;	
	pthread_t onvif_motion_th2;		
	NET_COMMAND g_cmd;
	LISTEN_ST listen_st;
	int i;
	pthread_t id_encode;
	char ip[50] = {0};
	int port = 16100;
	int onvif_server_port = 18222;
	int cam = 0x01;
	int chan = 0;
	int ret = 0;
	char rtsp_url[50];
	int count = 0;


	

	ret = pthread_create(&onvif_th,NULL,OnvifUdpServerFunc,(void*)NULL);
	if ( 0 != ret )
	{		
		DPRINTK("create rtsp_connect th error\n");
		perror(strerror(errno));
		exit(0);
	}


	start_printf_send_th(28001);

	start_my_server(onvif_server_port);


	DPRINTK("out \n");
}


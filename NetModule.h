#ifndef _NET_MODULE_HEADER
#define _NET_MODULE_HEADER

#ifdef __cplusplus
extern "C" {
#endif


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

#include "BaseData.h"
#include "buf_array.h"
#include "onvif_base.h"
#include "net_ping.h"



#define NET_PROCESS_SOCKET (21325)
#define NET_PROCESS_TO_RTSP_SOCKET (22325)
#define NET_PROCESS_TO_ONVIF_SOCKET (22326)



#define BUILD_MODE_ALLWAY  0
#define BUILD_MODE_ONCE  1

#define NET_MODE_NORMAL (0)
#define NET_MODE_LOCAL    (1)

#define NET_SEND_BUF_MODE       (0)
#define NET_SEND_DIRECT_MODE (1)

#define MN_MAX_BUF_SIZE (256*1024)

#define MN_MAX_BIND_ENC_NUM (3)

#define MAX_CHANNEL (32)
//#define MAX_FRAME_BUF_SIZE (512*1024)
#define MAX_FRAME_BUF_SIZE  ONVIF_NET_MAX_BUF


typedef struct _NET_DATA_ST{
int cmd;
int data_length;
}NET_DATA_ST;

typedef struct _LISTEN_ST_{
	int listen_port;
	int connect_mode;
	int is_create;
}LISTEN_ST;


typedef struct _NET_MODULE_ST_ {
int listen_socket;
int client;
int work_mode;
CYCLE_BUFFER * cycle_send_buf;
int (*net_get_data)(void * pst,char * buf,int length,int cmd);
int send_th_flag;
int recv_th_flag;
int is_alread_work;
int build_once;
int bind_enc_num[MN_MAX_BIND_ENC_NUM];
int send_enc_data;
int video_mode;
int video_image_size;
int frame_num[MN_MAX_BIND_ENC_NUM];
int lost_frame[MN_MAX_BIND_ENC_NUM];
int dvr_cam_no;
int send_cam;
int net_send_count;
int live_cam;
int rec_cam;
int net_cam;
int recv_data_num;
char mac[30];
pthread_t netsend_id;
pthread_t netrecv_id;
pthread_t netcheck_id;
int stop_work_by_user;
int enc_info[8];
int cam_chan_recv_num[6];
int have_d1_enginer;
int ipcam_type;
int thread_stop_num;
int decoding;
int connect_mode; // NET_MODE_NORMAL |  NET_MODE_LOCAL
int send_data_mode; // NET_SEND_BUF_MODE | NET_SEND_DIRECT_MODE
int chan_num;
int connect_port;   // 12200  or  16000
int have_again_flag;
}NET_MODULE_ST;





int NM_command_recv_data(NET_MODULE_ST * pst);
int NM_net_send_data(NET_MODULE_ST * pst,char * buf,int length,int cmd);
int NM_get_buf(NET_MODULE_ST * pst,char * buf);
int NM_socket_read_data( int idSock, char* pBuf, int ulLen );
int NM_socket_send_data( int idSock, char* pBuf, int ulLen );
NET_MODULE_ST * NM_ini_st(int work_mode,int send_mode,int buf_size,int (*func)(void *,char *,int,int));
int  NM_reset(NET_MODULE_ST * pst);
int NM_destroy_st(NET_MODULE_ST * pst);
int NM_net_get_status(void * value);
int NM_connect(NET_MODULE_ST * pst,char * ip,int port);
int NM_disconnect(NET_MODULE_ST * pst);
int NM_connect_by_accept(NET_MODULE_ST * pst,int socket_fd);
int Net_dvr_send_cam_data(char * buf,int length,NET_MODULE_ST * server_pst);
int NM_destroy_work_thread(NET_MODULE_ST * pst);
int dvr_net_create_cam_listen(LISTEN_ST listen_st);


#ifdef __cplusplus
}
#endif



#endif



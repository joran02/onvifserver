
#include "NetModule.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

//#include "P2pBaseDate.h"
//#include "devfile.h"
//#include "bufmanage.h"
//#include "func_common.h"

extern NET_MODULE_ST * pNetModule[MAX_CHANNEL];
extern NET_MODULE_ST * pCommuniteToOnvif;


static int connect_num = 0;

int NM_command_recv_data(NET_MODULE_ST * pst)
{
	NET_DATA_ST st_data;
	int ret;
	char buf[MAX_FRAME_BUF_SIZE];
	char dest_buf[MAX_FRAME_BUF_SIZE];
	int convert_length = 0;
	BUF_FRAMEHEADER * pheader = NULL;
	
	ret = NM_socket_read_data(pst->client,&st_data,sizeof(NET_DATA_ST));	

	if( ret < 0 )
		return -1;	

	if( st_data.data_length > MAX_FRAME_BUF_SIZE || st_data.data_length <= 0)
	{
		DPRINTK(" length=%d error\n",st_data.data_length);
		return -1;
	}

	ret = NM_socket_read_data(pst->client,buf,st_data.data_length);
	if( ret < 0 )
		return -1;	

	
	convert_length =0;

	pst->recv_data_num++;
	if( pst->recv_data_num > 1000)
		pst->recv_data_num = 0;

	pheader = (BUF_FRAMEHEADER*)dest_buf;

	if( pheader->index >= 0 && pheader->index < 6 )
	{
		//DPRINTK("index=%d\n",pheader->index);
		pst->cam_chan_recv_num[pheader->index]++;
		if( pst->cam_chan_recv_num[pheader->index] > 1000)
			pst->cam_chan_recv_num[pheader->index] = 0;
	}

	if( convert_length > 0 )
		pst->net_get_data((void*)pst,dest_buf,convert_length,st_data.cmd);
	else
		pst->net_get_data((void*)pst,buf,st_data.data_length,st_data.cmd);

	return 1;	
}



int NM_net_send_data_direct(NET_MODULE_ST * pst,char * buf,int length,int cmd)
{
	int ret;
	NET_DATA_ST st_data;
	char dest_buf[MAX_FRAME_BUF_SIZE];
	char src_buf[MAX_FRAME_BUF_SIZE];
	int data_length;
	int convert_length;
	
	if( length + sizeof(NET_DATA_ST) >= MAX_FRAME_BUF_SIZE )
	{
		return -1;
	}

	if(  pst->recv_th_flag != 1 )
		return -1;

	st_data.cmd = cmd;
	st_data.data_length = length;

	fifo_lock_buf(pst->cycle_send_buf);	

	memcpy(src_buf,&st_data,sizeof(NET_DATA_ST));
	memcpy(src_buf + sizeof(NET_DATA_ST),buf,length);
	data_length = sizeof(NET_DATA_ST) + length;


	convert_length = 0;

	if( convert_length >  0 )
	{
		ret = NM_pst_socket_send_data( pst,pst->client, dest_buf, convert_length);
		if( ret < 0 )
		{		
			DPRINTK("NM_socket_send_data error no=%d\n",pst->dvr_cam_no);
			goto err;
		}
	}else
	{
		if( data_length > 0 )
		{
			ret = NM_pst_socket_send_data( pst,pst->client, src_buf, data_length);
			if( ret < 0 )
			{	
				DPRINTK("NM_socket_send_data error no=%d\n",pst->dvr_cam_no);
				goto err;
			}
		}	
	}	
	
	fifo_unlock_buf(pst->cycle_send_buf);	


	return 1;

err:
	fifo_reset(pst->cycle_send_buf);
	fifo_unlock_buf(pst->cycle_send_buf);

	return -1;
}



int NM_net_send_data(NET_MODULE_ST * pst,char * buf,int length,int cmd)
{
	int ret;
	NET_DATA_ST st_data;

	if( pst->client <= 0 )
	{
		return -1;
	}

	if( pst->send_data_mode == NET_SEND_DIRECT_MODE)
		return NM_net_send_data_direct(pst,buf,length,cmd);

	if( pst->send_th_flag != 1 || pst->recv_th_flag != 1 )
		return -1;
	
	if( length + sizeof(NET_DATA_ST) >= MAX_FRAME_BUF_SIZE )
	{
		return -1;
	}

	st_data.cmd = cmd;
	st_data.data_length = length;


	fifo_lock_buf(pst->cycle_send_buf);		
	
	ret = fifo_put(pst->cycle_send_buf,&st_data,sizeof(NET_DATA_ST));
	if( ret != sizeof(NET_DATA_ST) )
	{
		DPRINTK("%d insert error!\n",ret);		
		goto err;
	}	

	ret = fifo_put(pst->cycle_send_buf,buf,length);
	if( ret != length )
	{
		DPRINTK("%d %d insert error!\n",ret,length);		
		goto err;
	}
	
	fifo_unlock_buf(pst->cycle_send_buf);

	return 1;

err:
	fifo_reset(pst->cycle_send_buf);
	fifo_unlock_buf(pst->cycle_send_buf);

	return -1;
}

int NM_get_buf(NET_MODULE_ST * pst,char * buf)
{
	int ret;
	int length;
	NET_DATA_ST st_data;
	CYCLE_BUFFER * cycle_enc_buf_ptr = NULL;

	cycle_enc_buf_ptr = pst->cycle_send_buf;		

	fifo_lock_buf(pst->cycle_send_buf);	
	
	ret = fifo_get(cycle_enc_buf_ptr,&st_data,sizeof(NET_DATA_ST));
	if( ret != sizeof(NET_DATA_ST) )
	{
		DPRINTK("%d get error!\n",ret);		
		goto err2;
	}	

	length = st_data.data_length;

	memcpy(buf,&st_data,sizeof(NET_DATA_ST));
	
	ret = fifo_get(cycle_enc_buf_ptr,buf + sizeof(NET_DATA_ST),length);
	if( ret != length )
	{
		DPRINTK("%d %d get error! %d\n",ret,length,sizeof(NET_DATA_ST));		
		goto err2;
	}
	fifo_unlock_buf(pst->cycle_send_buf);


	length = sizeof(NET_DATA_ST) + st_data.data_length;

	return length;

err2:
	fifo_reset(cycle_enc_buf_ptr);
	fifo_unlock_buf(pst->cycle_send_buf);

	return -1;
}


int NM_socket_read_data( int idSock, char* pBuf, int ulLen )
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
			DPRINTK("ulReadLen=%d\n",iRet);
			DPRINTK("%s\n", strerror(errno));
			if( errno == EAGAIN )
                           continue;
                 
			return -1;
		}
		ulReadLen += iRet;
		pBuf += iRet;
	}

	
	
	return 1;
}

int NM_socket_send_data( int idSock, char* pBuf, int ulLen )
{
	int lSendLen;
	int lStotalLen;
	int lRecLen;
	int have_again_flag = 0;
	char* pData = pBuf;

	lSendLen = ulLen;
	lStotalLen = lSendLen;
	lRecLen = 0;

	if ( idSock <= 0 )
	{
		DPRINTK("idSock=%d\n",idSock);
		return -1;
	}	

	
	
	while( lRecLen < lStotalLen )
	{
		
		lSendLen = send( idSock, pData, lStotalLen - lRecLen, 0 );			
		if ( lSendLen <= 0 )
		{
			DPRINTK("lSendLen=%d %d\n",lSendLen,idSock);
			DPRINTK("%s   code=%d  EAGAIN=%d have_again_flag=%d\n", strerror(errno),errno,EAGAIN,have_again_flag);

			if( errno == EAGAIN )            
			{
				have_again_flag = 1;
				//DPRINTK("return 99\n");
				//return 99;
                          //continue;
			}
                                    
			
			return -1;
		}
		lRecLen += lSendLen;
		pData += lSendLen;
	}


	if( have_again_flag == 1 )
	{
		DPRINTK("return 99\n");
		return 99;	
	}

	return 1;
}



int NM_pst_socket_send_data(NET_MODULE_ST * pst, int idSock, char* pBuf, int ulLen )
{
	int lSendLen;
	int lStotalLen;
	int lRecLen;
	int have_again_flag = 0;
	char* pData = pBuf;

	lSendLen = ulLen;
	lStotalLen = lSendLen;
	lRecLen = 0;

	if ( idSock <= 0 )
	{
		DPRINTK("idSock=%d\n",idSock);
		return -1;
	}	

	
	
	while( lRecLen < lStotalLen )
	{
		if( idSock != pst->client)
			idSock =  pst->client;

		if ( idSock <= 0 )
		{
			DPRINTK("in send while idSock=%d\n",idSock);
			return -1;
		}
			
		lSendLen = send( idSock, pData, lStotalLen - lRecLen, 0 );			
		if ( lSendLen <= 0 )
		{
			DPRINTK("lSendLen=%d  want_send_num=%d %d  chan=%d\n",lSendLen,lStotalLen - lRecLen,idSock,pst->dvr_cam_no);
			DPRINTK("%s   code=%d  EAGAIN=%d have_again_flag=%d\n", strerror(errno),errno,EAGAIN,have_again_flag);

			if( errno == EAGAIN )            
			{
				have_again_flag = 1;
				pst->have_again_flag = 1;
				pst->stop_work_by_user = 1;				
				DPRINTK("stop_work_by_user set 1\n");
				sleep(3);
				continue;
			}                                    
			
			return -1;
		}
		lRecLen += lSendLen;
		pData += lSendLen;
	}
	

	return 1;
}


NET_MODULE_ST * NM_ini_st(int work_mode,int send_mode,int buf_size,int (*func)(void *,char *,int,int))
{
	NET_MODULE_ST * pst = NULL;

	pst = (NET_MODULE_ST *)malloc(sizeof(NET_MODULE_ST));

	if( pst == NULL )
	{
		printf("malloc error!\n");
		goto ini_error;
	}

	memset(pst,0x00,sizeof(NET_MODULE_ST));

	pst->cycle_send_buf = NULL;	
	pst->is_alread_work = 0;
	pst->build_once = work_mode;
	pst->work_mode = 0;
	pst->recv_data_num = 0;
	pst->send_data_mode = send_mode;
	pst->bind_enc_num[0] = -1;
	pst->bind_enc_num[1] = -1;
	pst->bind_enc_num[2] = -1;
	pst->bind_enc_num[3] = -1;
	pst->bind_enc_num[4] = -1;
	pst->bind_enc_num[5] = -1;
	pst->thread_stop_num = 0;	

	if( pst->send_data_mode == NET_SEND_BUF_MODE )
	{
		if( (pst->cycle_send_buf = (CYCLE_BUFFER *)init_cycle_buffer(buf_size)) == NULL )
		{
			DPRINTK("init_cycle_buffer error!\n");
			goto ini_error;
		}
	}else
	{
		if( (pst->cycle_send_buf = (CYCLE_BUFFER *)init_cycle_buffer(0x100000/8/8)) == NULL )
		{
			DPRINTK("init_cycle_buffer error!\n");
			goto ini_error;
		}
	}

	DPRINTK("cycle_send_buf = 0x%x\n",pst->cycle_send_buf);

	filo_set_save_num(pst->cycle_send_buf,0);

	pst->net_get_data = func;

	DPRINTK("work_mode=%d send_data_mode=%d buf_size=%d\n",work_mode,send_mode,buf_size);
	

	return pst;


ini_error:

	if( pst )
	{
		if( pst->cycle_send_buf )
			destroy_cycle_buffer(pst->cycle_send_buf);

		free(pst);
	}

	return NULL;
}



int  NM_reset(NET_MODULE_ST * pst)
{
	int i;
	
	if( pst == NULL )
	{
		DPRINTK("NET_MODULE_ST is NULL!\n");
		goto reset_error;
	}

	filo_set_save_num(pst->cycle_send_buf,0);
	fifo_reset(pst->cycle_send_buf);	

	pst->bind_enc_num[0] = -1;
	pst->bind_enc_num[1] = -1;
	pst->bind_enc_num[2] = -1;
	pst->bind_enc_num[3] = -1;
	pst->bind_enc_num[4] = -1;
	pst->bind_enc_num[5] = -1;
	pst->mac[0] = 0;
	pst->is_alread_work = 0;
	pst->recv_data_num = 0;	
	pst->stop_work_by_user = 0;
	for( i = 0; i < 8; i++)
		pst->enc_info[i] = 0;
	for( i = 0; i < 6; i++)
		pst->cam_chan_recv_num[i] = 0;
	pst->have_d1_enginer = 0;
	pst->ipcam_type = 0;
	pst->thread_stop_num = 0;	
	pst->have_again_flag = 0;

	return 1;

reset_error:
	return -1;
}


int NM_destroy_st(NET_MODULE_ST * pst)
{
	if( pst )
	{
		if( pst->build_once == BUILD_MODE_ALLWAY )
		{
			if( pst->cycle_send_buf )
				destroy_cycle_buffer(pst->cycle_send_buf);

			free(pst);
		}else
		{
			if( NM_reset(pst) < 0 )
				DPRINTK("NM_reset error \n");
		}		
	}

	return 1;
}





void NM_get_host_name(char * pServerIp,char * buf)
{

	int a;
	struct hostent *myhost;
	char ** pp;
	struct in_addr addr;
	int is_url = 0;
	
	for(a = 0; a < 8;a++)
	{
		if( pServerIp[a] == (char)NULL )
			break;

		if( (pServerIp[a] < '0' || pServerIp[a] > '9') 
					&& (pServerIp[a] != '.')  )
		{
			is_url = 1;
			
			 DPRINTK("pServerIp=%s\n",pServerIp);
			//不知道什么原因解析不了域名。
			myhost = gethostbyname(pServerIp); 

			if( myhost == NULL )
			{
				DPRINTK("gethostbyname can't get %s ip\n",pServerIp);
				//herror(pServerIp);
			/*
				printf("use ping to get ip\n");
				get_host_name_by_ping(pServerIp,buf);
				if( buf[0] == 0 )
					printf("get_host_name_by_ping also can't get %s ip!\n",pServerIp);
			*/
				 break;
			}
			 
			DPRINTK("host name is %s\n",myhost->h_name);		 
		 
			//for (pp = myhost->h_aliases;*pp!=NULL;pp++)
			//	printf("%02X is %s\n",*pp,*pp);
			
			pp = myhost->h_addr_list;
			while(*pp!=NULL)
			{
				addr.s_addr = *((unsigned int *)*pp);
				DPRINTK("address is %s\n",inet_ntoa(addr));
				sprintf(buf,"%s",inet_ntoa(addr));
				pp++;
			}
			
			break;
		}
	}

	if( is_url == 0 )
	{
		strcpy(buf,pServerIp);

		DPRINTK("is not url ,set ip buf %s\n",buf);
	}
}




int NM_client_socket_set(int * client_socket,int client_port,int delaytime,int create_socket,int connect_mode)
{
	struct  linger  linger_set;
//	int nNetTimeout;
	struct timeval stTimeoutVal;
//	struct sockaddr_in server_addr;
	int i;
	int keepalive = 1;
	int keepIdle = 10;
	int keepInterval = 3;
	int keepCount = 3;

	if( create_socket == 1 )
	{	
		if( connect_mode == NET_MODE_NORMAL )
		{
			if( -1 == (*client_socket =  socket(PF_INET,SOCK_STREAM,0)))
			{
				DPRINTK( "DVR_NET_socket initialize error\n" );	
				return ERR_NET_INIT1;
			}
		}else
		{
			if( -1 == (*client_socket =  socket(PF_LOCAL,SOCK_STREAM,0)))
			{
				DPRINTK( "DVR_NET_socket initialize error\n" );	
				return ERR_NET_INIT1;
			}
		}
	}

	linger_set.l_onoff = 1;
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



void NM_net_send_thread(void * value)
{
	NET_MODULE_ST * pst = NULL;
	char buf[MAX_FRAME_BUF_SIZE];
	char dest_buf[MAX_FRAME_BUF_SIZE];
	int data_length;
	int convert_length;
	int ret;

	pst = (NET_MODULE_ST *)value;

	DPRINTK("in  client=%d\n",pst->client);

	while( pst->send_th_flag == 1)
	{
	
		if( fifo_enable_get(pst->cycle_send_buf) == 0 )
		{
			usleep(100);
			continue;
		}

		data_length = NM_get_buf(pst,buf);

		convert_length = 0;
		
		if( convert_length >  0 )
		{
			ret = NM_socket_send_data( pst->client, dest_buf, convert_length);
			if( ret < 0 )
			{
				break;
			}
		}else
		{
			if( data_length > 0 )
			{
				ret = NM_socket_send_data( pst->client, buf, data_length);
				if( ret < 0 )
				{
					break;
				}
			}	
		}
		
	}

	pst->send_th_flag = -1;

	DPRINTK("out %d\n",pst->client);

	pthread_detach(pthread_self());
	
	return ;
}

void NM_net_recv_thread(void * value)
{
	NET_MODULE_ST * pst = NULL;
	fd_set watch_set;
	struct  timeval timeout;
	int ret;

	DPRINTK("in\n");

	pst = (NET_MODULE_ST *)value;

	DPRINTK("client = %d\n",pst->client);

	//pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	//pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);


	while( pst->recv_th_flag == 1)
	{
		FD_ZERO(&watch_set); 
 		FD_SET(pst->client,&watch_set);
		timeout.tv_sec = 0;
		timeout.tv_usec = 1000;

		if( pst->have_again_flag == 1 )
			   DPRINTK("[%d] select %d\n",pst->dvr_cam_no,pst->client);

		ret  = select( pst->client + 1, &watch_set, NULL, NULL, &timeout );
		if ( ret < 0 )
		{
			DPRINTK( "recv socket select error!\n" );
			goto error_end;
		}

		if ( ret > 0 && FD_ISSET( pst->client, &watch_set ) )
		{
			if( pst->have_again_flag == 1 )
			   DPRINTK("[%d] recv data %d\n",pst->dvr_cam_no,pst->client);
			
			ret = NM_command_recv_data(pst);
			if( ret < 0 )
			{
			/*	if( pst->client > 0 )
				{		
					close(pst->client);
					pst->client = 0;
				} */
				
					
			
				DPRINTK("NM_command_recv_data error!\n");
				break;
			}
		}	
	
		
	}


error_end:

	pst->recv_th_flag = -1;

	DPRINTK("out %d\n",pst->client);

	pthread_detach(pthread_self());
	
	return ;
}

void NM_net_check_thread(void * value)
{	
	NET_MODULE_ST * pst = (NET_MODULE_ST *)value;
	int  recv_num = 0;
	int connect_lost_time = 15 * 1000000 / 100000;
	int recv_time = 0;

	DPRINTK("in\n");

	while( ( pst->recv_th_flag == 1 ) && 
		( pst->send_th_flag == 1 || pst->send_data_mode == NET_SEND_DIRECT_MODE) 
		&& ( pst->stop_work_by_user == 0) )
	{
		usleep(100000);

		if( recv_num != pst->recv_data_num )
		{
			recv_num = pst->recv_data_num;
			recv_time = 0;
		}else
		{
			if( pst->connect_mode == NET_MODE_NORMAL &&  
				  pst->connect_port < NET_PROCESS_SOCKET )
			{
				recv_time++;

				if( recv_time > connect_lost_time )
				{
					DPRINTK("recv nothing more than connect_lost_time=%d\n",connect_lost_time);
					break;
				}
			}
		}
	}


	if( pst->is_alread_work == 1 )
	{
		NM_destroy_work_thread(pst);
		sleep(2);
		NM_destroy_st(pst);		
	}

	DPRINTK("out \n");

	if(1)
	{
		DPRINTK("tcp socket disconnect, app out\n");
		exit(0);
	}

	pthread_detach(pthread_self());
	
	return ;
}



int NM_create_work_thread(NET_MODULE_ST * pst)
{
	pthread_t netsend_id;
	pthread_t netrecv_id;
	pthread_t netcheck_id;
	int ret;


	if( pst->send_data_mode == NET_SEND_BUF_MODE )
	{
		pst->send_th_flag = 1;

		ret = pthread_create(&pst->netsend_id,NULL,(void*)NM_net_send_thread,(void *)pst);
		if ( 0 != ret )
		{
			pst->send_th_flag = 0;
			DPRINTK( "create NM_net_send_thread thread error\n");
			goto create_error;
		}
	}

	pst->recv_th_flag = 1;

	ret = pthread_create(&pst->netrecv_id,NULL,(void*)NM_net_recv_thread,(void *)pst);
	if ( 0 != ret )
	{
		pst->recv_th_flag = 0;
		DPRINTK( "create NM_net_recv_thread thread error\n");	
		goto create_error;
	}

	pst->is_alread_work = 1;

	ret = pthread_create(&pst->netrecv_id,NULL,(void*)NM_net_check_thread,(void *)pst);
	if ( 0 != ret )
	{		
		DPRINTK( "create NM_net_check_thread thread error\n");
		goto create_error;
	}

	usleep(100*1000);


	pthread_detach(pthread_self());
	

	DPRINTK("ok\n");
	
	return 1;

create_error:

	NM_destroy_work_thread(pst);

	pthread_detach(pthread_self());

	return -1;
}


void test_pthread(pthread_t tid) /*pthread_kill的返回值：成功（0） 线程不存在（ESRCH） 信号不合法（EINVAL）*/   
{   
    int pthread_kill_err;   
    pthread_kill_err = pthread_kill(tid,0);   
   
    if(pthread_kill_err == ESRCH)   
        DPRINTK("tid=%d \n",(unsigned int)tid);   
    else if(pthread_kill_err == EINVAL)   
        DPRINTK("发送信号非法。\n");   
    else   
        DPRINTK("ID为0x%x的线程目前仍然存活。\n",(unsigned int)tid);   
} 

int NM_destroy_work_thread(NET_MODULE_ST * pst)
{
	int count = 0;

	if( pst->send_data_mode == NET_SEND_BUF_MODE )
	{
		if( pst->send_th_flag == 1 )
		{	
			pst->send_th_flag = 0;

			count = 0;

			while(1)
			{
				usleep(10000);

				if( pst->send_th_flag == - 1 )
				{
					pst->send_th_flag = 0;
				
					DPRINTK("NM_net_send_thread out! %d\n",pst->client);

					break;
				}else
				{
					count++;

					if( count > 30 )
					{
						pst->send_th_flag = 0;
						DPRINTK("NM_net_send_thread force out! %d\n",pst->client);
						break;
					}
				}
			}
		}else
		{
			pst->send_th_flag = 0;
		}
	}

	if( pst->recv_th_flag == 1 )
	{	
		pst->recv_th_flag = 0;

		count = 0;

		while(1)
		{
			usleep(10000);

			if( pst->recv_th_flag == - 1 )
			{
				pst->recv_th_flag = 0;
			
				DPRINTK("NM_net_recv_thread out! %d\n",pst->client);

				break;
			}else
			{
				count++;

				if( count > 30 )
				{
					pst->recv_th_flag = 0;
					DPRINTK("NM_net_recv_thread force out! %d\n",pst->client);

					if( pst->stop_work_by_user == 0 )
					{
						DPRINTK("pthread_cancel %d\n",pst->netrecv_id);
						pthread_cancel(pst->netrecv_id);
					}
					
					break;
				}
			}
		}
	}else
	{
		pst->recv_th_flag = 0;
	}


	if( pst->client > 0 )
	{		
		if( pst->have_again_flag == 1 )
		{
			DPRINTK("[%d ] socket again,not close\n");
		}else		
		close(pst->client);
		pst->client = 0;
	}

	DPRINTK("out\n");

	return 1;
}





int NM_connect(NET_MODULE_ST * pst,char * ip,int port)
{
	char ip_buf[200];
	int a;
	int socket_fd = -1;
	int ret;
	struct sockaddr_in addr;
	struct sockaddr_un sa = {
                .sun_family = AF_LOCAL,
                .sun_path = "11"
        };

	sprintf(sa.sun_path,"/dev/%d",port);

	signal(SIGPIPE,SIG_IGN);
 
	memset(ip_buf,0,200);
	NM_get_host_name(ip,ip_buf);

	// 没有得到dns.
	if( ip_buf[0] == 0 )
	{
		for(a = 0; a < 8;a++)
		{
			if( ip[a] == (char)NULL )
				break;

			if( (ip[a] < '0' || ip[a] > '9') 
						&& (ip[a] != '.')  )
			{			
				DPRINTK("url is %s,but can not get ip\n",ip);
				return -1;
			}
		}
	}

	ret = NM_client_socket_set(&socket_fd,0,7,1,pst->connect_mode);
	if( ret < 0 )
	{
		DPRINTK("NM_client_socket_set error,ret=%d\n",ret);
		return ret;
	}

	pst->client = socket_fd;

	DPRINTK("pst->client = %d\n",pst->client);

	addr.sin_family = AF_INET;
	if( ip_buf[0] == 0 )
  		addr.sin_addr.s_addr = inet_addr(ip);
	else
		addr.sin_addr.s_addr = inet_addr(ip_buf);
	addr.sin_port = htons(port);	

	DPRINTK("ip=%s  port=%d \n",ip,port);

	pst->thread_stop_num = 0;


	if( pst->connect_mode == NET_MODE_NORMAL )
	{
		if (connect(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) 
		{
			perror(strerror(errno));
			
	        if (errno == EINPROGRESS) 
			{			
	             fprintf(stderr, "connect timeout\n");	
			}  

			DPRINTK("connect error 123!\n");
			
			goto connect_error;
	       }
	}else
	{
		if (connect(socket_fd, (struct sockaddr*)&sa, sizeof(sa)) == -1) 
		{
			perror(strerror(errno));
			
	        	if (errno == EINPROGRESS) 
			{			
	             		fprintf(stderr, "connect timeout\n");	
			}  

			DPRINTK("connect error 123!\n");			
			goto connect_error;
	    	}
	}
	
	if( NM_create_work_thread(pst) < 0 )
		goto connect_error;


	return 1;

connect_error:

	if(  pst->client )
	{			
		close( pst->client);
		pst->client = 0;		
	}
	
	 pst->is_alread_work = 0;
	
	return -1;
}



int NM_disconnect(NET_MODULE_ST * pst)
{
	NM_destroy_work_thread(pst);

	if(  pst->client )
	{			
		close( pst->client);
		pst->client = 0;		
	}

	pst->is_alread_work = 0;

	return 1;
}

int NM_connect_by_accept(NET_MODULE_ST * pst,int socket_fd)
{
	char ip_buf[200];
	int a;	
	int ret;


	ret = NM_client_socket_set(&socket_fd,0,15,0,pst->connect_mode);
	if( ret < 0 )
	{
		DPRINTK("NM_client_socket_set error,ret=%d\n",ret);
		return ret;
	}

	DPRINTK("tmp_fd =%d\n",socket_fd);

	pst->client = socket_fd;	

	if( NM_create_work_thread(pst) < 0 )
		goto connect_error;


	return 1;

connect_error:

	if(  pst->client )
	{			
		close( pst->client);
		pst->client = 0;		
	}
	
	return -1;
}


////////////////////////////////////////////////////////////////
//以下为dvr具体操作
static int g_listen_thread_work = 0;

int net_recv_data(void * pst,char * buf,int length)
{
	DPRINTK("buf length = %d\n",length);
	return 1;
}



int Net_dvr_send_cam_data(char * buf,int length,NET_MODULE_ST * server_pst)
{
	int ret;
	
	if( length >  200*1024)
	{
		DPRINTK("length = %d too bigger!\n",length);
		return -1;
	}

	if( server_pst )
	{
		if( NM_net_send_data(server_pst,buf,length,NET_DVR_NET_COMMAND) > 0 )
		{
			return 1;
		}
	}
	
	return -1;	
}

int CheckStuctDataIsSame(char* data1,char * data2,int len)
{
	int i;

	for( i = 0; i < len; i++)
	{
		if( data1[i] != data2[i] )
			return -1;
	}

	return 1;
}




void dvr_net_listen_thread(void * value)
{
	LISTEN_ST listen_st;
	LISTEN_ST * p_listen = NULL;
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
	NET_MODULE_ST * p_net_client = NULL;
	struct sockaddr_un sa = {
                .sun_family = AF_LOCAL,
                .sun_path = "11"
        };
	int chan =0;

	listen_st  = *(LISTEN_ST *)value;
	p_listen = (LISTEN_ST *)value;
	p_listen->is_create = 1;
	
	DPRINTK(" port = %d in\n",listen_st.listen_port);

	if( listen_st.listen_port < NET_PROCESS_TO_RTSP_SOCKET )
	{
		chan = listen_st.listen_port - NET_PROCESS_SOCKET;
	}else
	{
		chan = listen_st.listen_port;
	}

	if( listen_st.connect_mode == NET_MODE_NORMAL)
	{
		if( -1 == (listen_fd =  socket(PF_INET,SOCK_STREAM,0)))
		{
			DPRINTK( "Socket initialize error\n" );
			
			goto error_end;
		}
	}else
	{
		if( -1 == (listen_fd =  socket(PF_LOCAL,SOCK_STREAM,0)))
		{
			DPRINTK( "Socket initialize error\n" );
			
			goto error_end;
		}

		sprintf(sa.sun_path,"/dev/%d",listen_st.listen_port);
		unlink(sa.sun_path);
		DPRINTK("listen path:%s\n",sa.sun_path);
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
	server_addr.sin_port=htons(listen_st.listen_port); 


	if( listen_st.connect_mode == NET_MODE_NORMAL)
	{
		if ( -1 == (bind(listen_fd,(struct sockaddr *)(&server_addr),
			sizeof(struct sockaddr))))
		{		
			DPRINTK( "DVR_NET_Initialize socket bind error\n" );
			goto error_end;
		}
	}else
	{
		if ( -1 == (bind(listen_fd,(struct sockaddr *)(&sa),
		sizeof(sa))))
		{		
			DPRINTK( "DVR_NET_Initialize socket bind error\n" );
			perror(strerror(errno));
			goto error_end;
		}
	}

	if ( -1 == (listen(listen_fd,5)))
	{
	
		DPRINTK( "DVR_NET_Initialize socket listen error\n" );
		goto error_end;
	}

	sin_size = sizeof(struct sockaddr_in);

	while( g_listen_thread_work )
	{
		FD_ZERO(&watch_set); 
 		FD_SET(listen_fd,&watch_set);
		timeout.tv_sec = 0;
		timeout.tv_usec = 1000;

		ret  = select( listen_fd + 1, &watch_set, NULL, NULL, &timeout );
		if ( ret < 0 )
		{
			DPRINTK( "listen socket select error!\n" );
			goto error_end;
		}

		if ( ret > 0 && FD_ISSET( listen_fd, &watch_set ) )
		{
			if( listen_st.connect_mode == NET_MODE_NORMAL)
			{
				tmp_fd = accept( listen_fd,(struct sockaddr *)&adTemp, &sin_size);
			}else
			{
				tmp_fd = accept( listen_fd,(struct sockaddr *)&uadTemp, &sin_size);
			}
		
			if ( -1 == tmp_fd )
			{
				//	DPRINTK( "DVR_NET_Initialize accept error 2\n" );
			}else
			{
				//创建新的client_socket.				

				DPRINTK("tmp_fd =%d\n",tmp_fd);


				if( chan >= 0 )
				{

					if( listen_st.listen_port == NET_PROCESS_TO_ONVIF_SOCKET)
					{
						p_net_client = pCommuniteToOnvif;
						p_net_client->connect_port = listen_st.listen_port;
						DPRINTK("pCommuniteToOnvif connected\n");
					}else 
					{
						p_net_client = pNetModule[chan];
						if( p_net_client->client > 0 )
						{
							int count = 0;
							p_net_client->stop_work_by_user = 1;

							while(count < 50)
							{
								usleep(100000);								
								if( p_net_client->stop_work_by_user == 0 )
									break;
							}
							usleep(100000);
						}

						DPRINTK("pNetModule[%d] connected\n",chan);
					}
					
					if( p_net_client != NULL )
					{

						p_net_client->connect_mode = NET_MODE_NORMAL;
						p_net_client->connect_port = listen_st.listen_port;

						DPRINTK("accpet port=%d\n",p_net_client->connect_port );
						
						result = NM_connect_by_accept(p_net_client,tmp_fd);

						if( result < 0 )
						{
							DPRINTK("NM_connect_by_accept error: %s\n",inet_ntoa(adTemp.sin_addr));	

							NM_destroy_st(p_net_client);
						}else
						{
							DPRINTK("NM_connect_by_accept : %s\n",inet_ntoa(adTemp.sin_addr));

							

						}
					}
				}
			}
		}	
	
	}


error_end:

	if( listen_fd > 0 )
	{
		close(listen_fd);
		listen_fd = 0;
	}

	g_listen_thread_work = -1;

	DPRINTK(" out \n");

	pthread_detach(pthread_self());
	
	
	return ;
}


int dvr_net_create_cam_listen(LISTEN_ST listen_st)
{
	int i;
	pthread_t td_id;
	int ret;
	int count;
	static LISTEN_ST s_listen;	

	s_listen = listen_st;

/*
	if( g_listen_thread_work == 1 )
	{
		g_listen_thread_work = 0;

		count = 0;

		while(1)
		{
			usleep(100000);

			if( g_listen_thread_work == - 1 )
			{
				g_listen_thread_work = 0;
			
				DPRINTK("dvr_net_listen_thread out!\n");

				break;
			}else
			{
				count++;

				if( count > 30 )
				{
					g_listen_thread_work = 0;
					DPRINTK("dvr_net_listen_thread force out!\n");
					break;
				}
			}
		}
	}
*/
	g_listen_thread_work = 1;

	s_listen.is_create = 0;
	ret = pthread_create(&td_id,NULL,(void*)dvr_net_listen_thread,(void *)&s_listen);
	if ( 0 != ret )
	{			
		DPRINTK( "create dvr_net_listen_thread port=%d  connectmode=%d thread error\n",
			s_listen.listen_port,s_listen.connect_mode);	

		goto create_error;
	}	

	while(s_listen.is_create == 0 )
	{
		usleep(50);
	}


	return 1;

create_error:
	
	g_listen_thread_work = 0;

	return -1;
}





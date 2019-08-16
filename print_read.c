#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<errno.h>
#include<termios.h>

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
//#include "devfile.h"

#define DPRINTK(fmt, args...)	printf("(%s,%d)%s: " fmt,__FILE__,__LINE__, __FUNCTION__ , ## args)

static int old_fd1 = 0,old_fd2 = 0;;

static int open_port(int fd,int comport)
{  
    fd=open("/dev/ttyAMA0",O_RDONLY||O_NONBLOCK);
    if(-1==fd)
    {
         perror("can't open serial port");
        return(-1);
    	}
  
    if(fcntl(fd,F_SETFL,0)<0)
       printf("fcnt failed!\n");
    else
    	printf("fcntl=%d\n",fcntl(fd,F_SETFL,0));
    if(isatty(STDIN_FILENO)==0)
   	 printf("standard input is not a terminal device\n");
    else
    	printf("isatty success!\n");
	
    return fd;
    
}
static  int set_opt(int fd,int nSpeed,int nBits,int nFctl,char nEvent,int nStop)
{
    struct termios newtio,oldtio;
    if(tcgetattr(fd,&oldtio)!=0)
    {
        perror("SetupSerial 1");
        exit(1);
    }
    bzero(&newtio,sizeof(newtio));
    newtio.c_cflag |=CLOCAL | CREAD;//
    newtio.c_cflag &= ~CSIZE;
    //newtio.c_iflag &= ~(ICRNL|IGNCR);
    //newtio.c_lflag &=~(ICANON|ECHO|ECHOE|ISIG);
    //newtio.c_oflag &=~OPOST;
    switch(nBits)
    {
    case 7:
        newtio.c_cflag |= CS7;
        break;
    case 8:
        newtio.c_cflag |= CS8;
        break;
    }
    switch(nFctl)
    {
    case 0:
        newtio.c_cflag &=~CRTSCTS;
        break;
    case 1:
        newtio.c_cflag |=CRTSCTS;
        break;
    case 2:
        newtio.c_cflag |=IXON|IXOFF|IXANY;
        break;
    }
    switch(nEvent)
    {
    case'O':
        newtio.c_cflag |= PARENB;//enble
        newtio.c_cflag |= PARODD;
        newtio.c_iflag |= (INPCK| ISTRIP);
        break;
    case'E':
        newtio.c_iflag |= (INPCK | ISTRIP);
        newtio.c_cflag |= PARENB;
        newtio.c_cflag &= ~PARODD;
        break;
    case'N':
        newtio.c_cflag &=~PARENB;
        break;
    }
    switch(nSpeed)
    {
    case 2400:
        cfsetispeed(&newtio,B2400);
        cfsetospeed(&newtio,B2400);
        break;
    case 4800:
        cfsetispeed(&newtio,B4800);
        cfsetospeed(&newtio,B4800);
        break;
    case 9600:
        cfsetispeed(&newtio,B9600);
        cfsetospeed(&newtio,B9600);
        break;
    case 115200:
        cfsetispeed(&newtio,B115200);
        cfsetospeed(&newtio,B115200);
        break;
    default:
        cfsetispeed(&newtio,B9600);
        cfsetospeed(&newtio,B9600);
        break;
    }
    if(nStop==1)
        newtio.c_cflag &=~CSTOPB;
    else if(nStop==2)
        newtio.c_cflag |= CSTOPB;
    newtio.c_cc[VTIME]=5;
    newtio.c_cc[VMIN]=0;
    //newtio.c_lflag &=~(ICANON|ECHO|ECHOE|ISIG);
    //newtio.c_oflag &=~OPOST;
    tcflush(fd,TCIFLUSH);
    if((tcsetattr(fd,TCSANOW,&newtio))!=0)
    {
        perror("com set error");
        return -1;
    }
    printf("set done!\n");
    return 0;
}

int main33(void)
{
  int fd=-1;
  int i,nread;
   char buff[512]="hello";

   start_printf_send_th();

   while(1)
   {
   	sleep(1);
	DPRINTK("test\n");
   }

   return 1;
  if((fd=open_port(fd,0))<0)
   {
        perror("open_port erro");
        exit(1) ;
   }
/*
  if((i=set_opt(fd,115200,8,0,'N',1))<0)
  {
    perror("set_opt error");
    return 1;
  } 
  */
  printf("ID IS: %d\n",fd);
while(1)
{
  while((nread=read(fd,buff,512))>0)
  {

     //  printf("len %d\n",nread);
//    nread=read(fd,buff,512);
    //write(fd, "1" , 1); 
    printf("\n%s\n",buff);    
  }
}

if(close(fd)<0){
    perror("close:");
    exit(1);
   }
   else
    printf("close ok");
   exit(0);

}






static int NM_client_socket_set2(int * client_socket,int client_port,int delaytime,int create_socket)
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
		if( -1 == (*client_socket =  socket(PF_INET,SOCK_STREAM,0)))
		{
			DPRINTK( "DVR_NET_socket initialize error\n" );	
			return -1;
		}
	}

	linger_set.l_onoff = 1;
	linger_set.l_linger = 0;

    if (setsockopt( *client_socket, SOL_SOCKET, SO_LINGER , (char *)&linger_set, sizeof(struct  linger) ) < 0 )
    {
		printf( "setsockopt SO_LINGER  error\n" );
		return -1;	
	}

	
    if (setsockopt(*client_socket, SOL_SOCKET, SO_KEEPALIVE, (char *)&keepalive, sizeof(int) ) < 0 )
    {
		printf( "setsockopt SO_KEEPALIVE error\n" );
		return -1;	
	}

	
	if(setsockopt( *client_socket,SOL_TCP,TCP_KEEPIDLE,(void *)&keepIdle,sizeof(keepIdle)) < -1)
	{
		printf( "setsockopt TCP_KEEPIDLE error\n" );
		return -1;	
	}

	
	if(setsockopt( *client_socket,SOL_TCP,TCP_KEEPINTVL,(void *)&keepInterval,sizeof(keepInterval)) < -1)
	{
		printf( "setsockopt TCP_KEEPINTVL error\n" );
		return -1;	
	}

	
	if(setsockopt( *client_socket,SOL_TCP,TCP_KEEPCNT,(void *)&keepCount,sizeof(keepCount)) < -1)
	{
		printf( "setsockopt TCP_KEEPCNT error\n" );
		return -1;	
	}



	i = 1;

	if (setsockopt(*client_socket, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(int)) < 0) 
	{		
		close( *client_socket );
		*client_socket = 0;
		DPRINTK("setsockopt SO_REUSEADDR error \n");		
		return -1;	
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
			return -1;
		}
			 	
		if ( setsockopt( *client_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&stTimeoutVal, sizeof(struct timeval) ) < 0 )
	       {
	       	close( *client_socket );
			*client_socket = 0;
			printf( "setsockopt SO_RCVTIMEO error\n" );
			return -1;
		}
	  }



	return 1;
	   
}


#define RECV_BUF_SIZE  (1000)

static void NM_net_recv_thread2(void * value)
{
	
	fd_set watch_set;
	struct  timeval timeout;
	int ret;
	int socket_rd;

	DPRINTK("in\n");

	socket_rd = *(int *)value;

	DPRINTK("socket_rd = %d\n",socket_rd);

	while( 1)
	{
		FD_ZERO(&watch_set); 
 		FD_SET(socket_rd,&watch_set);
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		ret  = select( socket_rd + 1, &watch_set, NULL, NULL, &timeout );
		if ( ret < 0 )
		{
			DPRINTK( "recv socket select error!\n" );
			goto error_end;
		}		

		if ( ret > 0 && FD_ISSET( socket_rd, &watch_set ) )
		{					
			char buf[RECV_BUF_SIZE];		
			ret = recv( socket_rd, buf, RECV_BUF_SIZE, 0 );
			if( ret <= 0 )
			{
				DPRINTK("recv err %d\n",ret);
				break;		
			}
		}	
	
		
	}


error_end:

	DPRINTK("out %d\n",socket_rd);

	if(  socket_rd)
	{			
		close( socket_rd);
		socket_rd = 0;		
	}

	dup2(old_fd1,   1);	
	dup2(old_fd2,   2);	

	pthread_detach(pthread_self());
	
	return ;
}



static void create_socket_recv(int socket_fd)
{
	int ret;
	pthread_t netsend_id;
	pthread_t netrecv_id;
	pthread_t netcheck_id;
	static int accept_socket_fd = 0;

	accept_socket_fd = socket_fd;
	
	ret = NM_client_socket_set2(&accept_socket_fd,0,7,0);
	if( ret < 0 )
	{
		DPRINTK("NM_client_socket_set error,ret=%d\n",ret);
		goto err;
	}


	ret = pthread_create(&netrecv_id,NULL,(void*)NM_net_recv_thread2,(void *)&accept_socket_fd);
	if ( 0 != ret )
	{		
		DPRINTK( "create NM_net_recv_thread thread error\n");	
		goto err;
	} 

	
	if( dup2(socket_fd,   1) < 0 )
	{
		perror(strerror(errno));

		goto err;
	}
	
	if( dup2(socket_fd,   2) < 0 )
	{
		perror(strerror(errno));

		goto err;
	}

	sleep(1);	

	return ;

err:

	if(  socket_fd)
	{			
		close( socket_fd);
		socket_fd = 0;		
	}

	return ;
}


static int prinf_read(int listen_port) 
{	
	int listen_fd = 0;
	struct sockaddr_in adTemp;
	struct  linger  linger_set;
	int i,sin_size;
	struct  timeval timeout;
	int ret;
	int tmp_fd = 0;	
	struct sockaddr_in server_addr;
	fd_set watch_set;
	int result;

	old_fd1 = dup(1);
	old_fd2 = dup(2);

	signal(SIGPIPE,SIG_IGN);

	//listen_port  =  28002;
	
	DPRINTK(" port = %d in\n",listen_port);

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
	server_addr.sin_port=htons(listen_port); 

	if ( -1 == (bind(listen_fd,(struct sockaddr *)(&server_addr),
		sizeof(struct sockaddr))))
	{		
		DPRINTK( "DVR_NET_Initialize socket bind error\n" );
		goto error_end;
	}

	if ( -1 == (listen(listen_fd,5)))
	{
	
		DPRINTK( "DVR_NET_Initialize socket listen error\n" );
		goto error_end;
	}

	sin_size = sizeof(struct sockaddr_in);

	while( 1 )
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
			if ( -1 == (tmp_fd = accept( listen_fd,
					(struct sockaddr *)&adTemp, &sin_size)))
			{
					DPRINTK( "DVR_NET_Initialize accept error 2\n" );
			}else
			{				
				create_socket_recv(tmp_fd);
				
			}
		}	
		
	
	}


error_end:

	if( listen_fd > 0 )
	{
		close(listen_fd);
		listen_fd = 0;
	}	

	DPRINTK(" out \n");

 
  return 1;
}

static void net_dvr_prinf_read(void * value)
{
	int port = *(int*)value;
	prinf_read(port);
	
	return ;
}


int  start_printf_send_th(int port)
{
	int ret;
	pthread_t id_net_print_catch;
	static int static_port = 0;

	static_port = port;
	
	ret = pthread_create(&id_net_print_catch,NULL,(void*)net_dvr_prinf_read,(void*)&static_port);
	if ( 0 != ret )
	{
		DPRINTK( "create net_dvr_prinf_read  thread error\n");
		return -1;
	}	

	return 1;
}


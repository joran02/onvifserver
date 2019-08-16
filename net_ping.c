
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<netinet/ip_icmp.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <errno.h>

#include "net_ping.h"

#define PACKET_SIZE     4096


// 效验算法
unsigned short cal_chksum(unsigned short *addr, int len)
{
    int nleft=len;
    int sum=0;
    unsigned short *w=addr;
    unsigned short answer=0;
    
    while(nleft > 1)
    {
           sum += *w++;
        nleft -= 2;
    }
    
    if( nleft == 1)
    {       
        *(unsigned char *)(&answer) = *(unsigned char *)w;
           sum += answer;
    }
    
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;
    
    return answer;
}

int net_is_ip(char * url)
{
	int a;
	int i;
	int len;
	int have_point = 0;

	len = strlen(url);

	if( len < 7 )
		return -1;


	for( i = 0; i < len;i++)
	{
		if( (url[i] >= '0') &&  (url[i] <= '9')  )
		{
		}else
		{
			if( url[i] == '.' )
		   		 have_point = 1;
			else			
			{
				//printf("2222 ==%d %c\n",i,url[i]);
				return -1;
			}
		}
	}

	//printf("2\n");

	if( have_point == 0 )
		return -1;	
	return 1;
}

// Ping函数
int net_ping( char *ips)
{
    struct timeval timeo;
    int sockfd;
    struct sockaddr_in addr;
    struct sockaddr_in from;
    
    struct timeval *tval;
    struct ip *iph;
    struct icmp *icmp;
	int timeout = 1000;

    char sendpacket[PACKET_SIZE];
    char recvpacket[PACKET_SIZE];
    
    int n;
    pid_t pid;
    int maxfds = 0;
    fd_set readfds;

    if( net_is_ip(ips) < 0 )
    {
    		//printf("%s is not ip\n",ips);
		return -1;
    }
    
    // 设定Ip信息
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ips);  

    // 取得socket
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) 
    {
        printf("ip:%s,socket error\n",ips);
	 printf("%s\n", strerror(errno));
	printf("%s\n", strerror(errno));	
	 exit(0);
        goto ping_err;
    }
    
    // 设定TimeOut时间
    timeo.tv_sec = timeout / 1000;
    timeo.tv_usec = timeout % 1000;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo)) == -1)
    {
        printf("ip:%s,setsockopt error\n",ips);
        goto ping_err;
    }
    
    // 设定Ping包
    memset(sendpacket, 0, sizeof(sendpacket));
    
    // 取得PID，作为Ping的Sequence ID
    pid=getpid();
    int i,packsize;
    icmp=(struct icmp*)sendpacket;
    icmp->icmp_type=ICMP_ECHO;
    icmp->icmp_code=0;
    icmp->icmp_cksum=0;
    icmp->icmp_seq=0;
    icmp->icmp_id=pid;
    packsize=8+56;
    tval= (struct timeval *)icmp->icmp_data;
    gettimeofday(tval,NULL);
    icmp->icmp_cksum=cal_chksum((unsigned short *)icmp,packsize);

    // 发包
    n = sendto(sockfd, (char *)&sendpacket, packsize, 0, (struct sockaddr *)&addr, sizeof(addr));
    if (n < 1)
    {
        printf("ip=%s,sendto error\n",ips);
        goto ping_err;
    }

    // 接受
    // 由于可能接受到其他Ping的应答消息，所以这里要用循环
    while(1)
    {
        // 设定TimeOut时间，这次才是真正起作用的
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        maxfds = sockfd + 1;
        n = select(maxfds, &readfds, NULL, NULL, &timeo);
        if (n <= 0)
        {
            //printf("ip:%s,Time out error\n",ips);
            goto ping_err;
        }

        // 接受
        memset(recvpacket, 0, sizeof(recvpacket));
        int fromlen = sizeof(from);
        n = recvfrom(sockfd, recvpacket, sizeof(recvpacket), 0, (struct sockaddr *)&from, &fromlen);
        if (n < 1) {
            break;
        }
        
        // 判断是否是自己Ping的回复
        char *from_ip = (char *)inet_ntoa(from.sin_addr);
        // printf("fomr ip:%s\n",from_ip);
         if (strcmp(from_ip,ips) != 0)
         {
           // printf("ip:%s,Ip wang\n",ips);
            continue;
         }
        
        iph = (struct ip *)recvpacket;
    
        icmp=(struct icmp *)(recvpacket + (iph->ip_hl<<2));

       // printf("ip:%s,icmp->icmp_type:%d,icmp->icmp_id:%d\n",ips,icmp->icmp_type,icmp->icmp_id);
       // 判断Ping回复包的状态
        if (icmp->icmp_type == ICMP_ECHOREPLY && icmp->icmp_id == pid)
        {
            // 正常就退出循环
            break;
        } 
        else
        {
            // 否则继续等
            continue;
        }
    }
    
    // 关闭socket
     if( sockfd > 0 )
     	close(sockfd);

   // printf("ip:%s,Success\n",ips);
    return 1;

ping_err:
	 if( sockfd > 0 )
     	close(sockfd);
     return -1;
}


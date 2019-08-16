//////////////////////////////////////////////////////////////////////

#ifndef _BUF_ARRAY_H_
#define _BUF_ARRAY_H_

#include "BaseData.h"

#define BUFFSIZE 1024 * 1024

#define min(x, y) ((x) < (y) ? (x) : (y))
#define DPRINTK(fmt, args...)	printf("onvif (%s,%d)%s: " fmt,__FILE__,__LINE__, __FUNCTION__ , ## args)



typedef struct cycle_buffer {
        unsigned char *buf; /* store data */
        unsigned int size; /* cycle buffer size */
        unsigned int in; /* next write position in buffer */
        unsigned int out; /* next read position in buffer */
		int item_num;		
		int save_num;
        pthread_mutex_t lock;
		pthread_mutex_t read_lock;		
		int total_write;
		int total_read;
		int cam_no;
}CYCLE_BUFFER;

CYCLE_BUFFER * init_cycle_buffer(int size);
void destroy_cycle_buffer(CYCLE_BUFFER * fifo);
void filo_set_save_num(CYCLE_BUFFER * fifo,int num);
unsigned int fifo_get(CYCLE_BUFFER * fifo,unsigned char *buf,unsigned int len);
unsigned int fifo_put(CYCLE_BUFFER * fifo,unsigned char *buf,unsigned int len);
void fifo_reset(CYCLE_BUFFER * fifo);
unsigned int fifo_len(CYCLE_BUFFER * fifo);
int fifo_num(CYCLE_BUFFER * fifo);
void fifo_lock_buf(CYCLE_BUFFER * fifo);
void fifo_unlock_buf(CYCLE_BUFFER * fifo);
void fifo_read_lock_buf(CYCLE_BUFFER * fifo);
void fifo_read_unlock_buf(CYCLE_BUFFER * fifo);
int fifo_enable_insert(CYCLE_BUFFER * fifo);














#endif // !defined(_BUF_ARRAY_H_)

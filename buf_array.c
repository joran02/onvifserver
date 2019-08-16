 //////////////////////////////////////////////////////////////////////

#include <pthread.h>
#include "buf_array.h"


CYCLE_BUFFER * init_cycle_buffer(int size)
{
        int ret;
		CYCLE_BUFFER * fifo;

        ret = size & (size - 1);
		DPRINTK("size=%d ret=%d \n",size,ret);
        if (ret)
        {
        	DPRINTK("size=%d ret=%d error\n",size,ret);
                goto err;
        }

        fifo = (struct cycle_buffer *) malloc(sizeof(CYCLE_BUFFER));
        if (!fifo)
        {
        	DPRINTK("malloc_err size=%d\n",sizeof(CYCLE_BUFFER));
                return NULL;	
        }

        memset(fifo, 0x00, sizeof(CYCLE_BUFFER));
        fifo->size = size;
        fifo->in = fifo->out = 0;
        pthread_mutex_init(&fifo->lock, NULL);
		pthread_mutex_init(&fifo->read_lock, NULL);
		
		
        fifo->buf = (unsigned char *) malloc(size);
        if (!fifo->buf)
        {
        	DPRINTK("malloc_err size=%d\n",size);
                goto malloc_err;
        }

        memset(fifo->buf, 0x00, size);

		fifo->item_num = 0;
		fifo->save_num = 1;

		fifo->total_read = 0;
		fifo->total_write= 0;

		printf("init size = %d\n",size);

        return fifo;
malloc_err:
        free(fifo);
err:
        return NULL;
}

void destroy_cycle_buffer(CYCLE_BUFFER * fifo)
{
	if( fifo )
	{
		if( fifo->buf )
		{
			free(fifo->buf);
			fifo->buf = NULL;
		}

		free(fifo);		
	}
}

void filo_set_save_num(CYCLE_BUFFER * fifo,int num)
{
	if( fifo == NULL )
	{
		DPRINTK("fifo is NULL \n");
		return ;
	}

	fifo->save_num = num*2;

	DPRINTK("savenum = %d\n",num);
}


unsigned int fifo_get(CYCLE_BUFFER * fifo,unsigned char *buf,unsigned int len)
{
        unsigned int l;
		unsigned int old = len;

		if(len == 0 )
		{
			DPRINTK("len=%d\n",len);
			DPRINTK("cycle_send_buf = 0x%x",fifo);
			return len;
		}

        len = min(len, fifo->in - fifo->out);
        l = min(len, fifo->size - (fifo->out & (fifo->size - 1)));

		if( (len >= 256*1024) || (l >= 256*1024) )
		{
			DPRINTK("%d %d\n",len,l);
		}
		
        memcpy(buf, fifo->buf + (fifo->out & (fifo->size - 1)), l);
        memcpy(buf + l, fifo->buf, len - l);

        fifo->out += len;		
		
		fifo->total_read++;

		//fifo->item_num--;

		if( fifo->total_read >= 1000000)
			fifo->total_read = 0;

		if( fifo->total_write >= fifo->total_read )		
			fifo->item_num = fifo->total_write-fifo->total_read;
		else		
		{
			fifo->item_num = fifo->total_write + 1000000 -fifo->total_read;
			//DPRINTK("w=%d r=%d n=%d\n",fifo->total_write,fifo->total_read,fifo->item_num);
		}
		
	

        return len;
}

unsigned int fifo_put(CYCLE_BUFFER * fifo,unsigned char *buf,unsigned int len)
{
        unsigned int l;		
		static int count1 = 0;

		if(len == 0 )
		{
			DPRINTK("len=%d\n",len);
			DPRINTK("cycle_send_buf = 0x%x",fifo);
			return len;
		}

		
        len = min(len, fifo->size - fifo->in + fifo->out);
        l = min(len, fifo->size - (fifo->in & (fifo->size - 1)));

		
        memcpy(fifo->buf + (fifo->in & (fifo->size - 1)), buf, l);
        memcpy(fifo->buf, buf + l, len - l);	

        fifo->in += len;
		fifo->total_write++;

		//fifo->item_num++;
		
		if( fifo->total_write >= 1000000)
			fifo->total_write = 0;

		if( fifo->total_write >= fifo->total_read )		
			fifo->item_num = fifo->total_write-fifo->total_read;
		else		
		{
			fifo->item_num = fifo->total_write + 1000000 -fifo->total_read;
			//DPRINTK("w=%d r=%d n=%d\n",fifo->total_write,fifo->total_read,fifo->item_num);
		}


		count1++;

		if( count1 > 1000)
			{
			//DPRINTK("w=%d r=%d n=%d\n",fifo->total_write,fifo->total_read,fifo->item_num);
			count1 = 0;
			}


        return len;
}

void fifo_reset(CYCLE_BUFFER * fifo)
{
	DPRINTK("fifo_reset %d %d %d\n",fifo->in , fifo->out ,fifo->item_num );
	
	fifo->item_num = 0;
	fifo->in = fifo->out = 0;	

	fifo->total_read = 0;
	fifo->total_write= 0;
}

unsigned int fifo_len(CYCLE_BUFFER * fifo)
{	
	return fifo->in - fifo->out;	
}

int fifo_num(CYCLE_BUFFER * fifo)
{	
	return fifo->item_num;	
}

void fifo_lock_buf(CYCLE_BUFFER * fifo)
{
	pthread_mutex_lock(&fifo->lock);
}

void fifo_unlock_buf(CYCLE_BUFFER * fifo)
{
	pthread_mutex_unlock(&fifo->lock);
}

void fifo_read_lock_buf(CYCLE_BUFFER * fifo)
{
	pthread_mutex_lock(&fifo->read_lock);
}

void fifo_read_unlock_buf(CYCLE_BUFFER * fifo)
{
	pthread_mutex_unlock(&fifo->read_lock);
}

int fifo_enable_insert(CYCLE_BUFFER * fifo)
{
	if( fifo_len(fifo)  > ((int)(fifo->size) - 1024) )	
		return 0;

	return 1;
}

int fifo_enable_get(CYCLE_BUFFER * fifo)
{

	if( fifo->item_num  <= fifo->save_num )
	{
		if( (fifo_enable_insert(fifo) == 0) && (fifo->item_num >= 2 )  )
			return 1;	
		
		return 0;
	}else
	{
		if( fifo->item_num < 2 )
			return 0;
	}

	return 1;
}

 

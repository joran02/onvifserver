SOURCE = $(wildcard *.c)  
OBJS = $(patsubst %.c,%.o,$(SOURCE))

CROSS_COMPILE=
CC = arm-hisiv500-linux-gcc
#CC = gcc
APP = onvif_server
LIBDIR=/home/dev/Documents/river/openssl-0.9.8x
#/usr/local/openssl/lib
INPATH=/home/dev/Documents/river/openssl-0.9.8x/include
#/usr/local/openssl/include/
INPATH2 = /opt/Hi3531_SDK_V1.0.7.1/osdrv/kernel/linux-3.0.y/include/
#CFLAGS+=  -ffunction-sections -fdata-sections  -DWITH_DOM  -DWITH_OPENSSL -DWITH_NO_C_LOCALE  -I $(INPATH) -I $(INPATH2) -DDEBUG
CFLAGS+=  -ffunction-sections -fdata-sections  -DWITH_DOM  -DWITH_OPENSSL -DWITH_NO_C_LOCALE  -I $(INPATH) -I $(INPATH2) 
#MY_CFLAGS+=-Os  -DWITH_DOM  -DWITH_OPENSSL  -L$(LIBDIR) -lssl -L$(LIBDIR) -lcrypto  -lm -lpthread 
MY_CFLAGS+=  -DWITH_DOM  -DWITH_OPENSSL  -lm -lpthread -ldl -Wl,--gc-sections
LDFLAGS += -lm

all:$(APP)
$(APP):$(OBJS)
	$(CC)  $(MY_CFLAGS) -o $@ $^  $(LIBDIR)/libssl.a  $(LIBDIR)/libcrypto.a 
	arm-hisiv500-linux-strip  $(APP)
	cp -rf $(APP) /nfs/yb
	
.PHONY:clean
clean:
	rm -f *.o *.d $(APP)	


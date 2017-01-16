#����Ŀ¼����


RELEASE_DIR_PATH = $(PROJECT_DIR_PATH)/release
LIBRARY_DIR_PATH  = $(PROJECT_DIR_PATH)/library
INCLUDE_DIR_PATH = $(PROJECT_DIR_PATH)/include



#�������ѡ��  -D_GNU_SOURCE  �ǽ��ȱ��pthread_rwlock_t����
LDFLAGS = -lpthread -lm -lstdc++ -ldl -lrt
#CFLAGS = -Wall -Werror -g -D_GNU_SOURCE -march=armv4t  #ARM9
#CFLAGS = -Wall -Werror -g -D_GNU_SOURCE -march=armv7-a -mcpu=cortex-a9 -mfloat-abi=softfp -mfpu=vfpv3-d16 #A9
#CFLAGS = -Wall -Werror -g -D_GNU_SOURCE 
CFLAGS = -Wall -g -O2 -D_GNU_SOURCE


#���������
ifeq ($(CROSS_COMPILER_X86),y)
	CC = gcc
	AR = ar
endif

ifeq ($(CROSS_COMPILER_ARM_HI),y)
#	CC = arm-hismall-linux-gcc  #ARM9
#	AR = arm-hismall-linux-ar
#	STRIP = arm-hismall-linux-strip
	CC = arm-hisiv100nptl-linux-gcc #A9�ں�
	AR = arm-hisiv100nptl-linux-ar
	STRIP = arm-hisiv100nptl-linux-strip
endif

ifeq ($(CROSS_COMPILER_ARM_TI),y)
	CC = arm-none-linux-gnueabi-gcc
	AR = arm-none-linux-gnueabi-ar
	STRIP = arm-none-linux-gnueabi-strip
endif



#��̬��
ifeq ($(USING_GENERAL_LIB),y)
INCLUDE_DIR += -I $(INCLUDE_DIR_PATH)/general
STATIC_LIB_DIR += $(LIBRARY_DIR_PATH)/lib_general.a 
endif

ifeq ($(USING_CRYPTO_LIB),y)
INCLUDE_DIR += -I $(INCLUDE_DIR_PATH)/crypto
STATIC_LIB_DIR += $(LIBRARY_DIR_PATH)/lib_crypto.a 
endif

ifeq ($(USING_EVENTLOOP_LIB),y)
INCLUDE_DIR += -I $(INCLUDE_DIR_PATH)/eventloop
STATIC_LIB_DIR += $(LIBRARY_DIR_PATH)/lib_eventloop.a 
endif

ifeq ($(USING_ZMALLOC_LIB),y)
INCLUDE_DIR += -I $(INCLUDE_DIR_PATH)/zmalloc
STATIC_LIB_DIR += $(LIBRARY_DIR_PATH)/lib_zmalloc.a 
endif

ifeq ($(USING_MTDUTILS_LIB),y)
INCLUDE_DIR += -I $(INCLUDE_DIR_PATH)/mtd-utils
STATIC_LIB_DIR += $(LIBRARY_DIR_PATH)/libmtd.a 
endif

ifeq ($(USING_SQLITE3_LIB),y)
INCLUDE_DIR += -I $(INCLUDE_DIR_PATH)/sqlite3
STATIC_LIB_DIR += $(LIBRARY_DIR_PATH)/libsqlite3.a 
endif

INCLUDE_DIR += -I $(INCLUDE_DIR_PATH)/libusb-1.0
STATIC_LIB_DIR += $(LIBRARY_DIR_PATH)/libusb-1.0.a 


#���ӿ�����
LDFLAGS += $(STATIC_LIB_DIR)
CFLAGS += $(INCLUDE_DIR)




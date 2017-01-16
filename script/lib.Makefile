#����ͷ�ļ�
include $(PROJECT_DIR_PATH)/script/inc.Makefile


#Ŀ���Զ�����
SRCS = $(wildcard *.c)
OBJS = $(SRCS:%.c=%.o)

#������
LIB_NAME = lib_$(TARGET).a

#ͷ�ļ�
LIB_INC = lib_$(TARGET).h


#�����
all: $(OBJS) lib

$(OBJS): %.o:%.c
	$(CC) $(CFLAGS) -c -o $@ $< 

lib: $(OBJS)
	$(AR) rcu $(LIB_NAME) $(OBJS) 

clean:
	@rm -f *.o
	@rm -f $(OBJS)
	@rm -f $(LIB_NAME)

install:	
	@cp -rf $(LIB_NAME)	$(LIBRARY_DIR_PATH)
	@cp -rf $(LIB_INC) $(INCLUDE_DIR_PATH)/$(TARGET)
	
	

.PHONY: all clean install



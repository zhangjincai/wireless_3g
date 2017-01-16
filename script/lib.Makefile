#包含头文件
include $(PROJECT_DIR_PATH)/script/inc.Makefile


#目标自动依赖
SRCS = $(wildcard *.c)
OBJS = $(SRCS:%.c=%.o)

#库名称
LIB_NAME = lib_$(TARGET).a

#头文件
LIB_INC = lib_$(TARGET).h


#编译库
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



#包含头文件
include $(PROJECT_DIR_PATH)/script/inc.Makefile

#目标自动依赖
SRCS = $(wildcard *.c)
OBJS = $(SRCS:%.c=%.o)

#执行文件
BINS = $(TARGET)

all: $(OBJS) $(BINS)


$(OBJS): %.o:%.c
	$(CC) $(CFLAGS) -c -o $@ $< 

$(BINS): $(OBJS)
	echo $(LDFLAGS)
	@$(CC) -o $@ $^ $(LDFLAGS)
	@$(STRIP) $(BINS)
	
clean:
	@rm -f $(BINS) $(OBJS)

install:
	@cp -rf $(BINS) $(RELEASE_DIR_PATH)


.PHONY: all clean install



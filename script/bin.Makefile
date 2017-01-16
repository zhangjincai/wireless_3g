#����ͷ�ļ�
include $(PROJECT_DIR_PATH)/script/inc.Makefile

#Ŀ���Զ�����
SRCS = $(wildcard *.c)
OBJS = $(SRCS:%.c=%.o)

#ִ���ļ�
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



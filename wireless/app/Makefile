TARGET = wireless

CC = arm-none-linux-gnueabi-gcc
AR = arm-none-linux-gnueabi-ar
STRIP = arm-none-linux-gnueabi-strip


LDFLAGS = -lpthread -lm -lstdc++ -ldl -lrt
CFLAGS = -Wall -g -O2 -D_GNU_SOURCE


LDFLAGS += lib_logdb.a lib_general.a lib_eventloop.a


SRCS = $(wildcard *.c)
OBJS = $(SRCS:%.c=%.o)


BINS = $(TARGET)

all: $(OBJS) $(BINS)


$(OBJS): %.o:%.c
	$(CC) $(CFLAGS) -c -o $@ $< 

$(BINS): $(OBJS)
	@$(CC) -o $@	$^ $(LDFLAGS)

#copy wireless to firmware dir
	cp wireless /mysoftware/BicycleProjectForEngineer_161103/firmware/ -r

clean:
	@rm -f $(BINS) $(OBJS)






.PHONY: all clean



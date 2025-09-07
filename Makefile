CC = gcc
CFLAGS =  -std=c99 -Os -s
LDFLAGS = 
LDFLAGS_PTHREAD = -DMULTITHREAD_ON

ifdef OS
	ifeq ($(OS), Windows_NT) # On windows
		LDFLAGS += -lws2_32 -lpthread
	endif
else
		LDFLAGS_PTHREAD += -lpthread
endif

SRCS = tinyc.c
TARGET = tinyc

.PHONY: all clean

all:
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS) $(LDFLAGS_PTHREAD)

single_thread:
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)_single_thread $(LDFLAGS)

debug:
	$(CC) $(CFLAGS) -g $(SRCS) -o $(TARGET) $(LDFLAGS) $(LDFLAGS_PTHREAD)

clean:
	rm -f $(TARGET)
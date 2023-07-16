CC = gcc
CFLAGS = -Wall -O3 
LDFLAGS = 
LDFLAGS_PTHREAD = -DMULTITHREAD_ON

ifdef OS
    ifeq ($(OS), Windows_NT) # On windows
        LDFLAGS += -lws2_32
    endif
else
        LDFLAGS_PTHREAD += -lpthread
endif

SRCS = tinyc.c
TARGET = tinyc

.PHONY: all clean

mono:
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)_monothread $(LDFLAGS)

multithread:
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)_multithread $(LDFLAGS) $(LDFLAGS_PTHREAD)

clean:
	rm -f $(TARGET)
CC = gcc
CFLAGS = -Wall
LDFLAGS = 

ifdef OS
    ifeq ($(OS), Windows_NT) # Windows
        LDFLAGS += -lws2_32
    endif
else
    UNAME_S := $(shell uname -s) # Linux
    ifeq ($(UNAME_S), Linux)
        LDFLAGS += 
    endif
endif

SRCS = tinyc.c
OBJS = $(SRCS:.c=.o)
TARGET = tinyc

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

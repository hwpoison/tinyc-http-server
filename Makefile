CC = gcc
CFLAGS = -Wall
LDFLAGS = -lws2_32

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
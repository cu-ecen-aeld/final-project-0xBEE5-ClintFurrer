CC = $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Wextra -O2 -I. -Iinclude
LDFLAGS ?= -lcjson

TARGET = user_int
SRCS = user_interface.c
HDRS = user_interface.h
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

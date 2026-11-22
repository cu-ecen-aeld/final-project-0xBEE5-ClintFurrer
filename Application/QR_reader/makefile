CC = $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Wextra -O2 -I. -Iinclude
LDFLAGS ?=

TARGET = qr_read
SRCS = qr_reader_app.c
HDRS = qr_reader_app.h
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

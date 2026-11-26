CC = $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Wextra -O2 -I. -Iinclude
LDFLAGS ?= -lcjson

TARGET = user_int
SRCS = user_interface.c QR_reader/qr_reader_app.c
HDRS = user_interface.h QR_reader/qr_reader_app.h
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

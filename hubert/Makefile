CC 		= gcc
CFLAGS 	= -Wall -std=c99 -Iinclude -D_GNU_SOURCE
LFLAGS	= -pthread
TARGET 	= hubert
OBJS 	= $(patsubst %.c, %.o, $(wildcard src/*.c))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(LFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $^ -c -o $@

.PHONY: clean

clean:
	rm -f $(OBJS) $(TARGET)

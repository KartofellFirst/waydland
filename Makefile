CC = gcc
CFLAGS = -Wall -Wextra -O2 $(shell pkg-config --cflags gtk4 gtk4-layer-shell-0)
LIBS = $(shell pkg-config --libs gtk4 gtk4-layer-shell-0)
TARGET = waydland
SRCS = src/main.c src/island.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LIBS)

clean:
	rm -f $(TARGET)

install:
	cp $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(TARGET)

.PHONY: all clean install uninstall

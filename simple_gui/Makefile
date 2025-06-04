CC = gcc
CFLAGS = -Wall -g -std=c99
RAYLIB_PATH = /usr/local/lib
RAYLIB_INCLUDE_PATH = /usr/local/include
LDLIBS = -lraylib -lm -lpthread -ldl

TARGET = main

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -I$(RAYLIB_INCLUDE_PATH) main.c -o $(TARGET) $(LDLIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

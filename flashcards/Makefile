CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pedantic -g
# Add include path for raylib if it's not in a standard system location, e.g., -I/usr/local/include

LDFLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
# Add library path for raylib if it's not in a standard system location, e.g., -L/usr/local/lib

TARGET = flashcards
SRC = flashcards.c
OBJ = $(SRC:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

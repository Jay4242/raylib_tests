CC = gcc
CFLAGS = -Wall -Wextra -g
LIBS = -lcurl -ljansson

guess_llama: guess_llama.c
	$(CC) $(CFLAGS) guess_llama.c -o guess_llama $(LIBS)

clean:
	rm -f guess_llama

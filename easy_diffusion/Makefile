CC = gcc
CFLAGS = -Wall -Wextra -g

easy_diffusion: easy_diffusion.c
	$(CC) $(CFLAGS) -o easy_diffusion easy_diffusion.c -ljansson -lcurl -lraylib

clean:
	rm -f easy_diffusion

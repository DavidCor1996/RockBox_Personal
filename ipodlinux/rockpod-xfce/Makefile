CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Os

all: rockpod-xfce-shell

rockpod-xfce-shell: rockpod-xfce-shell.c
	$(CC) $(CFLAGS) -o $@ rockpod-xfce-shell.c

preview: rockpod-xfce-shell
	./rockpod-xfce-shell --preview-ppm preview.ppm

clean:
	rm -f rockpod-xfce-shell preview.ppm

.PHONY: all preview clean

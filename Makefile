CFLAGS := $(shell pkg-config --libs gtk+-3.0 --cflags gtk+-3.0) -std=gnu99 -g
bubble: bubble.c
	gcc -o bubble bubble.c $(CFLAGS)

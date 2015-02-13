.PHONY: all clean

CFLAGS=-Wall

all: pt1230

interactive: interactive.c

pt1230: pt1230.c

clean:
	-rm interactive
	-rm pt1230

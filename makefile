.PHONY: all clean

CFLAGS=-Wall -g

all: pt1230

interactive: interactive.c

pt1230: pt1230.c

textlabel: textlabel.c
	$(CC) $(CFLAGS) -I/usr/include/freetype2 -lfreetype -lfontconfig -o $@ $<

clean:
	-rm interactive
	-rm pt1230
	-rm textlabel

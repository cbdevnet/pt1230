.PHONY: all clean

CFLAGS=-Wall -g

all: pt1230

interactive: interactive.c

pt1230: pt1230.c

textlabel: textlabel.c
	$(CC) $(CFLAGS) -I/usr/include/freetype2 -lfreetype -lfontconfig -o $@ $<

line2bitmap: line2bitmap.c

clean:
	-rm interactive
	-rm pt1230
	-rm textlabel
	-rm line2bitmap

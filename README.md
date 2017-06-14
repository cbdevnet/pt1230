# pt1230

This project provides an easy-to-use, lightweight interface to the Brother PT-1230PC label printer (and possibly other, 
protocol-compatible printers).

## Quick start

Create an image you want to print with the following parameters

* **Colors**: 2 (Monochrome)
* **Width**: 64 pixels
* **Height**: However long you want the label to be

Note that it is probably easier to design on a rotated frame and then rotate it to fit these parameters.

See below for some ideas on software to do this with.

Save as ASCII bitmap (or convert to that, see below).

Run `./pt1230` to see whether your printer was detected successfully.

Run `./pt1230 -b input.file` to print a label.

## Building

The main printer interface `pt1230`, the `line2bitmap` tool and the interactive harness have no dependencies
other than standard system headers, a GNU makefile is included.

Simply running make while running a system with a working C compiler should do the trick.

For the `textlabel` tool, the following development packages are required (listed for debian)

* libfreetype6-dev
* libfontconfig1-dev

`make` builds the main interface binary, the textlabel tool and line2bitmap.

## Details

### Image Format

Raster images need to be supplied as 64 pixels wide monochrome images in ASCII bitmap format,
meaning consecutive lines of 64 0/1 characters.
The linemap format is similarly defined as consecutive 0/1 characters representing white/black bars, respectively.

### Interface usage

The main application of this project is the `pt1230` binary, presenting a convenient interface to the printer
for bitmap/line data. Input data can either be piped in via stdin, or read from a file by supplying the
option mentioned below.
	
Options accepted by the interface are as follows

|Option		| Description							|
|---------------|---------------------------------------------------------------|
|`-h`		| Print a short help text					|
|`-d <device>`	| Override device node location (default: `/dev/usb/lp0`)	|
|`-f <file>`	| Set input file location (default: `stdin`)			|
|`-v <verbosity>`| Set output verbosity (0 - 4, default: 1 (Info))		|
|`-c`		| Chain print mode (default: off)				|
|`-m`		| Print delimiter/cut mark between labels (default: off)	|

Interface operation modes are

`-s`:	Status query mode (default)
`-b`:	Bitmap mode (see Image data format)
`-l`:	Linemap mode (see Image data format)

### Interactive harness usage

The interactive harness tool was mainly used to aid in reverse-engineering the printer protocol.
The code is messy and incomplete, but it can do some things. Upon starting, it opens the device and waits for input
on stdin. 

Valid commands are

| Key 	| Action 					|
|-------|-----------------------------------------------|
|q	| Close device and exit				|
|i	| Check for data to be read from device		|
|c	| Send "clear print buffer" command		|
|s	| Send "status request" command			|
|r	| Send "switch to raster mode" command		|
|z	| Send white raster line shorthand		|
|l	| Send black raster line			|
|x	| Send stripe raster line a (used for testing)	|
|y	| Send stripe raster line b (used for testing)	|
|f	| Send "print and feed" command			|
|p	| Send "print" command				|
|b	| Enable/disable bitmap mode			|
|1	| Send black pixel in bitmap mode		|
|0	| Send white pixel in bitmap mode		|
|newline| Transmit raster line in bitmap mode		|

### `textlabel` usage

`textlabel` accepts text as command line arguments and renders it into the bitmap format expected by the main
interface, allowing quick creation of text labels. This tool was mainly written as an exercise to explore the
FontConfig and FreeType APIs (though also to be able to create labels more easily), so it might work or it might not.
In most cases, it should. Building `textlabel` requires fontconfig as well as freetype development files. 

Recognized options are

| Option		| Description 		|	
|-----------------------|-----------------------|
| `--font <fontspec>`	| Set font		|
| `--width <width>`	| Set width		|
| `--`			| Stop option parsing	|

### `line2bitmap` usage

The `line2bitmap` tool can be used to create bitmap format images from linemap format barcodes, for example for
compositing a label from a barcode and text.

Recognized options are

| Option		| Description 			|	
|-----------------------|-------------------------------|
| `--height <height>`	| Barcode height in pixels	|
| `--width <width>`	| Single bar width in pixels	|

# Other helpful tools

`bincodes` (https://github.com/jduepmeier/bincodes/) enables you to create barcode data fit for simply piping into the interface's
linemap setting.

`bitmap` (A standard X11 application, package x11-apps in Debian) and its helper application `bmtoa` can be used for quickly creating
bitmaps fit to be used with the bitmap mode of the interface.

The GIMP has the capabilities to export it's projects as X Bitmap Files (xbm), which can also be read by `bmtoa` and thus used as
input to the interface.

# Protocol documentation

The protocol used in this interface has been reverse-engineered by reading software written by other people as well as more-or-less 
official specification documents and wiki pages. See the "References" section for links to those resources.

The device sports a switch on its back side, offering a choice between "EL" (Editor Light) and "E" modes.

EL seems to enable a mass storage medium containing windows binaries for the manufacturers interfacing tools, while
the "E" position, at least under Linux, simply presents a USB Line printer interface to the system.

Communication is bi-directional, with the printer always returning a full status report structure (32 Bytes).

Initialization is performed by requesting the printer clear the print buffer
```
Host=>Printer | 1B 40
```

After which by common agreement, a status request is sent (this might not be required for operation)
```
Host=>Printer | 1B 69 53
```

The printer now sends 32 bytes of status data, which may be interpreted in order to find the media width, current
printer phase, etc. Refer to the references section to find links explaining the status descriptor more in-depth
```
Printer=>Host | 32 Bytes Status descriptor
```

The printer should now be set to raster graphics mode, in order to send sequential raster lines to be printed
```
Host=>Printer | 1B 69 52 01
```

The printer is now ready to accept sequential lines of bitmapped data, special line commands or printing commands.

Bitmap raster lines consist of a header
```
Host=>Printer | 47 $a $b
```

followed by n data bytes, with n = ($a+256\*$b). Another way of looking at this would be that the data length is encoded as 
16bit unsigned integer in little endian notation. Since the print head in the 1230PC can only print 64 bits/pixels per
raster line, $b can always be 0 (as this printer will never need more than 256 data bytes for any one raster line). However, in 
order to support larger media widths, there is a padding at the beginning of the data section, which (according to a more-or-less 
official spec document) must be set to 0 or "damage to the print head might ensue". The padding for 12mm media spans 4 bytes
```
Host=>Printer | 00 00 00 00
```

after which 8 printable data bytes are sent, for a total of 12 bytes. Therefore, $a can be set to 0x0C for printing with 12mm media.
The data bytes are mapped bit-by-bit to pixels, left-to-right mapping to MSB-to-LSB. No compression is performed, although most documents
mention RLE/TIFF compression. To print an all-black line on 12mm media would therefore end the raster line transfer with
```
Host=>Printer | FF FF FF FF FF FF FF FF
```

An empty line can be printed by sending
```
Host=>Printer | 5A
```
instead of the full raster line structure

The printer buffers the raster data internally (up to 30cm of data, according to some documents), indicating action by turning off or
blinking the activity light. In order to print the current data buffer, a print-and-feed command can be sent
```
Host=>Printer | 1A
```
this prints the buffer and thereafter advances the tape to a point where it can be safely cut, exposing the printed area.

In order to chain-print, a simple "print" command can be sent, which only advances the tape a minimal amount, but still allows new
raster data to be transferred.
```
Host=>Printer | 0C
```

References
----------
Special thanks to Bernard Hatt, who wrote a similar tool, which was a great help in understanding the printer protocol
(but please think a bit more about variable naming).
 => http://forums.openprinting.org/read.php?24,11091

The Undocumented Printing Wiki has some information about the protocol for various P-Touch printers.
 => http://www.undocprint.org/formats/page_description_languages/brother_p-touch

Another application supporting this printer is blabel, which presents a graphical interface for label printing.
 => http://apz.fi/blabel/

The Brother PT-9500PC Command reference document seems not to be distributed by Brother anymore, but digital copies of it
can be found by querying your favourite search engine.

Sources & Feedback
------------------
This projects home lies at https://github.com/cbdevnet/pt1230/. Should this page ever not be available anymore, the new location
will be announced somewhere on http://dev.cbcdn.com/

Feature requests, bug reports and general Feedback is welcome and accepted via email to cb@cbcdn.com or via the Github Issue tracking
system.

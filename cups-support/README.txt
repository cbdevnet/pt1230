This folder contains some files that make the interface work
rudimentarily with CUPS. This needs a lot of manual configuration,
is limited in various ways (eg, supporting only 10 discrete label 
lengths, more can be configured in the PPD) and is generally not 
that stable. If thats ok for you, this works fine.

SETUP:
	Install pt1230 (the interface) to the PATH (eg. /usr/bin)
	
	Find your printers device node (usually /dev/usb/lp*)
	 Hint: pt1230 -d DEVICENODE should print a status report 
	
	Install the printer backend script pt1230-backend by copying
	 it to /usr/lib/cups/backends/pt1230

	If your device node is not /dev/usb/lp0, edit the backend
	 script to reflect that
	
	lpinfo -v should now include a listing like "direct pt1230"

	Create the printer by runnning
	 lpadmin -p MyLabelmaker -P pt1230.ppd -L "Location" -v pt1230

	Enable printing by running
		cupsenable MyLabelmaker
		cupsaccept MyLabelmaker

	You should now be able to print labels. Layout them in landscape,
	 select a fitting label length in the print options and print them.

	Layout works best when working at native resolution (64 pixels height)

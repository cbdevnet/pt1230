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
	 it to /usr/lib/cups/backends/pt1230 (linking is not sufficient)
	 Some distributions might instead use a path like
	 /usr/libexec/cups/backends
	
	lpinfo -v should now include a listing like "direct pt1230"
	 Hint: lpinfo, lpadmin and the cups* commands can only be run as root

	You can now continue by adding the printer via the web interface.
	 The web interface will ask you about the device URI/connection string
	 at some point. This should be pt1230:DEVICENODE, e.g. pt1230:/dev/usb/lp1
	 When asked for make/model info, choose to upload a PPD file and select
	 the one in this folder.

	Alternatively, do the following in a shell

	Create the printer by runnning
	 lpadmin -p MyLabelmaker -P pt1230.ppd -L "Location" -v pt1230:DEVICENODE
	 Hint: Remember to substitute DEVICENODE with your actual device node

	Enable printing by running
		cupsenable MyLabelmaker
		cupsaccept MyLabelmaker



	You should now be able to print labels. Layout them in landscape,
	 select a fitting label length in the print options and print them.

	Layout works best when working at native resolution (64 pixels height)

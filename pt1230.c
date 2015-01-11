#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <fcntl.h>

#include "pt1230.h"

int usage(char* fn){
	printf("PT1230 Interface\n");
	printf("Valid arguments:\n");
	printf("\t-h\t\tPrint this text\n");
	printf("\t-d <devicenode>\tSet output device node (Default: %s)\n", DEFAULT_DEVICENODE);
	printf("\t-f <inputfile>\tSpecify input file (Default: read from stdin)\n");
	printf("\t-v\t\tIncrease verbosity\n");
	printf("\t-s\t\tQuery printer status (default)\n");
	printf("\t-b\t\tBitmap mode\n");
	printf("\t-l\t\tLinemap mode\n");
	printf("\t-c\t\tChain print (do not feed after printing)\n");
	return 1;
}

void debug(unsigned severity, unsigned current_level, char* fmt, ...){
	va_list args;
	va_start(args, fmt);
	if(severity<=current_level){
		vfprintf(stderr, fmt, args);
	}
	va_end(args);
}

int parse_arguments(CONF* cfg, int argc, char** argv){
	unsigned i, v;
	char* device=NULL;
	char* input=NULL;

	for(i=0;i<argc;i++){
		if(argv[i][0]=='-'){
			switch(argv[i][1]){
				case 'h':
					return -1;
				case 'd':
					device=argv[++i];
					break;
				case 'f':
					input=argv[++i];
					break;
				case 'v':
					for(v=1;argv[i][v]=='v';v++){
					}
					cfg->verbosity=v;
					break;
				case 's':
					cfg->mode=MODE_QUERY;
					break;
				case 'b':
					cfg->mode=MODE_BITMAP;
					break;
				case 'l':
					cfg->mode=MODE_LINEMAP;
					break;
				case 'c':
					cfg->chain_print=true;
					break;
				default:
					debug(LOG_ERROR, cfg->verbosity, "Unknown command line argument %s\n", argv[i]);
					return -1;
			}
		}
		else{
			debug(LOG_ERROR, cfg->verbosity, "Unknown command line argument %s\n", argv[i]);
			return -1;
		}
	}

	//Open output device
	if(!device){
		device=DEFAULT_DEVICENODE;
	}
	debug(LOG_INFO, cfg->verbosity, "Opening device at %s\n", device);
	cfg->device_fd=open(device, O_RDWR);
	if(cfg->device_fd<0){
		debug(LOG_ERROR, cfg->verbosity, "Failed to open printer device node\n");
		return -1;
	}

	//Open input data source
	if(input){
		//Open file
		debug(LOG_INFO, cfg->verbosity, "Opening input file at %s\n", input);
		cfg->input_fd=open(input, O_RDONLY);
	}
	else{
		cfg->input_fd=fileno(stdin);
	}

	if(cfg->input_fd<0){
		debug(LOG_ERROR, cfg->verbosity, "Failed to open input data source\n");
		return -1;
	}

	return 0;
}

int main(int argc, char** argv){
	CONF cfg={
		0,		//verbosity
		-1,		//device fd
		-1,		//input fd
		false,		//chain print
		MODE_QUERY	//mode
	};

	//parse arguments
	if(parse_arguments(&cfg, argc-1, argv+1)<0){
		exit(usage(argv[0]));
	}
	
	debug(LOG_DEBUG, cfg.verbosity, "Status structure size %d\n", sizeof(STATUS));
	
	//TODO clean up
	return 0;
}

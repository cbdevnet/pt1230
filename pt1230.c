#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>

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

int fetch_status(int fd, unsigned attempts, unsigned timeout, size_t buffer_length, char* buffer){
	unsigned run;
	ssize_t bytes;

	if(!buffer||buffer_length<sizeof(PROTO_STATUS)*4){
		return -1;
	}

	for(run=0;run<attempts;run++){
		if(run>0){
			usleep(timeout);
		}

		bytes=read(fd, buffer, buffer_length);

		if(bytes<0){
			perror("device/read");
			return -1;
		}

		if(bytes==0){
			continue;
		}

		//the device only ever should send full PROTO_STATUS reports
		//that is, according to some document.
		if(bytes%sizeof(PROTO_STATUS)!=0){
			debug(LOG_ERROR, 0, "Invalid response from device\n");
			return -1;
		}

		return bytes/sizeof(PROTO_STATUS);
	}
	return 0;
}

int print_status(unsigned count, PROTO_STATUS* data){
	unsigned i, j;

	for(i=0;i<count;i++){
		printf("Magic ");
		for(j=0;j<sizeof(data[i].magic);j++){
			printf("%02X ", data[i].magic[j]);
		}
		printf("\nMedia width: %d\n", data[i].media_width);
		printf("Media type: %d\n", data[i].media_type);
		printf("Media length: %d\n", data[i].media_length);
		printf("Error descriptors %02X %02X\n", data[i].error1, data[i].error2);
		printf("Status %02X\n", data[i].status);
		printf("Phase %02X High %02X Low %02X\n", data[i].phase, data[i].phase_high, data[i].phase_low);
		printf("Notification %02X\n", data[i].notification);
	}

	return 0;
}

int send_command(int fd, size_t length, char* buffer){
	ssize_t bytes;

	bytes=write(fd, buffer, length);

	if(bytes<0){
		perror("device/write");
		return -1;
	}

	if(bytes!=length){
		debug(LOG_ERROR, 0, "Incomplete write on device\n");
		return -1;
	}

	return 0;
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
	int count;
	char device_buffer[(DEVICE_BUFFER_LENGTH*sizeof(PROTO_STATUS))];

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
	
	debug(LOG_DEBUG, cfg.verbosity, "Status structure size %d\n", sizeof(PROTO_STATUS));
	debug(LOG_DEBUG, cfg.verbosity, "Init sentence length %d\n", sizeof(PROTO_INIT));

	//fetch device_fd data for leftovers
	if(fetch_status(cfg.device_fd, DEFAULT_ATTEMPTS, DEFAULT_TIMEOUT, sizeof(device_buffer), device_buffer)<0){
		return -1;
	}

	//send init command
	if(send_command(cfg.device_fd, sizeof(PROTO_INIT)-1, PROTO_INIT)<0){
		return -1;
	}

	//send status request
	if(send_command(cfg.device_fd, sizeof(PROTO_STATUS_REQUEST)-1, PROTO_STATUS_REQUEST)<0){
		return -1;
	}

	//wait for status response
	count=fetch_status(cfg.device_fd, DEFAULT_ATTEMPTS, DEFAULT_TIMEOUT, sizeof(device_buffer), device_buffer);
	if(count<0){
		return -1;
	}
	if(count==0){
		debug(LOG_WARNING, cfg.verbosity, "Received no status data, continuing anyway...\n");
	}
	debug(LOG_DEBUG, cfg.verbosity, "Received %d status response structures\n", count);

	if(cfg.mode==MODE_QUERY||cfg.verbosity>0){
		//dump status record
		print_status(count, (PROTO_STATUS*)device_buffer);
	}
	
	if(cfg.mode!=MODE_QUERY){
		//TODO Read input data
	}

	//clean up
	close(cfg.device_fd);
	close(cfg.input_fd);
	return 0;
}

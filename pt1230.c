#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

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
	printf("\t-m\t\tPrint cut marker after label\n");
	//TODO invert flag?
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

int fetch_status(int fd, int attempts, unsigned timeout, size_t buffer_length, char* buffer){
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
					cfg->verbosity=v-1;
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
				case 'm':
					cfg->print_marker=true;
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
	cfg->device_fd=open(device, O_RDWR | O_SYNC);
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

int process_data(CONF* cfg){
	char data_buffer[DATA_BUFFER_LENGTH];
	uint8_t line_buffer[8], current_bit=0, current_byte=0;
	ssize_t bytes;
	unsigned i;

	memset(line_buffer, 0, sizeof(line_buffer));

	do{
		//read data from input
		bytes=read(cfg->input_fd, data_buffer, sizeof(data_buffer)-1);

		if(bytes<0){
			//escape from loop
			break;
		}

		data_buffer[bytes]=0;
		debug(LOG_DEBUG, cfg->verbosity, "Current data buffer: %s\n", data_buffer);

		//process
		for(i=0;i<bytes;i++){
			switch(cfg->mode){
				case MODE_BITMAP:
					switch(data_buffer[i]){
						case '1':
							line_buffer[7-current_byte]|=(0x01<<current_bit);
						case '0':
							current_bit++;
							current_bit%=8;
							if(current_bit==0){
								current_byte++;
								current_byte%=8;
							}
							break;
						case '\n':
							debug(LOG_DEBUG, cfg->verbosity, "Sending bitmap raster line\n");
							if(send_command(cfg->device_fd, sizeof(PROTO_RASTERLINE)-1, PROTO_RASTERLINE)<0){
								return -1;
							}
							if(send_command(cfg->device_fd, sizeof(line_buffer), (char*)line_buffer)<0){
								return -1;
							}

							memset(line_buffer, 0, sizeof(line_buffer));
							
							//not really necessary, prevents some dumb paths though
							current_bit=0;
							current_byte=0;
							break;
						default:
							debug(LOG_WARNING, cfg->verbosity, "Illegal character '%02X' in input stream, ignoring\n", (unsigned char)data_buffer[i]);
					}
					break;
				case MODE_LINEMAP:
					switch(data_buffer[i]){
						case '0':
							debug(LOG_DEBUG, cfg->verbosity, "Sending white raster line\n");
							if(send_command(cfg->device_fd, sizeof(PROTO_RASTERLINE_WHITE)-1, PROTO_RASTERLINE_WHITE)<0){
								return -1;
							}
							break;
						case '1':
							debug(LOG_DEBUG, cfg->verbosity, "Sending black raster line\n");
							if(send_command(cfg->device_fd, sizeof(PROTO_RASTERLINE_BLACK)-1, PROTO_RASTERLINE_BLACK)<0){
								return -1;
							}
							break;
						default:
							debug(LOG_WARNING, cfg->verbosity, "Illegal character '%02X' in input stream, ignoring\n", (unsigned char)data_buffer[i]);
					}
					break;
				default:
					//FIXME tcc falls through here because of some bug
					debug(LOG_ERROR, cfg->verbosity, "Illegal branch, mode is %d, aborting\n", cfg->mode);
					return -1;
			}
			usleep(1000);
		}
	}
	while(bytes>0);
	if(bytes<0){
		perror("input/read");
		return -1;
	}
	if(cfg->print_marker){
		debug(LOG_DEBUG, cfg->verbosity, "Sending label delimiter\n");
		for(i=0;i<20;i++){
			if(send_command(cfg->device_fd, sizeof(PROTO_RASTERLINE_WHITE)-1, PROTO_RASTERLINE_WHITE)<0){
				return -1;
			}
		}
		if(send_command(cfg->device_fd, sizeof(PROTO_RASTERLINE_BLACK)-1, PROTO_RASTERLINE_BLACK)<0){
			return -1;
		}
	}
	return 0;
}

int main(int argc, char** argv){
	int count;
	unsigned i;
	char device_buffer[(DEVICE_BUFFER_LENGTH*sizeof(PROTO_STATUS))];
	bool print_ended=false;
	PROTO_STATUS* status=(PROTO_STATUS*)device_buffer;

	CONF cfg={
		.verbosity=0,
		.device_fd=-1,
		.input_fd=-1,
		.chain_print=false,
		.print_marker=false,
		.mode=MODE_QUERY
	};

	//parse arguments
	if(parse_arguments(&cfg, argc-1, argv+1)<0){
		exit(usage(argv[0]));
	}

	if(cfg.device_fd<0){
		debug(LOG_ERROR, cfg.verbosity, "Failed to access the printer\n");
		exit(usage(argv[0]));
	}
	
	debug(LOG_DEBUG, cfg.verbosity, "Status structure size %d\n", sizeof(PROTO_STATUS));
	debug(LOG_DEBUG, cfg.verbosity, "Init sentence length %d\n", sizeof(PROTO_INIT));
	debug(LOG_INFO, cfg.verbosity, "Verbosity %d\n", cfg.verbosity);

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
		print_status(count, status);
	}
	
	if(cfg.mode!=MODE_QUERY){
		//switch to raster graphics mode
		debug(LOG_INFO, cfg.verbosity, "Switching to raster graphics mode\n");
		if(send_command(cfg.device_fd, sizeof(PROTO_RASTER)-1, PROTO_RASTER)<0){
			return -1;
		}

		//handle input data
		debug(LOG_STATUS, cfg.verbosity, "Reading image data\n");
		if(process_data(&cfg)<0){
			return -1;
		}
		
		//flush printing buffer to tape
		debug(LOG_STATUS, cfg.verbosity, "Starting printer processing\n");
		if(cfg.chain_print){
			if(send_command(cfg.device_fd, sizeof(PROTO_PRINT)-1, PROTO_PRINT)<0){
				return -1;
			}
		}
		else{
			if(send_command(cfg.device_fd, sizeof(PROTO_PRINT_FEED)-1, PROTO_PRINT_FEED)<0){
				return -1;
			}
		}

		//wait until printer is done
		debug(LOG_STATUS, cfg.verbosity, "Waiting for printer to finish\n");
		
		while(!print_ended){
			count=fetch_status(cfg.device_fd, -1, 5000, sizeof(device_buffer), device_buffer);
			if(count<0){
				return -1;
			}
			
			if(count==0){
				debug(LOG_WARNING, cfg.verbosity, "Received no status data, printer may have encountered an error or still be printing\n");
				break;
			}
			else{
				debug(LOG_DEBUG, cfg.verbosity, "Received %d status response structures\n", count);
			}
		
			if(cfg.verbosity>=LOG_DEBUG){
				print_status(count, status);
			}

			for(i=0;i<count;i++){
				switch(status[i].status){
					case 0x00:
						//status request response
						//should not happen here, if it does anyway, ignore it
						break;
					case 0x01:
						debug(LOG_STATUS, cfg.verbosity, "Printing completed successfully\n");
						print_ended=true;
						break;
					case 0x02:
						debug(LOG_ERROR, cfg.verbosity, "Device reported an error, status dump follows\n");
						print_status(1, status+i);
						print_ended=true;
						break;
					case 0x06:
						debug(LOG_DEBUG, cfg.verbosity, "Phase change\n");
						break;
				}
			}
		}
	}

	//clean up
	close(cfg.device_fd);
	close(cfg.input_fd);
	return 0;
}

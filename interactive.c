#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>

void dump_buffer(unsigned length, char* data){
	unsigned i;
	for(i=0;i<length;i++){
		printf("%02d|%02X ", i, (unsigned char)data[i]);
		if(i>0&&((i+1)%16)==0){
			printf("\n");
		}
	}
	printf("\n");
}

int send_data(int fd, unsigned len, char* data){
	int rv;

	rv=write(fd, data, len);

	return rv;
}

int fetch_response(int fd, unsigned timeout, unsigned maxbytes, char* data){
	fd_set readfds;
	struct timeval tv;
	int rv;

	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);

	tv.tv_sec=(timeout/1000000);
	tv.tv_usec=(timeout%1000000);

	syncfs(fd);
	rv=select(fd+1, &readfds, NULL, NULL, &tv);

	if(rv<0){
		perror("select");
		return -1;
	}
	else if(rv==0){
		//nothing
		return 0;
	}
	else if(FD_ISSET(fd, &readfds)){
		//data ready, read it
		rv=read(fd, data, maxbytes);
		return rv;
	}

	//weird. somethings broken.
	fprintf(stderr, "select returned bs.\n");
	return -1;
}

int main(int argc, char** argv){
	int fd=-1;
	int rv;
	int oper;
	char buffer[1024];

	char bmode_buffer[8];
	bool bmode=false;
	unsigned bmode_byte=0;
	unsigned bmode_bit=0;

	memset(bmode_buffer, 0, 8);

	printf("PT1230 Interactive mode\n");

	//open device file
	fd=open("/dev/usb/lp0", O_RDWR |  O_SYNC);

	if(fd<0){
		printf("Failed to open device\n");
		exit(1);
	}

	do{
		oper=getchar();
		if(!bmode&&(oper=='\n'||oper=='\r')){
			continue;
		}

		switch(oper){
			case 'i':
				printf("Checking for incoming data.\n");
				break;
			case 'b':
				bmode=!bmode;
				if(!bmode){
					printf("Disabled bitmap mode, print with p/f\n");
				}
				else{
					printf("Enabled bitmap mode, enter lines of 64 0/1 ASCII bytes, terminate with b\n");
					memset(bmode_buffer, 0, 8);
				}
				break;
			case '1':
				bmode_buffer[(7-bmode_byte)]|=((0x01)<<(bmode_bit));
			case '0':
				if(bmode){
					bmode_bit++;
					bmode_bit%=8;
					if(bmode_bit==0){
						bmode_byte++;
						bmode_byte%=8;
					}
				}
				break;
			case '\n':
				if(bmode){
					printf("Sending bmode raster line\n");
					dump_buffer(8, bmode_buffer);
					rv=send_data(fd, 7, "G\x0C\0\0\0\0\0");
					printf("Sent %d bytes\n", rv);
					rv=send_data(fd, 8, bmode_buffer);
					printf("Sent %d bytes\n", rv);
					memset(bmode_buffer, 0, 8);
				}
				else{
					printf("Something went wrong.\n");
				}
				break;
			case 'c':
				printf("Clear buffer\n");
				rv=send_data(fd, 2, "\x1b@");
				printf("Sent %d bytes\n", rv);
				break;
			case 's':
				printf("Requesting status\n");
				rv=send_data(fd, 3, "\x1biS");
				printf("Sent %d bytes\n", rv);
				break;
			case 'r':
				printf("Switching device to raster mode\n");
				rv=send_data(fd, 4, "\x1biR\x01");
				printf("Sent %d bytes\n", rv);
				break;
			case 'z':
				printf("Sending blank raster line\n");
				rv=send_data(fd, 1, "Z");
				printf("Sent %d bytes\n", rv);
				break;
			case 'l':
				printf("Sending raster line\n");
				rv=send_data(fd, 15, "G\x0c\x00\0\0\0\0\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF");
				printf("Sent %d bytes\n", rv);
				break;
			case 'x':
				printf("Sending stripe raster line a\n");
				rv=send_data(fd, 15, "G\x0c\x00\0\0\0\0\x8A\xAA\xAA\xAA\xAA\xAA\xAA\xA1");
				printf("Sent %d bytes\n", rv);
				break;
			case 'y':
				printf("Sending stripe raster line b\n");
				rv=send_data(fd, 15, "G\x0c\x00\0\0\0\0\x85\x55\x55\x55\x55\x55\x55\x51");
				printf("Sent %d bytes\n", rv);
				break;
			case 'f':
				printf("Sending print and feed command\n");
				rv=send_data(fd, 1, "\x1a");
				printf("Sent %d bytes\n", rv);
				break;
			case 'p':
				printf("Sending print command\n");
				rv=send_data(fd, 1, "\x0c");
				printf("Sent %d bytes\n", rv);
				break;
		}

		rv=fetch_response(fd, 2000000, 1024, buffer);
		
		if(rv<0){
			printf("Response reading failed, aborting\n");
			oper='q';
		}
		else{
			if(rv>0){
				printf("Read %d bytes\n", rv);
				dump_buffer(rv, buffer);
			}
		}
	}
	while(oper!='q'&&oper!=EOF);

	printf("Closing down\n");
	close(fd);
}

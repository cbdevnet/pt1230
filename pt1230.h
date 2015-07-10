#define DEFAULT_DEVICENODE 	"/dev/usb/lp0"
#define DEFAULT_ATTEMPTS	5
#define DEFAULT_TIMEOUT		200
#define DEVICE_BUFFER_LENGTH	25
#define DATA_BUFFER_LENGTH	1024

#define PROTO_INIT 		"\x1B@"			//Clear data buffer
#define PROTO_STATUS_REQUEST	"\x1BiS"		//Request printer status
#define PROTO_RASTER 		"\x1BiR\x01"		//Switch to raster mode
#define PROTO_RASTERLINE_WHITE	"Z"			//All-white raster line (width-independent)
							//All-black raster line (12mm) FIXME
#define PROTO_RASTERLINE_BLACK	"G\x0C\x00\0\0\0\0\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
#define PROTO_RASTERLINE	"G\x0C\x00\0\0\0\0"	//Generic raster line header
#define PROTO_PRINT		"\x0C"			//Print current buffer
#define PROTO_PRINT_FEED	"\x1A"			//Print current buffer and feed for cutting

typedef struct __attribute__((__packed__)) /*_PT1230_STATUS*/ {
	uint8_t magic[8];
	uint8_t error1;
	uint8_t error2;
	uint8_t media_width;
	uint8_t media_type;
	uint8_t reserved1[5];
	uint8_t media_length;
	uint8_t status;
	uint8_t phase;
	uint8_t phase_high;
	uint8_t phase_low;
	uint8_t notification;
	uint8_t reserved2[9];
} PROTO_STATUS;

typedef enum /*_1230_MODE*/ {
	MODE_QUERY=0,
	MODE_BITMAP=1,
	MODE_LINEMAP=2
} MODE;

typedef struct /*_CONFIG*/ {
	unsigned verbosity;
	int device_fd;
	int input_fd;
	bool chain_print;
	bool print_marker;
	MODE mode;
} CONF;

#define LOG_ERROR 0
#define LOG_STATUS 0
#define LOG_WARNING 0
#define LOG_INFO 1
#define LOG_DEBUG 2

void debug(unsigned severity, unsigned current_level, char* fmt, ...);

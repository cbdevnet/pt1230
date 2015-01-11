#define DEFAULT_DEVICENODE "/dev/usb/lp0"

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
} STATUS;

typedef enum /*_1230_MODE*/ {
	MODE_QUERY,
	MODE_BITMAP,
	MODE_LINEMAP
} MODE;

typedef struct /*_CONFIG*/ {
	unsigned verbosity;
	int device_fd;
	int input_fd;
	bool chain_print;
	MODE mode;
} CONF;

#define LOG_ERROR 0
#define LOG_WARNING 1
#define LOG_INFO 2
#define LOG_DEBUG 3

void debug(unsigned severity, unsigned current_level, char* fmt, ...);

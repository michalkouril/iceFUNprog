/*
 *
 *  Copyright(C) 2018 Gerald Coe, Devantech Ltd <gerry@devantech.co.uk>
 *
 *  Permission to use, copy, modify, and/or distribute this software for any purpose with or
 *  without fee is hereby granted, provided that the above copyright notice and
 *  this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO
 *  THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
 *  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 *  DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 *  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 *  CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>


enum cmds
{
    AD0=0xa0, AD1, AD2, AD3, AD4,
    DONE = 0xb0, GET_VER, RESET_FPGA, ERASE_CHIP, ERASE_64k, PROG_PAGE, READ_PAGE, VERIFY_PAGE, GET_CDONE, RELEASE_FPGA,
    WRITE_READ_USART=0xc0, GET_SET_USART_CONFIG
    // UARTMODE=0xc0, UARTRX, UARTTX, UARTINFO
};

#define FLASHSIZE 1048576	// 1MByte because that is the size of the Flash chip
unsigned char FPGAbuf[FLASHSIZE];
unsigned char SerBuf[300];
char ProgName[30];
int fd;
char verify;
int rw_offset = 0;

static void help(const char *progname)
{
	fprintf(stderr, "Programming tool for Devantech iceFUN board.\n");
	fprintf(stderr, "Usage: %s [-P] <SerialPort>\n", progname);
	fprintf(stderr, "       %s <input file>\n", progname);
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -P <Serial Port>      use the specified USB device [default: /dev/ttyACM0]\n");
	fprintf(stderr, "  -h                    display this help and exit\n");
	fprintf(stderr, "  -o <offset in bytes>  start address for write [default: 0]\n");
	fprintf(stderr, "                        (append 'k' to the argument for size in kilobytes,\n");
	fprintf(stderr, "                         or 'M' for size in megabytes)\n");
	fprintf(stderr, "                         or prefix '0x' to the argument for size in hexdecimal\n");
	fprintf(stderr, "  -A <ADC number>       get ADC result. ADC number=1..4\n");
	fprintf(stderr, "  -I <val>              get UART reg info. If val > 1 set it as SPBRG. (25 ... 115,200b)\n");
	fprintf(stderr, "  -R <val>              Send <val> to FPGA over USART and [optionally] wait for reponse\n");
	fprintf(stderr, "  --help\n");
	fprintf(stderr, "  -v                    skip verification\n");
	fprintf(stderr, "Example:\n");
	fprintf(stderr, "%s -P /dev/ttyACM1 blinky.bin\n", progname);
	fprintf(stderr, "\n");
	fprintf(stderr, "Exit status:\n");
	fprintf(stderr, "  0 on success,\n");
	fprintf(stderr, "  1 operation failed.\n");
	fprintf(stderr, "\n");
}

int GetVersion(void)								// print firmware version number
{
	SerBuf[0] = GET_VER;
	write(fd, SerBuf, 1);
//	tcdrain(fd);
	read(fd, SerBuf, 2);
	if(SerBuf[0] == 38) {
		fprintf(stderr, "iceFUN v%d, ",SerBuf[1]);
		return 0;
	}
	else {
		fprintf(stderr, "%s: Error getting Version\n", ProgName);
		return EXIT_FAILURE;
	}
}

int GetADC(int adc)
{
	switch(adc) {
	case 0: SerBuf[0] = AD0; break;
	case 1: SerBuf[0] = AD1; break;
	case 2: SerBuf[0] = AD2; break;
	case 3: SerBuf[0] = AD3; break;
	case 4: SerBuf[0] = AD4; break;
	default:
		fprintf(stderr, "%s: Invalid ADC number (%d)\n", ProgName, adc);
		return EXIT_FAILURE;
	}
	write(fd, SerBuf, 1);
	read(fd, SerBuf, 2);
	printf("AD%d: %d\n", adc, SerBuf[0]*256+SerBuf[1]);
	return 0;
}

void GetUARTInfo(int val)
{
	SerBuf[0] = GET_SET_USART_CONFIG;

  SerBuf[1] = 4;
  SerBuf[2] = val; // set SPBRG to 25 115,200b -- theoretically
  
	int y=write(fd, SerBuf, (val==1?1:3));

	// printf("GetUARTInfo sending (%d): len %d (%X ...)\n", y, 1, SerBuf[0]);
	int i=read(fd, SerBuf, 16); // ret, MODE, SPBRG, TXSIZE.., TXHEADL, TXHEADH, TXTAIL.., RXSIZE..., RXHEAD.., RXTAIL..
	printf("GetUartInfo: ret: %02x TXSTA: %02x RCSTA: %02x BAUDCON: %02x SPBRG: %d USB buffer size: %d USB timeout: %d UART timeout: %d\n", 
                    SerBuf[0], SerBuf[1], SerBuf[2], SerBuf[3], 
                    SerBuf[4]+SerBuf[5]*256, 
                    SerBuf[6]+SerBuf[7]*256, 
                    SerBuf[8]+SerBuf[9]*256, 
                    SerBuf[10]+SerBuf[11]*256 
                    );
}

void SendRecvUARTData(char *buf)
{
	uint8_t len=255;
	uint8_t send_len=strlen(buf);

	SerBuf[0] = WRITE_READ_USART;
	SerBuf[1] = send_len; 
	SerBuf[2] = len;
  strcpy((char*)(&(SerBuf[3])), buf);
  send_len += 3;

	int y=write(fd, SerBuf, send_len);
	// printf("SendRecvUARTData sending (%d): len %d (%X %X %X %X %X %X...)\n", y, len, 
  //     SerBuf[0], SerBuf[1], SerBuf[2], SerBuf[3], SerBuf[4], SerBuf[5]);

	int i=read(fd, SerBuf, len+2);
  // returns err -- 0..no error,1..timeout, 2..framing error, 3..overrun error
	printf("SendRecvUARTData(%d): len %d err: %d (%02X %02X %02X %02X %02X %02X ...)\n", i, SerBuf[0], SerBuf[1], 
                                            SerBuf[0], SerBuf[1], SerBuf[2], SerBuf[3], SerBuf[4], SerBuf[5]);

  for(int j=0;j<i;j++) {
    // printf("%02X ", SerBuf[j+2]);
    printf("%c", SerBuf[j+2]);
  }
  printf("\n");
	// len=SerBuf[0]+256*SerBuf[1];
	// for(int i=0;i<len;i++) printf("%X(%c) ", SerBuf[i+2], SerBuf[i+2]);
}

int resetFPGA(void)									// reset the FPGA and return the flash ID bytes
{
	SerBuf[0] = RESET_FPGA;
	write(fd, SerBuf, 1);
	read(fd, SerBuf, 3);
	fprintf(stderr, "Flash ID %02X %02X %02X\n",SerBuf[0], SerBuf[1], SerBuf[2]);
	return 0;
}

int releaseFPGA(void)								// run the FPGA
{
	SerBuf[0] = RELEASE_FPGA;
	write(fd, SerBuf, 1);
	read(fd, SerBuf, 1);
	fprintf(stderr, "Done.\n");
	return 0;
}


int main(int argc, char **argv)
{
const char *portPath = "/dev/serial/by-id/";
const char *filename = NULL;
char portName[300];
int adc=-1;
int uartmode=-1;
int uartinfo=0;
int uarttx_len=0;
int uartrx=0;
char uarttx_data[300]="";
int length;
struct termios config;

	verify = 1;										// verify unless told not to
 	DIR * d = opendir(portPath); 						// open the path
	struct dirent *dir; 							// for the directory entries
	if(d==NULL) strcpy(portName, "/dev/ttyACM0");		// default serial port to use
	else {
		while ((dir = readdir(d)) != NULL) 			// if we were able to read somehting from the directory
		{
			char *pF = strstr(dir->d_name, "iceFUN");
			if(pF) {
				strcpy(portName, portPath);			// found iceFUN board so copy full symlink
				strcat(portName, dir->d_name);
				break;
			}
		}
		closedir(d);
	}	

/* Decode command line parameters */
	static struct option long_options[] = {
		{"help", no_argument, NULL, -2},
		{NULL, 0, NULL, 0}
	};

	int opt;
  char *endptr;
	while ((opt = getopt_long(argc, argv, "P:o:A:U:T:R:I:vh", long_options, NULL)) != -1) {
		switch (opt) {
		case 'P': /* Serial port */
			strcpy(portName, optarg);
			break;
		case 'A': /* ADC */
			adc = atoi(optarg);
			break;
		case 'R': /* UART RX */
			uartrx = 1;
			uarttx_len = strlen(optarg);
			strncpy(uarttx_data, optarg, uarttx_len);
      uarttx_data[uarttx_len] = 0;
			break;
		case 'I': /* UART Info -- if param > 1 -- set SPBRG */
			uartinfo = atoi(optarg);
			break;
		case 'h':
		case -2:
			help(argv[0]);
			close(fd);
			return EXIT_SUCCESS;
		case 'v':
			verify = 0;
			break;
    case 'o': /* set address offset */
      rw_offset = strtol(optarg, &endptr, 0);
      if (*endptr == '\0')
        /* ok */;
      else if (!strcmp(endptr, "k"))
        rw_offset *= 1024;
      else if (!strcmp(endptr, "M"))
        rw_offset *= 1024 * 1024;
      else {
        fprintf(stderr, "'%s' is not a valid offset\n", optarg);
        return EXIT_FAILURE;
      }
      break;
		default:
			/* error message has already been printed */
			fprintf(stderr, "Try `%s -h' for more information.\n", argv[0]);
			close(fd);
			return EXIT_FAILURE;
		}
	}

	fd = open(portName, O_RDWR | O_NOCTTY);
	if(fd == -1) {
		fprintf(stderr, "%s: failed to open serial port.\n", argv[0]);
		return EXIT_FAILURE;
	}
	tcgetattr(fd, &config);
	cfmakeraw(&config);								// set options for raw data
	tcsetattr(fd, TCSANOW, &config);

	if (optind + 1 == argc) {
		filename = argv[optind];
	} else if (optind != argc) {
		fprintf(stderr, "%s: too many arguments\n", argv[0]);
		fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);
		close(fd);
		return EXIT_FAILURE;
	} else  {
		fprintf(stderr, "%s: missing argument\n", argv[0]);
		fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);
		close(fd);
		return EXIT_FAILURE;
	}

	if (adc != -1) {
	    GetADC(adc);
	    return 0;
	}

	if (uartinfo != 0) {
	    GetUARTInfo(uartinfo); // SPBRG if > 1
	    return 0;
	}

	if (uartrx != 0) {
	    SendRecvUARTData(uarttx_data);
	    return 0;
	}

	FILE* fp = fopen(filename, "rb");
	if(fp==NULL) {
		fprintf(stderr, "%s: failed to open file %s.\n", argv[0], filename);
		close(fd);
		return EXIT_FAILURE;
	}
	length = fread(&FPGAbuf[rw_offset], 1, FLASHSIZE, fp);

	strcpy(ProgName, argv[0]);
	if(!GetVersion()) resetFPGA();						// reset the FPGA
	else return EXIT_FAILURE;

  int endPage = ((rw_offset + length) >> 16) + 1;
  for (int page = (rw_offset >> 16); page < endPage; page++)			// erase sufficient 64k sectors
  {
    SerBuf[0] = ERASE_64k;
    SerBuf[1] = page;
    write(fd, SerBuf, 2);
    fprintf(stderr,"Erasing sector %02X0000\n", page);
    read(fd, SerBuf, 1);
  }
  fprintf(stderr, "file size: %d\n", (int)length);

	int addr = rw_offset;
	fprintf(stderr,"Programming ");						// program the FPGA
	int cnt = 0;
  int endAddr = addr + length; // no flashsize check
	while (addr < endAddr)
	{
		SerBuf[0] = PROG_PAGE;
		SerBuf[1] = (addr>>16);
		SerBuf[2] = (addr>>8);
		SerBuf[3] = (addr);
		for (int x = 0; x < 256; x++) SerBuf[x + 4] = FPGAbuf[addr++];
		write(fd, SerBuf, 260);
		read(fd, SerBuf, 4);
		if (SerBuf[0] != 0)
		{
			fprintf(stderr,"\nProgram failed at %06X, %02X expected, %02X read.\n", addr - 256 + SerBuf[1] - 4, SerBuf[2], SerBuf[3]);
			return EXIT_FAILURE;
		}
		if (++cnt == 10)
		{
			cnt = 0;
			fprintf(stderr,".");
		}
	}

	if(verify) {
		addr = rw_offset;
		fprintf(stderr,"\nVerifying ");
		cnt = 0;
		while (addr < endAddr)
		{
			SerBuf[0] = VERIFY_PAGE;
			SerBuf[1] = (addr >> 16);
			SerBuf[2] = (addr >> 8);
			SerBuf[3] = addr;
			for (int x = 0; x < 256; x++) SerBuf[x + 4] = FPGAbuf[addr++];
			write(fd, SerBuf, 260);
			read(fd, SerBuf, 4);
			if (SerBuf[0] > 0)
			{
				fprintf(stderr,"\nVerify failed at %06X, %02X expected, %02X read.\n", addr - 256 + SerBuf[1] - 4, SerBuf[2], SerBuf[3]);
				return EXIT_FAILURE;
			}
			if (++cnt == 10)
			{
				cnt = 0;
				fprintf(stderr,".");
			}
		}
	}
	fprintf(stderr,"\n");

	releaseFPGA();
	return 0;
}



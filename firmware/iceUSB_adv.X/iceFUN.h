
// This is a guard condition so that contents of this file are not included
// more than once.  
#ifndef XC_HEADER_TEMPLATE_H
#define	XC_HEADER_TEMPLATE_H

#include <xc.h> // include processor files - each processor file is guarded.  

#define MODULE  38
#define VERSION     2

#define USB_SIZE 300
#define UART_BUF_SIZE 64


#define BLUE        LATCbits.LATC6
#define CRESET      LATAbits.LATA5
#define CDONE       PORTBbits.RB6

#define SDO         LATCbits.LATC2
#define SDOtris     TRISCbits.TRISC2
#define SDI         PORTCbits.RC3
#define SDItris     TRISCbits.TRISC3
#define SCK         LATCbits.LATC4
#define SCKtris     TRISCbits.TRISC4
#define CS          LATCbits.LATC5
#define CStris      TRISCbits.TRISC5

#define AD1tris     TRISCbits.TRISC0
#define AD2tris     TRISCbits.TRISC1
#define AD3tris     TRISCbits.TRISC7
#define AD4tris     TRISBbits.TRISB4
#define AD1sel      ANSELCbits.ANSC0
#define AD2sel      ANSELCbits.ANSC1
#define AD3sel      ANSELCbits.ANSC7
#define AD4sel      ANSELBbits.ANSB4

#define FL_READ         0x03
#define FL_ERASE64      0xD8
#define FL_CHIPERASE    0xC7
#define FL_PROG         0x02
#define FL_WRITE_EN     0x06
#define FL_WRITE_DIS    0x04
#define FL_ERASESEQ     0x44
#define FL_PROGSEQ      0x42
#define FL_READSEQ      0x48
#define FL_READSTAT1    0x05
#define FL_READSTAT2    0x35
#define FL_WRITESTAT    0x01
#define WEVS            0x50
#define READMANID       0x9F
#define READID          0x90
#define POWERDOWN       0xB9
#define POWERUP_ID      0xAB

void ADhandler(char chn);

#endif	/* XC_HEADER_TEMPLATE_H */


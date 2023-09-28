
#include <xc.h>
#include <stdint.h>
#include "mcc_generated_files/usb/usb.h"
#include "iceFUN.h"

void Get_USB_serial(void);

#define UART_TIMEOUT 65535
#define USB_TIMEOUT 65535

extern uint8_t usbBuf[USB_SIZE];
extern uint16_t usbIdx;
extern uint8_t BytesToSend;
uint8_t Counter = 1;

enum cmds {
    AD1=0xa1, AD2, AD3, AD4,
    DONE=0xb0, GET_VER, RESET_FPGA, ERASE_CHIP, ERASE_64K, PROG_PAGE, READ_PAGE, VERIFY_PAGE, GET_CDONE, RELEASE_FPGA,
    WRITE_READ_USART=0xc0, GET_SET_USART_CONFIG
};


uint8_t flashRW(uint8_t d)
{
    for(uint8_t x=0; x<8; x++)
    {
        if(d&0x80) SDO=1; else SDO=0;
        d <<= 1;
        if(SDI) d|=1;
        SCK = 1;
        NOP();
        NOP();
        NOP();
        NOP();
        SCK = 0;
    }
    return d;
}

void flashWrEn(void)
{
    CS = 0;
    flashRW(FL_WRITE_EN);
    CS = 1;
}

uint8_t flashStatus(void)
{
    CS = 0;
    flashRW(FL_READSTAT1);
    uint8_t x = flashRW(0);
    CS = 1;
    return x;
}

void flashBsyWait(void)
{
    while(flashStatus()&0x01);
}

void flashErase64k(uint8_t addrH)
{
    flashWrEn();
    CS = 0;
    flashRW(FL_ERASE64);
    flashRW(addrH);
    flashRW(0);
    flashRW(0);
    CS = 1;   
    flashBsyWait();
}

void flashReadPage(uint8_t addrH, uint8_t addrM, uint8_t addrL)
{
    while(USBUSARTIsTxTrfReady() == false) CDCTxService();
    flashRW(FL_READ);
    flashRW(addrH);
    flashRW(addrM);
    flashRW(addrL);
    for(uint8_t chunk=0; chunk<4; chunk++) {
        for(uint8_t x=0; x<64; x++) usbBuf[x] = flashRW(0);
        putUSBUSART(usbBuf,64);
        CDCTxService();
        while(USBUSARTIsTxTrfReady() == false) CDCTxService();
    }
}

void resetFPGA(void)
{
    CRESET = 0;
    SDO = 1;
    SDOtris = 0;
    SDItris = 1;
    SCK = 0; 
    SCKtris = 0;
    CS = 1;
    CStris = 0;
    for(uint8_t x=0; x<100; x++) NOP();
    CS = 0;
    flashRW(POWERUP_ID);
    flashRW(0);    // dummy
    flashRW(0);    // dummy
    flashRW(0);    // dummy
    flashRW(0);    // read legacy ID (but don't really want it)
    CS = 1;
}

void verifyPage(void)    // verify the page currently in memory
{
    CS = 0;
    flashRW(FL_READ);
    flashRW(usbBuf[1]);
    flashRW(usbBuf[2]);
    flashRW(usbBuf[3]);
    for(usbIdx=4; usbIdx<260; usbIdx++) {
        uint8_t byte = flashRW(0);
        if(byte!=usbBuf[usbIdx]) {
            CS = 1;
            usbBuf[0] = (uint8_t)(-1);                  // fail
            usbBuf[1] = (uint8_t)(usbIdx%256);
            usbBuf[2] = usbBuf[usbIdx];
            usbBuf[3] = byte;
            return;
        }
    }
    CS = 1;
    usbBuf[0] = 0;                  // success
}


void SendToSerial(uint8_t SendLen)
{
    uint8_t usbBufStart = 3; // cmd(1byte),sendlen(1byte),recvlen(1byte)
    uint8_t i;
    for(i=0;i<SendLen;i++) {
       TXREG = usbBuf[i+usbBufStart];                 // send data to FPGA
       NOP();
       NOP();
       while(!PIR1bits.TXIF);
    }
}

void RecvFromSerial(uint8_t RecvLen)
{
    uint8_t usbBufStart = 2; // will be >0 once we implement sending length in response
    uint8_t RecvLenCurrent=0;
    usbBuf[1] = 0; // all ok
    for(RecvLenCurrent=0;RecvLenCurrent<RecvLen;RecvLenCurrent++) {
       uint16_t RecvTimeout = UART_TIMEOUT;

       // wait for incoming character or timeout
       while(PIR1bits.RCIF == false && RecvTimeout>0) RecvTimeout--;

       // record timeout or frameing/overflow error and break
       if (RecvTimeout == 0) { usbBuf[1] = 1; break; }
       if (RCSTAbits.FERR)   { usbBuf[1] = 2; break; }
       if (RCSTAbits.OERR)   { 
           // to clear -- 
           RCSTAbits.CREN = 0;
           NOP();
           RCSTAbits.CREN = 1;
           usbBuf[1] = 3; 
           break; 
       }

       // record incoming byte
       usbBuf[RecvLenCurrent+usbBufStart] = RCREG;
       PIR1bits.RCIF = 0; // clear interrupt flag
       NOP();
       NOP();
    }
    usbBuf[0] = RecvLenCurrent;
    BytesToSend = RecvLenCurrent+usbBufStart;
}

void SetUSARTConfig()
{
  switch(usbBuf[1]) {
    case 1:
      TXSTA = usbBuf[2];
      break;
    case 2: 
      RCSTA = usbBuf[2];
      break;
    case 3: 
      BAUDCON = usbBuf[2];
      break;
    case 4: 
      SPBRG = usbBuf[2]; // +256*usbBuf[2];
      break;
    default:
      break;
  }
}

void GetUSARTConfig()
{
  // usbBuf[0] = usbBuf[0];
  usbBuf[1] = TXSTA;
  usbBuf[2] = RCSTA;
  usbBuf[3] = BAUDCON;
  usbBuf[4] = SPBRG & 0xff;
  usbBuf[5] = (SPBRG >> 8) & 0xff;
  usbBuf[6] = USB_SIZE & 0xff;
  usbBuf[7] = (USB_SIZE >> 8) & 0xff;
  usbBuf[8] = USB_TIMEOUT & 0xff;
  usbBuf[9] = (USB_TIMEOUT >> 8) & 0xff;
  usbBuf[10] = UART_TIMEOUT & 0xff;
  usbBuf[11] = (UART_TIMEOUT >> 8) & 0xff;
  BytesToSend = 12;
}

//void releaseFPGA(void)
//{
//    SDOtris = 1;
//    SDItris = 1;
//    SCKtris = 1;
//    CStris = 1;
//    CRESET = 1;
//}

void commandManager(void)
{
//uint8_t c;
uint16_t x;

    if(usbIdx==0) return;
    switch(usbBuf[0]) {
        case GET_VER:
            usbBuf[0] = MODULE;
            usbBuf[1] = VERSION;
            usbIdx = 0;
            BytesToSend = 2;
            break;
        case RESET_FPGA:
            resetFPGA();
            CS = 0;
            flashRW(READMANID);
            usbBuf[0] = flashRW(0);    // manufacturer ID
            usbBuf[1] = flashRW(0);    // device ID 1
            usbBuf[2] = flashRW(0);    // device ID 2
            CS = 1;
            usbIdx = 0;
            BytesToSend = 3;
            break;                    
        case READ_PAGE:
            while(usbIdx<4) Get_USB_serial();    // make sure address is in
            uint8_t addrH = usbBuf[1];
            uint8_t addrM = usbBuf[2];
            uint8_t addrL = usbBuf[3];          
            CS = 0;
            flashReadPage(addrH, addrM, addrL);
            CS = 1;
            usbIdx = 0;
            break;
        case ERASE_64K:
        {
            while(usbIdx<2) Get_USB_serial();    // make sure address is in
            uint8_t addrH = usbBuf[1];
            flashErase64k(addrH);
            usbIdx = 0;
            usbBuf[0] = DONE;
            BytesToSend = 1;
        }
            break;
        case PROG_PAGE:
        {
            while(usbIdx<260) Get_USB_serial();    // wait for address + 256 bytes
            flashWrEn();
            uint8_t addrH = usbBuf[1];
            uint8_t addrM = usbBuf[2];
            uint8_t addrL = usbBuf[3];          
            CS = 0;
            flashRW(FL_PROG);
            flashRW(addrH);
            flashRW(addrM);
            flashRW(addrL);
            for(usbIdx=4; usbIdx<260; usbIdx++) flashRW(usbBuf[usbIdx]);
            CS = 1;
//            flashBsyWait();
//            verifyPage();
            usbBuf[0] = 0;

            usbIdx = 0;
            BytesToSend = 4;
        }
            break;
        case VERIFY_PAGE:
            while(usbIdx<260) Get_USB_serial();     // wait for address + 256 bytes
            BytesToSend = 4;
            verifyPage();
            usbIdx = 0;
            break;
        case RELEASE_FPGA:
            for(x=0; x<60000; x++) NOP();
            SDOtris = 1;
            SDItris = 1;
            SCKtris = 1;
            CStris = 1;
            CRESET = 1;
            usbBuf[0] = 0;
            BytesToSend = 1;
            usbIdx = 0;
            break;
        case WRITE_READ_USART:
        {
            uint16_t RecvTimeout = USB_TIMEOUT;
            while(usbIdx<3 && RecvTimeout--) Get_USB_serial();    // make sure lengths (recvlen/sendlen) are in
            if (RecvTimeout==0) { usbIdx=0; break; }

            uint8_t SendLen = usbBuf[1];
            uint8_t RecvLen = usbBuf[2];

            RecvTimeout = USB_TIMEOUT;
            while(usbIdx<SendLen+3 && RecvTimeout--) Get_USB_serial();     // wait for data
            if (RecvTimeout==0) { usbIdx=0; break; }

            if (SendLen > 0) SendToSerial(SendLen);
            if (RecvLen > 0) RecvFromSerial(RecvLen);
            usbIdx = 0;
        }
            break;
        case GET_SET_USART_CONFIG:
            if (usbIdx>1) SetUSARTConfig();
            GetUSARTConfig();
            usbIdx = 0;
            break;
        case AD1:                           // Get Analogue channel
        case AD2:
        case AD3:
        case AD4:
            ADhandler(usbBuf[0]);
            usbBuf[0] = ADRESH;
            usbBuf[1] = ADRESL;
            BytesToSend = 2;
            usbIdx = 0;
            break;
    }
   
}

// Zero for UART, 1,2,3 or 4 for USB
void ADhandler(char chn)  
{
unsigned char x;
    
    if(RCSTAbits.OERR) {
        RCSTAbits.CREN = 0;
        NOP();
        RCSTAbits.CREN = 1;
    }
    if(PIR1bits.RCIF) {
        x = RCREG;
        switch(x) {
            case 0xa1:  ADCON0 = (4<<2)+1;      // AN5
                        break;
            case 0xa2:  ADCON0 = (5<<2)+1;      // AN4
                        break;
            case 0xa3:  ADCON0 = (9<<2)+1;      // AN9
                        break;
            case 0xa4:  ADCON0 = (10<<2)+1;     // AN10
                        break;
            default:    return;
        }
    }
    else {
        switch(chn) {
            case 0xa1:  ADCON0 = (4<<2)+1;      // AN5
                        break;
            case 0xa2:  ADCON0 = (5<<2)+1;      // AN4
                        break;
            case 0xa3:  ADCON0 = (9<<2)+1;      // AN9
                        break;
            case 0xa4:  ADCON0 = (10<<2)+1;     // AN10
                        break;
            default:    return;
        }
    }

    ADCON1 = 0xE0;
    ADCON2 = 0;
    for(x=0; x<50; x++);
    ADCON0bits.ADGO = 1;
    NOP();
    NOP();
    while(ADCON0bits.ADGO);
    if(chn) return;                 // USB command so just return
    TXREG = ADRESL;                 // else send data to FPGA
    NOP();
    NOP();
    while(!PIR1bits.TXIF);
    TXREG = ADRESH;
    NOP();
    NOP();
    while(!PIR1bits.TXIF);        
}


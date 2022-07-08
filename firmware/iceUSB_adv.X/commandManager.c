
#include <xc.h>
#include <stdint.h>
#include "mcc_generated_files/usb/usb.h"
#include "iceFUN.h"

void Get_USB_serial(void);

extern uint8_t usbBuf[USB_SIZE];
extern uint16_t usbIdx;
extern uint8_t BytesToSend;

extern uint8_t uartRxBuf[UART_BUF_SIZE];
extern uint16_t uartRxHead;
extern uint16_t uartRxTail;

extern uint8_t uartTxBuf[UART_BUF_SIZE];
extern uint16_t uartTxHead;
extern uint16_t uartTxTail;

extern uint8_t uartMode; // default 0 -- original; 1 -- uart pass through

enum cmds {
    AD0=0xa0, AD1, AD2, AD3, AD4,
    DONE=0xb0, GET_VER, RESET_FPGA, ERASE_CHIP, ERASE_64K, PROG_PAGE, READ_PAGE, VERIFY_PAGE, GET_CDONE, RELEASE_FPGA,         
    UARTMODE=0xc0, UARTRX, UARTTX, UARTINFO
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
            usbBuf[0] = -1;                  // fail
            usbBuf[1] = usbIdx;
            usbBuf[2] = usbBuf[usbIdx];
            usbBuf[3] = byte;
            return;
        }
    }
    CS = 1;
    usbBuf[0] = 0;                  // success
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
        case AD0:                           // Get Analogue channel
        case AD1:
        case AD2:
        case AD3:
        case AD4:
            ADhandler(usbBuf[0]);
            usbBuf[0] = ADRESH;
            usbBuf[1] = ADRESL;
            BytesToSend = 2;
            usbIdx = 0;
            break;
            
        case UARTMODE:
            while(usbIdx<2) Get_USB_serial();    // make sure address is in
            uartMode = usbBuf[1];
            usbBuf[0] = 0;
            usbBuf[1] = uartMode;
            BytesToSend = 2;
            usbIdx = 0;
            break;
            
        case UARTINFO:
            usbBuf[0] = 0;
            usbBuf[1] = uartMode;
            usbBuf[2] = SPBRGL;
            usbBuf[3] = SPBRGH;
            usbBuf[4] = UART_BUF_SIZE%256;
            usbBuf[5] = UART_BUF_SIZE/256;
            usbBuf[6] = uartTxHead%256;;
            usbBuf[7] = uartTxHead/256;
            usbBuf[8] = uartTxTail%256;
            usbBuf[9] = uartTxTail/256;
            usbBuf[10] = UART_BUF_SIZE%256;
            usbBuf[11] = UART_BUF_SIZE/256;
            usbBuf[12] = uartRxHead%256;
            usbBuf[13] = uartRxHead/256;
            usbBuf[14] = uartRxTail%256;
            usbBuf[15] = uartRxTail/256;
            BytesToSend = 16;
            usbIdx = 0;
            break;
            
        case UARTRX:
            while(usbIdx<3) Get_USB_serial();    // make sure the size is in
            if (uartMode != 1) break;
            // send uart buffer to PC (max 256?)
            uint16_t respSize = usbBuf[1]+256*usbBuf[2];   
            uint16_t idx = uartRxTail;
            usbIdx = 2;
            while(idx!=uartRxHead) {
                idx++;
                if (idx==UART_BUF_SIZE) idx=0;
                usbBuf[usbIdx] = uartRxBuf[idx];
                usbIdx++;
                if (usbIdx-2 == respSize) break; // max send size
            }
            uartRxTail = idx;
            
            // respond with how many bytes we are sending and the bytes
            usbBuf[0] = (usbIdx-2)%256;
            usbBuf[1] = (usbIdx-2)/256;
            BytesToSend = respSize+2;
            usbIdx = 0;
            break;
            
        case UARTTX:
            // send usbBuf to uart
            while(usbIdx<3) Get_USB_serial();    // make sure the size is in
            uint16_t expectedSize = usbBuf[1]+256*usbBuf[2];
            while(usbIdx<(expectedSize+3)) Get_USB_serial();    // make sure the size is in
            if (uartMode != 1) 
            {
                usbBuf[0] = 0;
                usbBuf[1] = 0;
                BytesToSend = 2;
                usbIdx = 0;
                break;
            }
            // copy incoming data to Uart Tx buffer
            uint16_t i;
            for(i=0;i<expectedSize;i++)
            {
               uint8_t x=usbBuf[i+3];
               uint16_t incUartTxHead=uartTxHead+1;
               if (incUartTxHead==UART_BUF_SIZE) incUartTxHead=0;
               if (incUartTxHead==uartTxTail) break; // ignore -- out of space
               else
               {
                  uartTxHead = incUartTxHead;
                  uartTxBuf[uartTxHead]=x;
               }              
            }
            
            // respond with how many bytes we received
            usbBuf[0] = i%256;
            usbBuf[1] = i/256;
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
    // if UART data available
    if(PIR1bits.RCIF) {
        x = RCREG;
        if (uartMode == 1) 
        {
            // if head==tail -- nothing in the buffer
            uint16_t incUartRxHead=uartRxHead+1;
            if (incUartRxHead==UART_BUF_SIZE) incUartRxHead=0;
            if (incUartRxHead==uartRxTail) return; // ignore -- out of space
            else
            {
               uartRxHead = incUartRxHead;
               uartRxBuf[uartRxHead]=x;
            }
            return;
        } else
        switch(x) {
            case 0xa0:  ADCON0 = (29<<2)+1;      // Temp
                        break;                         
            case 0xa1:  ADCON0 = (4<<2)+1;      // AN4
                        break;
            case 0xa2:  ADCON0 = (5<<2)+1;      // AN5
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
            case 0xa0:  ADCON0 = (29<<2)+1;      // Temp
                        break;                       
            case 0xa1:  ADCON0 = (4<<2)+1;      // AN4
                        break;
            case 0xa2:  ADCON0 = (5<<2)+1;      // AN5
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
    for(x=0; x<50; x++); // min 200us?
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


#include <stdint.h>
#include "mcc_generated_files/usb/usb.h"
#include "iceFUN.h"

uint8_t BytesToSend = 0;

uint8_t usbBuf[USB_SIZE];
uint16_t usbIdx=0;

uint8_t uartRxBuf[UART_BUF_SIZE];
uint16_t uartRxHead=0;
uint16_t uartRxTail=0;

uint8_t uartTxBuf[UART_BUF_SIZE];
uint16_t uartTxHead=0;
uint16_t uartTxTail=0;

uint8_t uartMode=0; // default 0 -- original; 1 -- uart pass through

void Get_USB_serial(void)
{
    if( USBGetDeviceState() < CONFIGURED_STATE ) return;
    if( USBIsDeviceSuspended()== true ) return;

    if( USBUSARTIsTxTrfReady() == true)
    {
        uint8_t i;
        uint8_t numBytesRead;

        numBytesRead = getsUSBUSART(&usbBuf[usbIdx], 64);
        usbIdx += numBytesRead;
    }
    
    if( USBUSARTIsTxTrfReady() == true)
    {
        if(BytesToSend > 0)
        {
            putUSBUSART(usbBuf,BytesToSend);
            BytesToSend = 0;
        }
    }
    
    CDCTxService();
}


void UART_Tx_Queue(void)
{
    if (uartMode == 1 && PIR1bits.TXIF == 1 && uartTxHead != uartTxTail)
    {
       TXREG = uartTxBuf[uartTxTail];
       uartTxTail++;
       if (uartTxTail == UART_BUF_SIZE) uartTxTail = 0;
    }
}


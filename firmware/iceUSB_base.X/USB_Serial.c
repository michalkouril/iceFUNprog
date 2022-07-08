#include <stdint.h>
#include "mcc_generated_files/usb/usb.h"
#include "iceFUN.h"

//static uint8_t readBuffer[64];

//uint8_t writeBuffer[64];
uint8_t BytesToSend = 0;

uint8_t usbBuf[USB_SIZE];
uint16_t usbIdx=0;


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

//    if( USBGetDeviceState() < CONFIGURED_STATE )
//    {
//        return;
//    }
//
//    if( USBIsDeviceSuspended()== true )
//    {
//        return;
//    }
//
//    if( USBUSARTIsTxTrfReady() == true)
//    {
//        uint8_t i;
//        uint8_t numBytesRead;
//
//        numBytesRead = getsUSBUSART(readBuffer, sizeof(readBuffer));
//
//        for(i=0; i<numBytesRead; i++)
//        {
//            switch(readBuffer[i])
//            {
//                /* echo line feeds and returns without modification. */
//                case 0x0A:
//                case 0x0D:
//                    writeBuffer[i] = readBuffer[i];
//                    break;
//
//                /* all other characters get +1 (e.g. 'a' -> 'b') */
//                default:
//                    writeBuffer[i] = readBuffer[i] + 1;
//                    break;
//            }
//        }
//
//        if(numBytesRead > 0)
//        {
//            putUSBUSART(writeBuffer,numBytesRead);
//        }
//    }
//
//    CDCTxService();
//}


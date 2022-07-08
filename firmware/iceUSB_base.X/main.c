

#include "mcc_generated_files/mcc.h"
#include "iceFUN.h"

void commandManager(void);

void iceFUNinit(void)
{
    AD1tris = 1;
    AD2tris = 1;
    AD3tris = 1;
    AD4tris = 1;
    AD1sel = 1;
    AD2sel = 1;
    AD3sel = 1;
    AD4sel = 1;
    
    SDO = 1;        // start with SPI port tristated
    SDOtris = 1;
    SDItris = 1;
    SCK = 0; 
    SCKtris = 1;
    CS = 1;
    CStris = 1;
  
    CRESET = 1;     // start the FPGA on power up.

    TXSTA = 0x22;
    RCSTA = 0x90;
    BAUDCON = 0x08;
    SPBRG = 11;
}


void main(void)
{
    // initialize the device
    SYSTEM_Initialize();
    iceFUNinit();
    
    // When using interrupts, you need to set the Global and Peripheral Interrupt Enable bits
    // Use the following macros to:

    // Enable the Global Interrupts
    INTERRUPT_GlobalInterruptEnable();

    // Enable the Peripheral Interrupts
    INTERRUPT_PeripheralInterruptEnable();

    // Disable the Global Interrupts
    //INTERRUPT_GlobalInterruptDisable();

    // Disable the Peripheral Interrupts
    //INTERRUPT_PeripheralInterruptDisable();

    while (1)
    {
        // Add your application code
        Get_USB_serial();
        commandManager();
        ADhandler(0);
    }
}
/**
 End of File
*/
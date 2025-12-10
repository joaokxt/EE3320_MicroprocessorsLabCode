#include <stdint.h> // so I can use the integers I want
#include <stdio.h> // so I can use sprintf
#include "sleep.h"

// Useful defines
#define timedelay 200000
#define CFG_NoSS 0x0FC27
#define CFG_SS0 0x0C027
#define CFG_SS0_Start 0x1C027

////////////
// IIC defines
///////////
#define IIC_CFG *((uint32_t *) 0xE0005000)// IIC config register
#define IIC_STAT *((uint32_t *) 0xE0005004)// IIC Status config register
#define IIC_ADDR *((uint32_t *) 0xE0005008)// IIC Address register
#define IIC_DATA *((uint32_t *) 0xE000500C)// IIC Data register
#define IIC_TSIZE *((uint32_t *) 0xE0005014)// IIC Transfer Size register
#define IIC_ISR *((uint32_t *) 0xE0005010)// IIC Interupt Status register
#define IIC_Config 0x0C0F // 0x0C0E write 0x0C0F read
//SLCR Register addresses and lock/unlock key values
#define SLCR_LOCK *( (uint32_t *) 0xF8000004)
#define SLCR_UNLOCK *( (uint32_t *) 0xF8000008)
#define SLCR_IIC_RST *( (uint32_t *) 0xF8000224)
#define UNLOCK_KEY 0xDF0D
#define LOCK_KEY 0x767B
#define TimerDelay 200000
#define LM75B_Addr 0x48

////////////
// SPI defines
/////////////
// spi registers to work with the spi IP block in zynq
//we want to talk to SPI0
// define the minimum set to use
#define SPI0_CFG *((uint32_t *) 0xE0006000)// SPI config register
#define SPI0_EN *((uint32_t *) 0xE0006014)// SPI Enable register
#define SPI0_SR *((uint32_t *) 0xE0006004)// SPI Enable register
#define SPI0_TXD *((uint32_t *) 0xE000601C)// SPI write data port register
#define SPI0_RXD *((uint32_t *) 0xE0006020)// SPI read data port register
/////////////////////////////////////
#define LSM9DS1_Who 0x0f
#define LSM9DS1_CTRL_Reg1 0x10
#define LSM9DS1_CTRL_Reg2 0x20
#define LSM9DS1_Temp_G_low 0x15
#define LSM9DS1_Temp_G_high 0x16
#define JUNK 0
/////////////////////////////////////////////
//
// UnLock SPI
//
//SLCR addresses for SPI reset
#define SLCR_LOCK *( (uint32_t *) 0xF8000004)
#define SLCR_UNLOCK *( (uint32_t *) 0xF8000008)
#define SLCR_SPI_RST *( (uint32_t *) 0xF800021C)
//SLCR lock and unlock keys
#define UNLOCK_KEY 0xDF0D
#define LOCK_KEY 0x767B
////////////////////////

////////////
// UART defines
///////////
#define UART1_CR *((uint32_t *) 0xE0001000)
#define UART1_MR *((uint32_t *) 0xE0001004)
#define UART1_BAUDGEN *((uint32_t *) 0xE0001018)
#define UART1_BAUDDIV *((uint32_t *) 0xE0001034)

#define UART1_SR *((uint32_t *) 0xE000102C)
#define UART1_DATA *((uint32_t *) 0xE0001030)

#define BaudGen115200 0x7c
#define BaudDiv115200 6

////////////
// 7-segment defines
///////////
#define SEG_CTL *((uint32_t *) 0x43C10000)
#define SEG_DATA *((uint32_t *) 0x43C10004)

////////////
// Switch defines
///////////
#define BTN_DATA *((uint32_t *) 0x41200000)  // Buttons are the lower 4 bits


/*
    UART Setup 
*/

int UARTFullTx(){
    return(UART1_SR & 0x10) != 0;     // Return 1 if UART1 FIFO is full
}

int UARTEmptyRx(){
    if ((UART1_SR & 0x02) == 2){      // Return 1 if UART1 FIFO is empty
        return 1;
    }else {
        return 0;
    }
}

void ResetUart(){
    UART1_CR = 0x03;                // Reset UART's TX and RX FIFOs by driving '1' to corresponding bitfields
    while((UART1_CR & 0x01) == 1){} // ZYNQ automatically resets bits to '0' when reset is complete
    while((UART1_CR & 0x02) == 2){}
}

void SetBaud(){                      // Set Baudrate to 115200
    UART1_BAUDGEN = BaudGen115200;   // Set BAUDGEN to define RX sampling clock 
    UART1_BAUDDIV = BaudDiv115200;   // Set BAUDDIV to define TX clock. This clock will be the communication pacesetter
}

void ConfigureUart(){
  UART1_MR = 0x20;    

  uint32_t UARTtemp; 
  UARTtemp = UART1_CR;  // Copy current config
  UARTtemp |= 0x14;     // Mask 0001 0100 (TxE and RxE set to '1')
  UART1_CR = UARTtemp;  // Pass new config
}

void init_uart(){        // Initialize UART by resetting it, setting the Baudrate and passing 8N1 config.
    ResetUart();
    SetBaud();
    ConfigureUart();
}

/*
    Code for sending and receiving data in UART
*/

void UARTSendChar(char c){          
    while(UARTFullTx() == 1){}      // Wait for FIFO to empty
    UART1_DATA = c;                 // Pass character to TX
}

void UARTSendString(char *str){     
    while(*str != '\0'){            // Check if reached the end of null-terminated string
        UARTSendChar(*str);         // Send character-by-character
        str++;                      // Move pointer one position in memory (next char)
    }
}

char UARTGetChar(){
    while(UARTEmptyRx()){sleep(3);} // Wait for FIFO to fill up;
    return (char) UART1_DATA;       // Retrieve data and cast to char
}


/*
    IIC Setup
*/

/*
    Reset I2C. Credit to M. Welker
*/
void reset_iic(void){
    SLCR_UNLOCK = UNLOCK_KEY;       // Unlock SLCRs
    SLCR_IIC_RST = 0x3;             // Assert I2C reset
    SLCR_IIC_RST = 0;               // Deassert I2C reset
    SLCR_LOCK = LOCK_KEY;           // Relock SLCRs
}

void config_iic(){

    IIC_CFG = IIC_Config;
}

void init_iic(){
    reset_iic();
    config_iic();
}

/*
    Read from LM75B through I2C
*/

int read_temp_iic(){
    int lowByte, highByte;
    int temperature = 0;

    // Write the number of bytes to read in TRANS_SIZE (16 bits = 2 bytes)
    IIC_TSIZE =  2;
    // Write slave address to ADDR
    IIC_ADDR = LM75B_Addr;

    // Wait for TC bit in ISR to turn to 1 (Transmission complete)
    while((IIC_ISR & 0x1) != 1){
        usleep(timedelay);
    }

    highByte = IIC_DATA;            // Read first byte on FIFO.
    lowByte = IIC_DATA;             // Read next byte on FIFO.
    
    temperature |= (lowByte >> 5);  // Only bits 5, 6, and 7 of LSByte are useful.     
    temperature |= (highByte << 3); // All bits of MSByte are useful.

    // If MSB is 1, number is negative. Convert knowing that it's 2's complement.
    if(highByte & 0x80){
        temperature--;
        temperature = ~temperature;  
    }

    // According to NXP, temperature sensor has a resolution of 0.125C°/bit.
    return temperature * 0.125;    
}

/*
    SPI Setup
*/

void reset_spi(void)
{
	int i=0;                    // i for delay
	SLCR_UNLOCK = UNLOCK_KEY;	// Unlock SLCRs
	SLCR_SPI_RST = 0xF;		    // Assert SPI reset
	for(i=0;i<1000;i++);		// Make sure Reset occurs
	SLCR_SPI_RST = 0;		    // Deassert
	SLCR_LOCK = LOCK_KEY;		// Relock SLCRs
}

void write_spi(uint8_t adr, uint8_t byteToWrite){
    uint32_t dummyRead = 0;

    while((SPI0_SR & 0x10)) {  // Wait RX FIFO to empty
        dummyRead = SPI0_RXD;
    }
   
    SPI0_CFG = CFG_SS0;             // Slave select 0. Put SPI in manual mode.
    SPI0_TXD = adr;                 // Send address. Write bit is MSB (0)
    SPI0_TXD = byteToWrite;         // Send message to be transmitted
    SPI0_CFG = CFG_SS0_Start;       // Initiate transmission.
   
    usleep(timedelay);
   
    dummyRead = SPI0_RXD;           // One dummy read to push data into peripheral       

    SPI0_CFG = CFG_NoSS;            // Stop transmission and deselect slave
}

uint8_t read_spi(uint8_t adr){
    uint8_t dummyRead;
    uint8_t returnRead;

    while(SPI0_SR & 0x10) {  // Wait RX FIFO to empty
        dummyRead = SPI0_RXD;
    }
   
    SPI0_CFG = CFG_SS0;             // Enable manual mode & select SS0
    SPI0_TXD = (adr | 0x80);        // Pass address and read bit to TX register
    SPI0_TXD = 0x00;                // Add dummy byte for reading
    SPI0_CFG = CFG_SS0_Start;       // Start transmisstion
   
    usleep(timedelay);
   
    dummyRead = SPI0_RXD;           // 1st read byte is a dummy (1st write was address only)
    returnRead = SPI0_RXD;          // 2nd read to retrieve actual data (2nd write was a dummy)
   
    SPI0_CFG = CFG_NoSS;            // Stop transmision and deselect slave
   
    return returnRead;
}


uint16_t read_temp_spi(){
    int8_t spiLow = 0, spiHigh = 0;
    int16_t spiReading, spiCelsius;
    spiLow = read_spi(LSM9DS1_Temp_G_low);      // Read low byte for temperature
    spiHigh = read_spi(LSM9DS1_Temp_G_high);    // Read high byte
    spiReading = (spiHigh << 8) | spiLow;       // Shift bytes into correct bitfield in 16-bit format
    if(spiHigh & 0x80){                         // If MSB of High byte is 1, number is negative. Convert.
        spiReading--;                           
        spiReading = ~spiReading;
    }
    spiCelsius = (spiReading / 16) + 25;        // Resolution of (1/16)C*/bit and offset of 25C.
    return spiCelsius;
}

uint16_t read_z_gyro_spi(){
    int8_t zAxisL, zAxisH;
    int16_t zAxis;
    zAxisL = read_spi(0x1C);                // Read LSByte
    zAxisH = read_spi(0x1D);                // Read MSByte
    zAxis = (zAxisH << 8) | zAxisL;         // Shift into correct bitfields
    return zAxis;
}   


/*
    7-Segment display code
*/

void display_num(uint16_t n) {
    int i = 0;
    uint32_t temp = 0;

    int decomposedNumber[4];                // Decompose number in powers of 10

    decomposedNumber[0] = n%10;             // Ones
    decomposedNumber[1] = (n/10)%10;        // Tens
    decomposedNumber[2] = (n/100)%10;       // Hundreds
    decomposedNumber[3] = (n/1000)%10;      // Thousands

    for(i=0; i<4; i++){
        // Grab digit and shift it into its corresponding bitfield (each digit spans 1 byte)
        temp |= decomposedNumber[i] << (i*8);
    }

    // Disable decimal points 
    // Each byte's bit 7 corresponds to the dp. Because it's active low, we use 0x80 = 1000 0000.
    temp |= 0x80808080;

    SEG_CTL = 1;

    SEG_DATA = temp;
}

/*
    Main function
*/

int main(void){
    char buffer [64]; 
    uint16_t tempIIC, tempSPI, zAxisReading;

    init_uart();
    init_iic();
    
    reset_spi();
    SPI0_CFG = CFG_NoSS;
    SPI0_EN = 1;

    write_spi(LSM9DS1_CTRL_Reg1, 0xA0);         // Configure LSM9DS1 sensor's baudrate
    write_spi(LSM9DS1_CTRL_Reg2, 0xA0);

    for(;;){
        UARTSendString("\r\n");
        if(BTN_DATA & 0x1){
            //////////////////////////////////////
            // Read temperature through SPI
            //////////////////////////////////////
            tempSPI = read_temp_spi();
            display_num(tempSPI);
            sprintf(buffer, "Current temperature is %d C°, according to LSM9DS1 sensor.\r\n", tempSPI);
            //////////////////////////////////////
        } else {
            //////////////////////////////////////
            // Read temperature through I2C
            //////////////////////////////////////
            tempIIC = read_temp_iic();
            display_num(tempIIC);
            sprintf(buffer, "Current temperature is %d C°, according to LM75B sensor.\r\n", tempIIC);
            //////////////////////////////////////
        }

        UARTSendString(buffer);
        zAxisReading = read_z_gyro_spi();
        sprintf(buffer, "Z-Axis angular rate: %d ", zAxisReading);
        UARTSendString(buffer);
        UARTSendString("\r\n----------------------------------------");

        sleep(1);
    }
}

#include <stdint.h> // so I can use the integers I want
#include <stdio.h> // so I can use sprintf
#include "sleep.h"

// Useful defines
#define timedelay 200000
#define CFG_NoSS 0x0BC27
#define CFG_SS0 0x08C27
#define CFG_SS0_Start 0x18C27

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

void reset_iic(void){
    SLCR_UNLOCK = UNLOCK_KEY; //unlock SLCRs
    SLCR_IIC_RST = 0x3; //assert I2C reset
    SLCR_IIC_RST = 0; //deassert I2C reset
    SLCR_LOCK = LOCK_KEY; //relock SLCRs
}

void config_iic(){

    // Set clock to 400kHz. 111MHz/((DIVA+1)*(DIVB+1))/22
    // Bits 14 & 15 for DIVA. We want DIVA = 0.
    // Bits 8 - 13 for DIVB. We want DIVB = 12.
    // Bit 7 is reserved. Leave low.
    // Bit 6 clears FIFO and then goes back to 0.
    // Bits 5 & 4 are not relevant because ZYNQ is the only master here.
    // Bit 3 allows master to generate as ACK when receiving. Not necessary, but it is good practice.
    // Bit 2 defines addressing mode. '1' for 7-bit (only format supported by ZYNQ).
    // Bit 1 defines master.
    // Bit 0 determines direction of data. We will only read, so we raise it to 1.

    // This is 0000110001001111= 0xC4F

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

void config_spi(){
    SPI0_CFG = 0x8027;          // Slave pre-selected to 0
}

void init_spi(){
    reset_spi();
    config_spi();
}

void WRITE_SPI(uint8_t adr,uint8_t WRITE_BYTE){
    uint32_t dummy_read;
    while((SPI0_SR & 0x10) != 0) {
        dummy_read = SPI0_RXD;
    }
   
    SPI0_CFG = 0x0C027;
    SPI0_TXD = adr;
    SPI0_TXD = WRITE_BYTE;
    SPI0_CFG = 0x1C027;
   
    while((SPI0_SR & 0x04) == 0);
   
    dummy_read = SPI0_RXD;
   
    SPI0_CFG = 0x0FC27;
}

uint8_t READ_SPI(uint8_t adr){
    uint32_t dummy_read;
    uint32_t return_read;

    while((SPI0_SR & 0x10) != 0) {
        dummy_read = SPI0_RXD;
    }
   
    SPI0_CFG = 0x0C027;
    SPI0_TXD = (adr | 0x80);
    SPI0_TXD = 0x00;
    SPI0_CFG = 0x1C027;
   
    while((SPI0_SR & 0x04) == 0);
   
    dummy_read = SPI0_RXD;
    return_read = SPI0_RXD;
   
    SPI0_CFG = 0x0FC27;
   
    return (uint8_t)return_read;
}

uint16_t read_temp_spi(){
    uint8_t transmitByte = 0, dummyByte = 0;
    uint8_t lowByte, highByte;
    uint16_t temperature = 0;

    transmitByte |= 0x15;        // Shift 7-bit address
    transmitByte |= 0x80;       // Read bit

    SPI0_TXD = transmitByte;

    SPI0_TXD = dummyByte;
    highByte = SPI0_RXD;
    SPI0_TXD = dummyByte;
    lowByte = SPI0_RXD;

    temperature |= (highByte<<8);
    temperature |= lowByte;
    temperature &= 0xFFF;

    if(highByte & 0x8){
        temperature --;
        temperature = ~temperature;
    }

    return temperature;
}

uint16_t read_z_gyro_spi(){
    uint8_t transmitByte = 0, dummyByte = 0;
    uint8_t lowByte, highByte;
    uint16_t zAngular = 0;

    transmitByte |= 0x1C;        // Shift 7-bit address
    transmitByte |= 0x80;        // Read bit

    SPI0_TXD = transmitByte;

    SPI0_TXD = dummyByte;
    highByte = SPI0_RXD;
    SPI0_TXD = dummyByte;
    lowByte = SPI0_RXD;

    zAngular |= (highByte<<8);
    zAngular |= lowByte;
    zAngular &= 0xFFF;

    if(highByte & 0x8){
        zAngular --;
        zAngular = ~zAngular;
    }

    return zAngular;
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
    int currentTemp;
    char buffer [64]; 
    uint16_t currentZAngular;

    int8_t SPI_LO = 0, SPI_HI = 0;
    int16_t SPI_Temp, SPI_TempC;

    int8_t zAxisL, zAxisH;
    int16_t zAxis;

    init_uart();
    init_iic();
    
    reset_spi();
    SPI0_CFG = 0x0FC27;
    SPI0_EN = 1;
    WRITE_SPI(0x10,0xC0);
    WRITE_SPI(0x13,0x38);

    for(;;){
        UARTSendString("\r\n");
        if(BTN_DATA & 0x1){
            //////////////////////////////////////
            // Read temperature through SPI
            //////////////////////////////////////
            SPI_LO = (int8_t)READ_SPI(LSM9DS1_Temp_G_low);
            SPI_HI = (int8_t)READ_SPI(LSM9DS1_Temp_G_high);
            SPI_Temp = (SPI_HI << 8) | SPI_LO;
            SPI_TempC = 25 + (SPI_Temp / 16);
            // currentTemp = read_temp_spi();
            display_num(SPI_TempC);
            sprintf(buffer, "Current temperature is %d C°, according to LSM9DS1 sensor. ", SPI_TempC);
            //////////////////////////////////////
        } else {
            //////////////////////////////////////
            // Read temperature through I2C
            //////////////////////////////////////
            currentTemp = read_temp_iic();
            display_num(currentTemp);
            sprintf(buffer, "Current temperature is %d C°, according to LM75B sensor. ", currentTemp);
            //////////////////////////////////////
        }
        UARTSendString(buffer);
        UARTSendString("\r\n");
        zAxisL = READ_SPI(0x1C); 
        zAxisH = READ_SPI(0x1D); 
        zAxis = (zAxisH << 8) | zAxisL;
        sprintf(buffer, "Z-Axis angular rate: %d ", zAxis);
        UARTSendString(buffer);
        UARTSendString("\r\n----------------------------------------");
        sleep(1);
    }
}

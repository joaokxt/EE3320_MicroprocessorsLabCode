#include <stdint.h> // For uint16_t
#include <stdio.h>  // For sprintf(); usleep()
#include "sleep.h"

// Memory-mapped register definitions
#define SEG_CTL *((uint32_t *)0x43c10000)           // 7-segment control register
#define SEG_DATA *((uint32_t *)0x43C10004)          // 7-segment digit data register
#define Xadc_Data *((uint32_t *)0x43c5020C)         // VP input for the ADC
#define Xadc_Cfg *((uint32_t *)0x43c50300)          // VP input for the ADC

//
// Servo Defines
//
#define TTC0_ClkCntl_0 *((uint32_t *)0xF8001000)     // Config register
#define TTC0_OpMode_0 *((uint32_t *)0xF800100C)      // Operating Mode Configuration
#define TTC0_Interval_0 *((uint32_t *)0xF8001024)    // Interval; count
#define TTC0_Match_0 *((uint32_t *)0xF8001030)       // Match Count
#define TTC0_InterruptEn_0 *((uint32_t *)0xF8001060) // Interrupt Enable
#define TTC0_EvntCntl_0 *((uint32_t *)0xF800106C)    // Event Control

#define sleep_delay 10000 


/*
    init_servo() initializes the servo motor by setting its registers
    Most of the code was taken from M. Welker's sample code
*/

void init_servo(){
    TTC0_OpMode_0 = 0x11;   // Make sure it is off before we program it.
    usleep(2000);        
    TTC0_ClkCntl_0 = 0x13;  // CS = 0 (111MHz Clock), Prescale = 0111 (Div clock by 2^(7+1) = 1024), EN = 1
    usleep(2000);            
    TTC0_Interval_0 = 2160; // This resets counter every 2160 counts.
    usleep(2000);           

    // Counter frequency: (111MHz)/1024 = 108kHz. Reset frequency: (108kHz)/2160 = 50Hz. Reset period: 1/(50Hz) = 20ms

    TTC0_Match_0 = 162;     // 162. This sets it to a 1.5mS pulse width (50 min, 275 max)
    TTC0_InterruptEn_0 = 0; // Do not generate any interrupts
    TTC0_EvntCntl_0 = 0;    // Do not count events
    usleep(2000); 
    TTC0_OpMode_0 = 0x4A;
    // PL = 1, OW = 1, CR = 1 (reset the counter), ME = 1 Match enable DC = 0, count up, IM = 1 OD = 0count enable
}

void set_servo(float reading, int max, int min){
    int match_value;
    int range;

    range = max - min;                      // Subtract PWM's maximum value from the minimum to calculate its range

    match_value =  max - range * reading;   // Determine match by subtracting from max

    TTC0_Match_0 = match_value;             // Write into PWM match register.
}

void display_num(uint16_t n) {
    int i = 0;
    uint32_t temp = 0;

    int decomposedNumber[4];                // Decompose number in powers of 10

    decomposedNumber[0] = n%10;             // ones
    decomposedNumber[1] = (n/10)%10;        // tens
    decomposedNumber[2] = (n/100)%10;       // hundreds
    decomposedNumber[3] = (n/1000)%10;      // thousands

    for(i=0; i<4; i++){
        // Grab digit and shift it into its corresponding bitfield (each digit spans 1 byte)
        temp |= decomposedNumber[i] << (i*8);
    }

    // Disable decimal points (except central dp)
    // Each byte's bit 7 corresponds to the dp. Because it's active low, we use 0x80 = 1000 0000.
    temp |= 0x80008080;

    SEG_CTL = 1;

    SEG_DATA = temp;
}

float read_potentiometer(){
    uint16_t potentiometer_data;
    float voltage_data;

    Xadc_Cfg = 0x0803;                                           // Point to read the potentiometer (VO: 0011) 256 samples. with settlein.

    potentiometer_data = Xadc_Data;                              // Get current data on ADC

    potentiometer_data >>= 4;                                    // Bits 0-3 are unused.
    potentiometer_data &= 0x0FFF;                                // Make sure only 12 bits are read and not the entire register.
    voltage_data = (float)potentiometer_data / 4096.0f;          // ARM reads voltage data with a resolution of 3/4096 V per bit 
    voltage_data = (float)(int)(voltage_data * 100.0f) / 100.0f; // Only read two decimal places

    return voltage_data;
} 

int main(void){
    float voltage;
    uint16_t screenNumber;

    init_servo();
    sleep(2);

    for(;;){
        voltage = read_potentiometer();
        set_servo(voltage, 275, 50);

        screenNumber = (int)(voltage*100);
        display_num(screenNumber);

        usleep(sleep_delay);
    }

    return 0;
}
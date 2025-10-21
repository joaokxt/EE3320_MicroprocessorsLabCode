// M Welker Attempt to convert lab 1 from assembly to C.
// Modified by Joao Klein X T to implement "inverter" pushbutton

#include <stdint.h>

#define LED_DATA_ADDR	0x41210000 	// LED are the lower 10 bits
#define SW_DATA         0x41220000  // switches are the lower 12 bits
#define Button_Data     0x41200000 	// buttons are the lower 4 bits



 int main (void) {

		uint32_t *dataLED, *dataSW, *dataBut;
		uint32_t valSW;

		dataLED = (uint32_t *)LED_DATA_ADDR;	//set the address of dataLED to led control
		dataSW = (uint32_t *)SW_DATA;			//set the address of dataSW to Switches
		dataBut = (uint32_t *)Button_Data;		//set the address of dataSW to Buttons

		while(1){

		valSW = *dataSW; 	// read the switches
		if(*dataBut & 0x1){	// read the first pushbutton
			valSW = ~valSW;	// invert the switch data
		}
		*dataLED = 0;		// write '0' to the LED's
		*dataLED = valSW;	// assign the switch data into the LED register

		}



    return 0;
}

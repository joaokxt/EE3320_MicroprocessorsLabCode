#include <stdint.h>
#include <unistd.h>

#define SEG_CTL *((uint32_t *) 0x43C10000)
#define SEG_DATA *((uint32_t *) 0x43C10004)
#define BTN_DATA *((uint32_t *) 0x41200000)

/*
    Function written by Joao Klein Terck
    Inputs: unsigned 16-bit integer n. This number contains the digits to be displayed
    Outputs: none (void)

    This function takes the number to be displayed (in MM:SS format) and decomposes it in powers of 10 to retrieve its digits
    The digits should be a nibble long and they are shifted into its corresponding bitfield
    They are all packed in an unsigned 32-bit integer that will be setn to the data register of the 7-segment
    The packet is masked with 0x80808080 to disable d.p's (active low)

*/

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

    // Disable decimal points 
    // Each byte's bit 7 corresponds to the dp. Because it's active low, we use 0x80 = 1000 0000.
    temp |= 0x80808080;

    SEG_CTL = 1;

    SEG_DATA = temp;
}

int main(void){
    int stopwatchCounting = 0;
    int miliSecondsElapsed = 0;

    uint16_t totalCount = 0;

    uint8_t seconds = 0;
    uint8_t minutes = 0;

    for(;;){
        usleep(1000);                      // Sleep for only 1000us = 1ms
        miliSecondsElapsed += 1;           // Count how many ms have been slept

        // Only count if counting s is enabled and a whole second has passed
        if(stopwatchCounting == 1 && miliSecondsElapsed >= 1000){
            seconds++;

            // Start counting minutes when 60s have passed
            if(seconds == 60){
                seconds = 0;
                minutes++;
            }

            // Cut at 100min to avoid overflow
            if(minutes == 100){
                minutes = 0;
                seconds = 0;
            }

        }

        if(BTN_DATA&0x1){           // Read button 1 (001) to start
            stopwatchCounting = 1;
        }
        if(BTN_DATA&0x2){           // Read button 2 (010) to stop
            stopwatchCounting = 0;
        }
        if(BTN_DATA&0x4){           // Read button 3 (100) to reset if stopped
            if(stopwatchCounting == 0){
                seconds = 0;
                minutes = 0;
            }
        }
        
        totalCount = (minutes*100) + seconds;

        display_num(totalCount);

        if(miliSecondsElapsed >= 1000){
            miliSecondsElapsed = 0;
        }

        return 0;
    }
}
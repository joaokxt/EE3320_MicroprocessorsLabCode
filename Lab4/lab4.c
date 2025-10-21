#include <stdint.h>
#include <unistd.h>
#include <stdio.h>

#define LED_Data            *((uint32_t *)0x41210000)
#define Switch_Data         *((uint32_t *)0x41220000)  

#define Bank0_Input         *((uint32_t *)0xE000A060) 
#define Bank0_Output        *((uint32_t *)0xE000A040) 
#define Bank0_Dir     	    *((uint32_t *)0xE000A204) 
#define Bank0_Enable        *((uint32_t *)0xE000A208) 

#define Bank2_Input         *((uint32_t *)0xE000A068)
#define Bank2_Output        *((uint32_t *)0xE000A048)
#define Bank2_Dir           *((uint32_t *)0xE000A284)
#define Bank2_Enable        *((uint32_t *)0xE000A288)

void ConfigurePins(){
    Bank2_Dir = 0;

    Bank2_Dir |= 0x00078000;        // Set bits 15-18 as outputs (1)
    Bank2_Dir &= ~0x00780000;       // Make sure 19-22 are inputs (0)
   

    Bank2_Enable = 0x007F8000;      // Enable all bits
    Bank2_Output = 0;
}

void ConfigureLED(){
    LED_Data = 0;
}

void ShowColor(uint32_t msg){
    Bank0_Dir = 0;
	Bank0_Enable = 0;
	Bank0_Output = 0;

    if(msg & 0x01){
        Bank0_Dir = 0x20000;    // Red
	    Bank0_Enable = 0x20000;
	    Bank0_Output = 0x20000;
    } else if (msg & 0x02){
        Bank0_Dir = 0x60000;    // Yellow
	    Bank0_Enable = 0x60000;
	    Bank0_Output = 0x60000;
    } else if (msg & 0x04){
        Bank0_Dir = 0x40000;    // Green
	    Bank0_Enable = 0x40000;
	    Bank0_Output = 0x40000; 
    }
}

void Send4BitMessage(uint8_t msg){
    Bank2_Output = 0;
    for(int i = 0; i < 4; i++){
        if((msg >> i) & 0x01){
            Bank2_Output |= 0x1 << (i + 15);
        }
    }
}

uint32_t Read4BitMessage(){
    uint32_t inputTemp = 0;
    inputTemp = Bank2_Input & 0x00780000;
    return (inputTemp >> 19) & 0x0F;
}


int main(void){
    uint32_t temp;

    ConfigurePins();
    ConfigureLED();

    // 0x01 --> Red
    // 0x02 --> Yellow
    // 0x04 --> Green
 
    while(1){
        LED_Data = 0;
        LED_Data = Switch_Data;

        if(Switch_Data & 0x01){
            ShowColor(0x01);
            Send4BitMessage(0x01);
            sleep(2);
            ShowColor(0x04);
            Send4BitMessage(0x01);
            sleep(2);
            ShowColor(0x02);
            Send4BitMessage(0x01);
            sleep(1);
            ShowColor(0x01);
            Send4BitMessage(0x01);
            sleep(2);
            ShowColor(0x01);
            Send4BitMessage(0x04);
            sleep(2);
            ShowColor(0x01);
            Send4BitMessage(0x02);
            sleep(1);
            ShowColor(0x01);
            Send4BitMessage(0x01);
        } else {
            temp = Read4BitMessage();
            ShowColor(temp);
        }
    }
}
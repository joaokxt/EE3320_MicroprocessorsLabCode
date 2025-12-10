#include <stdint.h>
#include <stdio.h>
#include "sleep.h"

#define UART1_CR *((uint32_t *) 0xE0001000)
#define UART1_MR *((uint32_t *) 0xE0001004)
#define UART1_BAUDGEN *((uint32_t *) 0xE0001018)
#define UART1_BAUDDIV *((uint32_t *) 0xE0001034)

#define UART1_SR *((uint32_t *) 0xE000102C)
#define UART1_DATA *((uint32_t *) 0xE0001030)

#define BaudGen115200 0x7c
#define BaudDiv115200 6

/*
    Setup code
    Taken from lecture materials (UART-10.pptx)
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
    UART1_BAUDGEN = BaudGen115200;   // Set BAUDGEN to define RX sampling clock. RXD_C = 100MHz / BAUDGEN
    UART1_BAUDDIV = BaudDiv115200;   // Set BAUDDIV to define TX clock. TXD_C = RXD_C / (BAUDDIV + 1)
}

void ConfigureUart(){

  /*
    Configure to 8N1
    Mode Register, from LSB to MSB:
        00 --> Master clock 0 (100MHz)
        0x --> 8 bits
        100 --> No parity
        00 --> 1 stop bit
        0 --> Channel mode normal (no echo)
        Other bits --> UNUSED

    Pack this together and we need to pass the following to the mode register
    00100000 = 0x20
  */

  UART1_MR = 0x20;    

  uint32_t UARTtemp; 
  UARTtemp = UART1_CR;  // Copy current config
  UARTtemp |= 0x14;     // Mask 0001 0100 (TxE and RxE set to '1')
  UART1_CR = UARTtemp;  // Pass new config
}

void InitUart(){        // Initialize UART by resetting it, setting the Baudrate and passing 8N1 config.
    ResetUart();
    SetBaud();
    ConfigureUart();
}

/*
    Code for sending and receiving data in UART
    Made by Joao Klein T.
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
    while(UARTEmptyRx()){sleep(2);} // Wait for FIFO to fill up;
    return (char) UART1_DATA;       // Retrieve data and cast to char
}

/*
    Code to extract number from input
    Made by Joao Klein T.
*/

int GetInt(){
    int i = 0, j = 0;               // Counters
    int target = 0;                 // Number that will be returned
    int temp;                       // Used for calculations
    char numString[16];             // Store characters in this string
    char c = 'a';

    while(c != 0x0d){
        c = UARTGetChar();          // Get next character in FIFO. UART sends LSB first.
    
        if((c < 0x30 || c > 0x39) && !(c == 0x0d || c == 0x00)){
            return -1;              // Check if character is valid, otherwise return -1
        }
        if(c == 0x00 || c == 0x0d){break;}  
                                    // If NULL or ENTER was reached, stop processing
        numString[i] = c;           // Store extracted character in string
        i++;                      
    }

    while(j < i){                     // Stop when j = i, otherwise NULL will be read
        c = numString[j];             // Retrieve first character of string
        temp = c - 0x30;              // Convert it to number
        target = 10 * target + temp;  // Shift it into its right position
        j++;                        
    }

    return target;
}

/*
    Prime-number checker (can be optimized)
    Made by Joao Klein T.
*/

int IsPrime(int n){

    int divideCounter = 0;          // Counter for how many divisions were made

    if(n <= 2){                     // If number smaller than 2, stop
        return 1;
    }
    for(int i = 1; i <= n; i++){    // For each number smaller than n, check if n can be divided by it
        if(n % i == 0){
            divideCounter++;        // If so, add one to counter
        }
    }

    if(divideCounter > 2){          
        return 0;                   // Number not prime (more than 2 divisors)
    } else {
        return 1;                   // Number is prime (only 2 divisors)
    }
}

/*
    main() function
*/

int main(void){
    int number = 0;
    char buffer[64];

    InitUart();                     // Initialize UART1

    for(;;){
    while(number < 2){
        UARTSendString("Enter a number larger than 2:\t");
                                    // Ask for input
    
        number = GetInt();          // Retrieve number input by user

        UARTSendString("\r\n");

        sprintf(buffer, "User input: %d\n", number);
        UARTSendString(buffer);

        if(number < 2){
            UARTSendString("Invalid number!\t");
        }
    }

    while(!IsPrime(number)){
        number--;                  // Keep checking if number is prime until it is found
    }
    
    sprintf(buffer, "The nearest prime number is %d\n", number);
    UARTSendString(buffer);

    number = 0;
    }

    return 0;
}
/* 
 * File:   Xyloband_Tx_v1.c
 * Author: Rodders
 * Created on 10th August 2016
 * Config Control   10/8/2016 v1 - xxxxx
 *
 * Purpose: to activate the MRF89XAM8A-I/RM modules to enable the Xyloband to glow.
 *
 * Target PIC is PIC16F1789
 * 
 * see roddersblog.wordpress.com
 * 
 * twitter: rbell_uk
 */

#include <xc.h>
#include "config.h"
//#include <stdio.h>
//#include <stdlib.h>
#include <stdbool.h>			// Boolean (true/false) definition
//#include	"math.h"

#include "LCD_Module_1.h"
#define	_XTAL_FREQ	8000000		// Variable used for time delays only
#define BufferIsFull SSP1STAT.BF
#define LATCH_OE_TEST LATAbits,LATA3

/*
Define macros like so:
#define RD5  LATDbits.LATD5
#define RD4 LATDbits.LATD4
 
then in the code I can simply say:
{
   ...

   RD5=0; //set RD5 to output low
   RD4=1; //set RD4 to output high
   ...
}
*/

void flash(int intFlashes);
void initialPic(void);
void SPI_Init(void);
void RxTx_init(void);
void SPI_Config_Write(char,char);
void SPI_Data_Write(char);



char SPI_Config_Read(char);
char SPI_Data_Read(void);

const char* ConvertByteToString(char);

//global variables
//int xxx;
//unsigned short xxx

char Temp;

char Period_Count;                  //this flag increments on each 40us tick
bool boolFinished;                  //this flag keeps track of the end of the PWM Period
const char* myText;



// ********************************************************************
void interrupt isr(void){

    GIE=0;
        
    //find which interrupt using the flags and jump to service that particular interrupt
  
    //finish 
    GIE=1;
}

// ********************************************************************
//void interrupt low_isr(void){ //only if device has two priority levels???

//}

// ********************************************************************

void initialPIC(void)
{   // Initialize user ports and peripherals.
    //clock set up; 8MHz
    OSCCONbits.IRCF0=0;
    OSCCONbits.IRCF1=1;
    OSCCONbits.IRCF2=1;
    OSCCONbits.IRCF3=1;
    OSCCONbits.SCS0=false;
    OSCCONbits.SCS1=false;

//    OSCTUNE? - not needed
    OSCCONbits.SPLLEN = 0;   //x4 clock even if config file sets pllen=0

    // Set port directions for I/O pins: 0 = Output, 1 = Input
    //PortA
    ANSELA = 0b00000000;			// Make all PORTA pins digital I/O
    TRISA = 0b00000001;			// PWM RA0-RA7 = x8 pins    RA0 is LCD BF input
    LATA = 0x00;
    
    //PortB
    ANSELB = 0b00000000;
    TRISB = 0b11000000;			// PWM RB0-RB5 = x6 pins    Keep RB6 and RB7 clear as ICSP only                   // Change RB7 and RB6 to SPI inputs

    //PortC
    LATC=0x00;     
    ANSELC = 0x00;
    TRISC = 0b00010000;                     // for SPI to RF Module

    OPTION_REGbits.nWPUEN=1;
    WPUC = 0b00010000;
    INLVLC=0x00;          //schmitt triggers 0=off
    ODCONC=0x00;
 
    //setup for which pins are SCK and SDI and SS
    APFCON1=0b01000111;
    APFCON2=0x00;
    
    //PortD
    ANSELD = 0b00000000;
    WPUD=0b00000000;            //disable all weak pull-ups
    ODCOND=0b00000000;          //pull-pull output
    TRISD = 0b00000000;			// PWM RD0-RD7 = x8 pins

    //PortE
    ANSELE = 0b00000000;
    TRISE=0b00000000;                       // PWM RE0-RE1 = x2 pins

    PSMC1CON=0x00;
    PSMC1OEN=0x00; 



    //TMR2 Setup
    //uses Fosc/4 = 16Mhz
    T2CON=0b00000001;          //pre-scale to 1/4
    PR2=80;                    //test increment 256 steps
    T2CONbits.TMR2ON=true;
        
    // Interrupt on Change, IOC
    IOCAP=0x00000000;       //portA, Positive edge    ....caution as using CD4011B
    IOCAN=0x00000000;       //portA, Positive edge    ....caution as using CD4011B
    IOCAF=0;

    PIE1bits.SSP1IE=true;

    INTCONbits.IOCIE=0;          //activate interrupts now, otherwise interrupts happen but are just queued up for GIE to be enabled.
    PIE1bits.TMR2IE=false;        //eg interrupt events are still logged when GIE is clear
                                    // take off TMR2 as 40us 'ticks' were for the PWM board.

     
    //INTCON setup
    GIE=0;
    PEIE=1;

    //       NB, IPEN doesn't exist on 16F1789
}

// ********************************************************************
void SPI_Init(void)
{
    //SPI_CLK = 0; //output
    //SPI_SDI = 1; //input
    //SPI_SDO = 0; //output
    //SPI_SS = 0; //output
    SSP1CON1=0b00000010;                //fosc/64,SPI Master Mode
    SSP1STAT=0b01000000;
    
    //SSPCON3
    //SSPBUF
    //SSPADD

    PIE1bits.SSP1IE=true;

    SSP1CON1bits.SSPEN = 1;
    PIR1bits.SSP1IF= 0; // Clear interrupt flag
}


    
    
    
// ********************************************************************
void RxTx_Init(void)
{
    char charByte;
    //REG 00:GCONREG
    charByte=0b10010110;                // 100 Tx Mode
                                        // 10 863-870MHz
                                        // 11 Tune 180mV
                                        // R1/P1/S1 set is used
    
    SPI_Config_Write(0,charByte);
    
    //REG 0x01: DMODREG
    charByte=0b10101111;     //10=FSK, packet mode, -13.5dBm buffer gain
    SPI_Config_Write(0b00000001,charByte);
    
    //REG 0x02: FDEVREG single side frequency deviation...set to 30kHz
    charByte=0x13;                    //20kHz=.19
    SPI_Config_Write(2,charByte);    
    
    //REG 0x03: BRSREG bit rate set register...set to 10kbits
    charByte=0x13;
    SPI_Config_Write(3,charByte);
    
    //REG 0x04: FLTHREG

    //REG 0x05: FIFOCREG
    charByte=0b11111111;                //x64 byte FIFO, x63/4 byte threshold
    SPI_Config_Write(0x05,charByte);    
    
    
    //REG 0x0C: PACREG
    
    //REG 0x0D: FTXRXIREG
    charByte=0b10100001;                //FIFO Tx and Rx IRQ Setup
    SPI_Config_Write(0x0D,charByte); 
    
    //REG 0xxx: FTPRIREG
    
    //REG 0xxx: RSTHIREG
    
    //REG 0xxx: FILCREG
    

    //REG 0x1A: TXCONREG - set for 200KHz cut off and -8dBm output power
    charByte=0b01111110;
    SPI_Config_Write(26,charByte); 
    
    //REG 0x06: R1CREG
    charByte=119;
    SPI_Config_Write(6,charByte);
    
    //REG 0x07: P1CREG
    charByte=95;
    SPI_Config_Write(7,charByte); 
    
    //REG 0x08: S1CREG
    charByte=46;
    SPI_Config_Write(8,charByte);
    
    
    
    
    //REG 0x09: R2CREG
    charByte=119;
    SPI_Config_Write(9,charByte); 
    
    //REG 0x0A: P2CREG
    charByte=94;
    SPI_Config_Write(10,charByte); 
        
    //REG 0x0B: S2CREG
    charByte=120;
    SPI_Config_Write(11,charByte); 
        
    //REG 0xxx: CLKOUTREG
    
    
    //PLOADREG,NADDSREG,PKTCREG and FCRCREG registers
    
    //clear FRWAXS bit for FIFO write access - default
    
    //REG 0x1C: PLOADREG
    charByte=0b10000100;                   //Enable Manchester Encoding of Payload, 3x bytes long
    SPI_Config_Write(0x1C,charByte);
    
    //REG 0x1E: PKTCREG
    charByte=0b01101000;                   //Packet Config...Fixed len, preamble bytes=4 (max))
    SPI_Config_Write(0x1E,charByte);    

    //REG 0x12: SYNCREG
    charByte=0b10001110;                   //Sync Byte setup
    SPI_Config_Write(0x12,charByte);   

    //REG 0x16: SYNCREG
    charByte=0x2D;                   //SYnc Byte setup
    SPI_Config_Write(0x16,charByte);
    
    //REG 0x17: SYNCREG
    charByte=0xD4;                   //SYnc Byte setup
    SPI_Config_Write(0x17,charByte);
}

//Write Register Sequence - e.g. set up the control registers
void SPI_Config_Write(char AddressByte,char DataByte){
    char Rubbish;
    
        LATBbits,LATB0=0;               //CSCON
        LATBbits,LATB1=1;               //CSDATA
        
        SSP1STAT,BF=0;
        PIR1,SSP1IF=0;
        
        //send first byte=ADDRESS
        if (AddressByte>31) {
            AddressByte=31;
        }
        AddressByte=AddressByte<<1; //should get the A0-A4 in the right place, ;leaving LSB clear e.g. stop bit, as well as first two bits.  
        
        SSPBUF=AddressByte;
        while(PIR1,SSP1IF==0){
        }
        Rubbish=SSPBUF; //force read of SSPBUF, eg data from the SLAVE, and throw away. This resets BF.
        PIR1,SSP1IF=0;
        
        //send second byte=DATA
        SSPBUF=DataByte;
        while(PIR1,SSP1IF==0){
        }
        Rubbish=SSPBUF; //force read of SSPBUF, eg data from the SLAVE, and throw away. This resets BF.
        PIR1,SSP1IF=0;


//        while(PIR1,SSP1IF==0){
//        }
//        Rubbish=SSPBUF; //force read of SSPBUF, eg data from the SLAVE, and throw away. This resets BF.
//        PIR1,SSP1IF=0;


        
        LATBbits,LATB0=1;               //CSCON
        LATBbits,LATB1=1;               //CSDATA    
}

//Write Bytes Sequence - eg values to the FIFO
void SPI_Data_Write(char DataByte){   
    char Rubbish;
    
        LATBbits,LATB0=1;               //CSCON
        LATBbits,LATB1=0;               //CSDATA  
    
        SSP1STAT,BF=0;
        PIR1,SSP1IF=0;
        
        //send first byte=ADDRESS
        SSPBUF=DataByte;
        while(PIR1,SSP1IF==0){
        }
        Rubbish=SSPBUF; //force read of SSPBUF, eg data from the SLAVE, and throw away. This resets BF.
        PIR1,SSP1IF=0;       
      
        LATBbits,LATB0=1;               //CSCON
        LATBbits,LATB1=1;               //CSDATA 
    
}

//Read Register Sequence - e.g. read the control registers
char SPI_Config_Read(char RegAddress){

    char Rubbish;
    char DataRead;
    
        if (RegAddress>31) {
            RegAddress=31;
        }    
        LATBbits,LATB0=0;               //CSCON
        LATBbits,LATB1=1;               //CSDATA
        
        SSP1STAT,BF=0;
        PIR1,SSP1IF=0;
        
        //send first byte=ADDRESS
        RegAddress=RegAddress<<1;        //should get the A0-A4 in the right place, ;leaving LSB clear e.g. stop bit, as well as first two bits.   
        RegAddress=RegAddress | 0b01000000;   //set rw bit of the SPI stream
        SSPBUF=RegAddress;

        while(PIR1,SSP1IF==0){
        }
//        Rubbish=SSPBUF; //force read of SSPBUF, eg data from the SLAVE, and throw away. This resets BF.
//        PIR1,SSP1IF=0;
        
        //send second byte=DATA
        SSPBUF=Rubbish;
        while(PIR1,SSP1IF==0){
        }
        DataRead=SSPBUF; //force read of SSPBUF, eg data from the SLAVE, and throw away. This resets BF.
        PIR1,SSP1IF=0;

        while(PIR1,SSP1IF==0){
        }
        Rubbish=SSPBUF; //force read of SSPBUF, eg data from the SLAVE, and throw away. This resets BF.        
        
        LATBbits,LATB0=1;               //CSCON
        LATBbits,LATB1=1;               //CSDATA 
    
    return(DataRead);    
}

char SPI_Data_Read(){

        
    
    return(0);     
}
// ********************************************************************

// ********************************************************************
void flash(int intFlashes)
// as a test utility
{
    for (int j=0; j< intFlashes;j++)
    {
                    // Turn all LEDs on and wait for a time delay
                    LATAbits.LATA1=1;
                    for ( int i = 0; i < 100; i++ ) __delay_ms( 1 ); //see http://www.microchip.com/forums/m673703-p2.aspx
                    // On the PIC18F series, the __delay_ms macro is limited to a delay of 179,200
                    // instruction cycles (just under 15ms @ 48MHz clock).

                    // Turn all LEDs off and wait for a time delay
                    LATAbits.LATA1=0;
                    for ( int i = 0; i < 100; i++ ) __delay_ms( 9 );
    }
}


//To write a 16 character string to one of the two LCD lines
void LCD_write(const char *String, bool bLine){
    int iPointer;
    char cCounter;
    bool bAddress;
    if (bLine==0) {
        LCD_busy();
        LCD_clear();
    }
    else{
        LCD_DDRAM(0xC0);
    }
    
    for ( iPointer = 0; iPointer<16; iPointer++ ) {
        LCD_busy();
        LCD_Data(String[iPointer]); 
    }
    
}


const char* ConvertByteToString(char Value){

    char NibbleL;
    char NibbleH;
    static char OutputString[16];
    
    OutputString[0]='0';
    OutputString[1]='x'; 
    
        NibbleL = Value & 0b00001111;
        if (NibbleL < 10) {
            OutputString[3]=NibbleL+48;
        }    else{
            OutputString[3]=NibbleL+55;
        }
        
        Value=Value>>4;
        NibbleH=Value & 0b00001111;

        if (NibbleH < 10) {
            OutputString[2]=NibbleH+48;
        }    
        else{
            OutputString[2]=NibbleH+55;
        }
        
        for ( int i = 4; i < 16; i++ ) OutputString[i]=' ';
        
        return OutputString;

}



int main(int argc, char** argv) {
       //float i, a;
       //char v;
       //int Counter=0;

       //set up hardware
    
char RegValue;
char TestNo;
char intByte1;
char intByte2;
char intByte;
char intSync;

       initialPIC();
       flash(2); 
              
       LATCbits,LATC1=1;
       LATCbits,LATC2=1;
       SPI_Init();
       
       initialLCD();
       LCD_clear();
       

        myText="Xyloband v3.0   ";
        LCD_write(myText,0);
        myText="by Rodders      ";
        LCD_write(myText,1);        
        flash(1);
        LCD_clear();
               
        myText="Initialising the";
        LCD_write(myText,0);
        myText="MRF89XAM8A......";
        LCD_write(myText,1);        
        flash(1);
        LCD_clear();
       
//        while(1){
//            SPI_Config_Write(2,0xF5);
//            __delay_ms(100);
//            SPI_Config_Write(4,0xF5);
//            __delay_ms(100);            
//            SPI_Config_Write(8,0xF5);
//            __delay_ms(100);
//            SPI_Config_Write(16,0xF5);
//            __delay_ms(100);             
//        }       

// Setup via SPI the MRF89XAM8A RF Module....
       RxTx_Init();
       
        myText="SPI Init Finishd";
        LCD_write(myText,0);
        myText="                ";
        LCD_write(myText,1);        
        flash(1);
        LCD_clear(); 
        
        
//read some values back and 
        RegValue=SPI_Config_Read(0x00);
        myText="GCONREG Value = ";
        LCD_write(myText,0);
        myText=ConvertByteToString(RegValue);
        LCD_write(myText,1);        
        flash(1);
        LCD_clear(); 
        
        RegValue=SPI_Config_Read(0x01);
        myText="DMODREG Value = ";
        LCD_write(myText,0);
        myText=ConvertByteToString(RegValue);
        LCD_write(myText,1);        
        flash(1);
        LCD_clear();        
        
//        while(1){
//            
//            
//            RegValue=SPI_Config_Read(0x00);
//            __delay_ms(100);
//            RegValue=SPI_Config_Read(0x01);
//            __delay_ms(100);            
//            RegValue=SPI_Config_Read(2);
//            __delay_ms(100);
//            RegValue=SPI_Config_Read(3);
//            __delay_ms(100);             
//        }         
        
        
        
        
        
       
        for ( int i = 0; i < 100; i++ ) __delay_ms( 20 );
        boolFinished=true;
        GIE=0;

       
//        myText="Writing rnd data";
//        LCD_write(myText,0);
//        myText="to the FIFO reg ";
//        LCD_write(myText,1); 
        
        myText="Xyloband v3.0 ";
        LCD_write(myText,0);
        myText="by Rodders      ";
        LCD_write(myText,1);        

    TestNo=8;
    switch(TestNo){           

    case(1):
    // Test 1.... 
    while(1){
        for ( int i = 0; i < 256; i++ ){        

        SPI_Data_Write(0x01);
        __delay_ms( 1 );

            for ( int j = 0; i < 100; i++ ){    
            SPI_Data_Write(i);
            __delay_ms( 10 );
            }
        }
    }
    break;
    
    case(2):
    // Test 2.... 
    while(1){    
        for ( int i = 0; i < 256; i++ ){        

            for ( int j = 0; i < 100; i++ ){    
                SPI_Data_Write(i);
                SPI_Data_Write(i);
                SPI_Data_Write(i);
                SPI_Data_Write(i);
                __delay_ms( 10 );
            }            
        }
    }
    break;
    
    case(3):
    // Test 3.... 
    while (1) {
    //    for (int BaudRate = 50; BaudRate<256; BaudRate++){

    //        //REG 0x03: BRSREG bit rate set register...set to 8kbits
    //        SPI_Config_Write(3,BaudRate);
    //         __delay_ms( 1 );

        for (int FreqByte = 115; FreqByte<125; FreqByte++){

            //REG 0x03: BRSREG bit rate set register...set to 8kbits
            SPI_Config_Write(6,FreqByte);
             __delay_ms( 1 );

            LCD_clear();         
            myText="Freq    :       ";
            LCD_write(myText,0);
            myText=ConvertByteToString(FreqByte);
            LCD_write(myText,1);        

            for ( int i = 0; i < 256; i++ ){


                for (int j=0; j<10; j++){

                    SPI_Data_Write(i);
                    SPI_Data_Write(i);
                    SPI_Data_Write(i);
                    SPI_Data_Write(i);
                    SPI_Data_Write(i);
                    SPI_Data_Write(i);
                    SPI_Data_Write(i);
                    SPI_Data_Write(i);
                    __delay_ms( 2 );
                }    
            }
        }   

        flash(1);

    } //while
    break;
    
    case(4):
    // Test 4.... 
    while(1){    
        for ( int i = 0; i < 256; i++ ){        

            for ( int j = 0; i < 100; i++ ){    
                SPI_Data_Write(i);
                SPI_Data_Write(i);
                SPI_Data_Write(i);
                SPI_Data_Write(i);
                SPI_Data_Write(i);
                SPI_Data_Write(i);
                SPI_Data_Write(i);
                SPI_Data_Write(i);                
                
                
                __delay_ms( 10 );
            }            
        }
    }
    break;    
 
    case(5):
    // Test 5.... 
    while(1){ 
        for ( int i = 0; i < 8; i++ ){    
            SPI_Data_Write(0xAA);
        }
        
        //Sync Bytes x 2
        SPI_Data_Write(0x4B);
        SPI_Data_Write(0xD4);
        
        //Frames of Packet
        SPI_Data_Write(0x01);
        
        for ( int i = 0; i < 32; i++ ){    
            SPI_Data_Write(i);
        }
        
        
        __delay_ms( 400 );
        
        
    }
    break; 

    case(6):
    // Test 6....just load up the FIFO, when it hits 31x bytes it will be Tx along with the preamble and sync
    while(1){ 

        for (intSync =0; intSync<256; intSync++) {
            //Frames of Packet
            
            SPI_Data_Write(63);
            
            //first 8 bytes could be the lights?
                SPI_Data_Write(intSync);
                SPI_Data_Write(0x5A);
                SPI_Data_Write(0xFF);
                SPI_Data_Write(0xA5);
                SPI_Data_Write(0x00);
                SPI_Data_Write(0xFF);
                SPI_Data_Write(0x53);
                SPI_Data_Write(0xF3);                                                                
            
            for (intByte=0; intByte<56;intByte++){
   
                SPI_Data_Write(intSync);
            
            }
            
            __delay_ms( 400 );
        
        }
        
        
    }
    break; 

    case(7):
    // Test 7....signify 1x byte and then send 1x byte
        intByte2=0;
        while(1){ 

            for (int intByte1 =0; intByte1<256; intByte1++) {
                //Frames of Packet

                //First frame of packet is a single byte detailing length of second frame...
                SPI_Data_Write(0x02);

                //try 2x bytes ?
                SPI_Data_Write(intByte1);
                SPI_Data_Write(intByte2);

            myText=ConvertByteToString(intByte1);
            LCD_write(myText,0);        
            myText=ConvertByteToString(intByte2);
            LCD_write(myText,1);
            
            __delay_ms( 50 );
            
            //LCD_clear(); 

            }
            
            __delay_ms( 800 );
        
            intByte2++;
            if (intByte2==256){
                intByte2=0;
            }
        
    }
    break; 

    case(8):
    // Test 8
        while(1){ 

            for (int intByte1 =0; intByte1<256; intByte1++) {
                //Frames of Packet

                //First frame of packet is a single byte detailing length of second frame...
                SPI_Data_Write(0x03);

                //try 2x bytes ?
                SPI_Data_Write(0xB3);
                SPI_Data_Write(intByte1);
                SPI_Data_Write(intByte1);
                
            myText=ConvertByteToString(intByte1);
            LCD_write(myText,0);        
            //myText=ConvertByteToString(intByte2);
            LCD_write("0xB3",1);
            
            __delay_ms( 100 );
            
            //LCD_clear(); 

            }

        
        
    }
    break;
    
    }  //end switch 
    
    return (0); //(EXIT_SUCCESS);
}


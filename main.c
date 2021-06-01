/*--------------------------------------------------------
GEORGE MASON UNIVERSITY
ECE 447 - ADC12 Lab 8
  Using timer A2 to trigger ADC sampling
  ADC12 samples its LDR 5 times
  and returns average value in global variable temp.

Date:   Fall 2020
Authors: Jens-Peter Kaps, Sarah Fakhry

--------------------------------------------------------*/
#include <stdint.h>

#include <msp430.h>
#include "lcd.h"

float volt;
float volt_freq;
unsigned int temp;
unsigned int volt_bin;
unsigned int buzzer_check;
///////////////////////////////////////////////

// LCD memory map for numeric digits
const char digit[10][2] =
{
    {0xFC, 0x28},  /* "0" LCD segments a+b+c+d+e+f+k+q */
    {0x60, 0x20},  /* "1" */
    {0xDB, 0x00},  /* "2" */
    {0xF3, 0x00},  /* "3" */
    {0x67, 0x00},  /* "4" */
    {0xB7, 0x00},  /* "5" */
    {0xBF, 0x00},  /* "6" */
    {0xE4, 0x00},  /* "7" */
    {0xFF, 0x00},  /* "8" */
    {0xF7, 0x00}   /* "9" */
};

const char digitpos[6] = {pos6, pos5, pos4, pos3, pos2, pos1};

/* number to display, position of decimal point, no leading 0's */
void displayNum(unsigned long num, unsigned int decpoint)
{
    short i, digits;

    if(num > 99999)
        digits=6;
    else
        if(num > 9999)
            digits=5;
        else
            if(num > 999)
                digits = 4;
            else
                if(num > 99)
                    digits = 3;
                else
                    if(num > 9)
                        digits = 2;
                    else
                        digits = 1;
    if(decpoint > 5)
        decpoint=0;
    if(decpoint >= digits)
        digits = decpoint+1;

    for(i=6; i >= digits; i--){
        LCDMEM[digitpos[i]] = 0;   // clearing unused digits
        LCDMEM[digitpos[i]+1] &= 0x02;
    }
    for(i=digits; i!=0; i--){
        showDigit(num%10,digits-i,decpoint);
        num=num/10;
    }
}

/* Displays with a fixed decimal point e.g. 13.4 */
void showDigit(char c, unsigned int position, unsigned int decpoint)
{
    unsigned int mydigit;

    LCDMEM[digitpos[position]] = digit[c][0];
    mydigit = digit[c][1];
    if(position!=0){
        if(position==decpoint)                            // Decimal point position
            mydigit |= BIT0;
        else
            mydigit &= ~BIT0;
    }
    LCDMEM[digitpos[position]+1] = mydigit;
}

int lcd_init()
{
    PJSEL0 = BIT4 | BIT5;                   // For LFXT

    LCDCPCTL0 = 0xFFD0;     // Init. LCD segments 4, 6-15
    LCDCPCTL1 = 0xF83F;     // Init. LCD segments 16-21, 27-31
    LCDCPCTL2 = 0x00F8;     // Init. LCD segments 35-39

    // Disable the GPIO power-on default high-impedance mode
    // to activate previously configured port settings
    PM5CTL0 &= ~LOCKLPM5;

    // Configure LFXT 32kHz crystal
    CSCTL0_H = CSKEY >> 8;                  // Unlock CS registers
    CSCTL4 &= ~LFXTOFF;                     // Enable LFXT
    do
    {
      CSCTL5 &= ~LFXTOFFG;                  // Clear LFXT fault flag
      SFRIFG1 &= ~OFIFG;
    }while (SFRIFG1 & OFIFG);               // Test oscillator fault flag
    CSCTL0_H = 0;                           // Lock CS registers

    // Initialize LCD_C
    // ACLK, Divider = 1, Pre-divider = 16; 4-pin MUX
    LCDCCTL0 = LCDDIV__1 | LCDPRE__16 | LCD4MUX | LCDLP;

    // VLCD generated internally,
    // V2-V4 generated internally, v5 to ground
    // Set VLCD voltage to 2.60v
    // Enable charge pump and select internal reference for it
    LCDCVCTL = VLCD_8 | VLCDREF_0 | LCDCPEN;

    LCDCCPCTL = LCDCPCLKSYNC;               // Clock synchronization enabled

    LCDCMEMCTL = LCDCLRM;                   // Clear LCD memory

    LCDCCTL0 |= LCDON;

    return 0;
}

void lcd_clear()
{
    // Initially, clear all displays.
    LCDCMEMCTL |= LCDCLRM;
}

///////////////////////////////////////////////
int main(void)
{
    WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT
    P1OUT |= BIT1;              // give pullup/down resistor a '1' so it pulls up
    P1DIR |= BIT0;
    P1DIR &= ~BIT1;
    P2DIR |= BIT0;              // P1.0 set as output
    P9DIR &= ~BIT1;
    P1REN |= BIT1;              // enable pullup/down
    P2SEL1 |= BIT0;             // set select for P2.0
    P2SEL0 &= ~BIT0;
    PM5CTL0 &= ~LOCKLPM5;       // Unlock ports from power manager
    P1IES |= BIT1;              // Interrupt on falling edge
    P1IFG &= ~BIT1;             // clear interrupt flag
    P1IE |= BIT1;               // enable interrupt
    lcd_init();
    /* Initialize TA2 */
    TA2CTL = MC_1 | ID_3 | TASSEL_1 | TACLR;  // up mode, divide ACLK by 8
    TA2EX0 = TAIDEX_7;                        // and divide again by 8
    TA2CCR0 = 0x0034;                         // set hex for 1/10s
    TA2CCTL1 = OUTMOD_3;                      // set/reset gives one 1/10s impulse
    TA2CCR1 = 0x0033;                         // can be any value
    TB0CTL = MC_1 | ID_2 | TBSSEL_1 | TBCLR;
    TB0CCTL6 = OUTMOD_3;

    /* Initialize ADC12_A */
    ADC12CTL0 &= ~ADC12ENC;                             // Disable ADC12
    ADC12CTL0 = ADC12SHT0_8 | ADC12MSC |ADC12ON;        // Set sample time, Multisamples
    ADC12CTL1 = ADC12SHS_5 | ADC12SHP | ADC12CONSEQ_1;  // Trigger conversions
    // from timer TA2 CCR1, Enable sample timer, Sequence of Channels
    ADC12CTL3 = ADC12TCMAP;                             // Enable internal sensor
    ADC12MCTL0 = ADC12VRSEL_0 | ADC12INCH_9;            // ADC input ch A30 => P9OUT
    ADC12MCTL1 = ADC12VRSEL_0 | ADC12INCH_9;            // ADC input ch A30 => P9OUT
    ADC12MCTL2 = ADC12VRSEL_0 | ADC12INCH_9;            // ADC input ch A30 => P9OUT
    ADC12MCTL3 = ADC12VRSEL_0 | ADC12INCH_9;            // ADC input ch A30 => P9OUT
    ADC12MCTL4 = ADC12VRSEL_0 | ADC12INCH_9 | ADC12EOS; // ADC input ch A40 => P9OUT, end of sequence
    ADC12IER0 = ADC12IE4;                               // ADC_IFG upon conv result-ADCMEM4
    while(1)
    {
        ADC12CTL0 &= ~ADC12ENC;                     // Toggeling ENC to restart sequence, else have to use
        ADC12CTL0 |= ADC12ENC;                      // CONCEQ_3
        ADC12CTL0 |= ADC12SC;
        __bis_SR_register(LPM0_bits + GIE);         // LPM4 with interrupts enabled
        volt = ((3.3) / 4095) * temp;               // binary to voltage
        volt_freq = ((volt / 3.3) * 2700) + 300;  // voltage to hertz
        volt_bin = volt * 10;
        displayNum(volt_bin, 1);
        TB0CCR0 = 32768 / volt_freq;              // full timer divided by frequency
        TB0CCR6 = 16384 / volt_freq;              // half timer divided by frequency
    }
}

#pragma vector=ADC12_VECTOR
__interrupt void ADC12ISR (void)
{
    switch(__even_in_range(ADC12IV, ADC12IV_ADC12RDYIFG))
    {
    case ADC12IV_NONE:        break;        // Vector  0:  No interrupt
    case ADC12IV_ADC12OVIFG:  break;        // Vector  2:  ADC12MEMx Overflow
    case ADC12IV_ADC12TOVIFG: break;        // Vector  4:  Conversion time overflow
    case ADC12IV_ADC12HIIFG:  break;        // Vector  6:  ADC12BHI
    case ADC12IV_ADC12LOIFG:  break;        // Vector  8:  ADC12BLO
    case ADC12IV_ADC12INIFG:  break;        // Vector 10:  ADC12BIN
    case ADC12IV_ADC12IFG0:   break;        // Vector 12:  ADC12MEM0
    case ADC12IV_ADC12IFG1:   break;        // Vector 14:  ADC12MEM1
    case ADC12IV_ADC12IFG2:   break;        // Vector 16:  ADC12MEM2
    case ADC12IV_ADC12IFG3:   break;        // Vector 18:  ADC12MEM3
    case ADC12IV_ADC12IFG4:
        P1OUT ^= BIT0;                                             // Toggling on-board LED
        temp = ADC12MEM0+ADC12MEM1+ADC12MEM2+ADC12MEM3+ADC12MEM4;  // Move results, IFG is cleared
        temp = temp/5;                                             // Average computation
        __bic_SR_register_on_exit(LPM4_bits);                      // Exit active CPU
        break;                                                     // Vector 20:  ADC12MEM4 Interrupt
    default: break;
    }
}

// Interrupt Service Routine for Port 1
#pragma vector=PORT1_VECTOR       // associate funct. w/ interrupt vector
__interrupt void Port_1(void)     // name of ISR
{
    P1IFG &= ~BIT1;
    if(buzzer_check == 1) {       // checking if buzzer is on
        P2DIR &= ~(BIT0);         // clearing P2.0
    }
    else {
        P2DIR |= BIT0;            // buzzer remains on
    }
    buzzer_check ^= BIT0;         // toggling
}

// This function implements an efficient decimal to binary conversion.
// Note that a potential BCD overflow is not handled. In case this
// is needed, the function's return value as well as the data type
// of "Output" need to be changed from "unsigned int" to
// "unsigned long" and the intrinsics to __bcd_add_long(...).
unsigned int Dec2BCD(unsigned int Value)
{
    unsigned int i;
    unsigned int Output;
    for (i = 16, Output = 0; i; i--)
    // BCD Conversion, 16-Bit
    {
        Output = __bcd_add_short(Output, Output);
        if (Value & 0x8000)
            Output = __bcd_add_short(Output, 1);
        Value <<= 1;
    }
    return Output;
}

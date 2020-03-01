/*
 * TIDA-01362
 *
 * TIDA-01362 Driving a 3 Phase Stepper with a BLDC Motor Driver
 *
 * Copyright (C) 2016 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/
//3phase sinusoidal stepper driver
#include <msp430.h>

// GPIO Port 1 Definitions
#define NC1      	BIT0    // P1.0
#define NC2      	BIT1    // P1.1
#define NC3      	BIT2    // P1.2
#define NC4    		BIT3    // P1.3
#define POT         BIT4    // P1.4
#define PUSH        BIT5    // P1.5
#define LED         BIT6    // P1.6
#define NC5         BIT7    // P1.7

// GPIO Port 2 Definitions
#define EN         	BIT0    // P2.0
#define IN2         BIT1    // P2.1
#define nRESET      BIT2    // P2.2
#define nSLEEP      BIT3    // P2.3
#define IN3         BIT4    // P2.4
#define nCOMPO      BIT5    // P2.5
#define IN1         BIT6    // P2.6
#define nFAULT      BIT7    // P2.7

// Signal Frequency = 10kHz
unsigned int PERIOD = 700;//800;

// LED Timer
unsigned int LED_Count = 0;

// Sine Look Up Table for Stepper
#define SINE_TABLE_SZ   60

//120deg offset between U,V,W
unsigned int PWM_Sine_U = 0;
unsigned int PWM_Sine_V = 20;
unsigned int PWM_Sine_W = 40;

#define SCALE  0
unsigned int counter = 0;

// Function prototypes
void ConfigureAdc(void);

unsigned int ADC_value=0;

/*----------------------------------------------------
 * Look Up Table range from 90% of PERIOD to 10%.
 * If frequency changes then table should change
 * accordingly.
 ----------------------------------------------------*/

unsigned int PWM_Sine_Table[SINE_TABLE_SZ] = {400,437,473,509,544,577,608,637,663,686,706,722,735,743,748,749,746,740,729,714,696,675,650,623,593,561,527,492,455,418,381,344,307,272,238,206,176,149,124,103,85,70,59,53,50,51,56,64,77,93,113,136,162,191,222,255,290,326,362,399};

int main(void)
{
    // Configure CLKs
    // Stop Watchdog Timer
    WDTCTL = WDTPW | WDTHOLD;

    // 16MHz DCO required for 4MHz SMCLK
    DCOCTL = 0x00;
    DCOCTL = CALDCO_16MHZ;
    BCSCTL1 = CALBC1_16MHZ;
    // 2MHz SMCLK required for PWM frequency
    BCSCTL2 |= DIVS_0;
    // ACLK = VLO
    BCSCTL3 |= LFXT1S_0;

    // Configure GPIO
    P1SEL  &= ~(NC1 | NC2 | NC3 | NC4 | POT | PUSH | LED | NC5);
    P1SEL2 &= ~(NC1 | NC2 | NC3 | NC4 | POT | PUSH | LED | NC5);

    P2SEL  &= ~(EN | IN2 | nRESET | nSLEEP | IN3 | nCOMPO | IN1 | nFAULT);
    P2SEL2 &= ~(EN | IN2 | nRESET | nSLEEP | IN3 | nCOMPO | IN1 | nFAULT);

    P1DIR |= LED;
    P2DIR |= (EN | nRESET | nSLEEP);

    P1OUT &= ~LED;
    P2OUT |= (EN | nRESET | nSLEEP);

    // Setup LED Timer
    // Setup Watchdog Timer as an Interval Timer
    WDTCTL = ((0x5A00)+(0x0010)+(0x0008)+(0x0001));
    IE1 |= WDTIE;

    // Setup Timer A0
    // IN1
    TA0CTL = TASSEL_2 | MC_0 | TACLR | ID_1;
    TA0CCTL1 = OUTMOD_6;
    TA0CCR0 = PERIOD;
    TA0CCR1 = PWM_Sine_Table[PWM_Sine_U];
    P2DIR |= IN1;
    P2SEL |= IN1;
    P2SEL2 &= ~(IN1 | BIT7);

    // Setup Timer A1
    // IN2 and IN3
    TA1CTL = TASSEL_2 | MC_0 | TACLR | ID_1;
    TA1CCTL1 = OUTMOD_6;
    TA1CCTL2 = OUTMOD_6;
    TA1CCR0 = PERIOD;
    TA1CCR1 = PWM_Sine_Table[PWM_Sine_V];
    TA1CCR2 = PWM_Sine_Table[PWM_Sine_W];
    P2DIR |= (IN2 | IN3);
    P2SEL |= (IN2 | IN3);
    P2SEL2 &= ~(IN2 | IN3);

//    ConfigureAdc();                 // ADC set-up function call
//    __enable_interrupt();           // Enable interrupts.

    // Start Timers Together
    TA0CTL = TASSEL_2 | MC_3;
    TA1CTL = TASSEL_2 | MC_3;


    while(1)
    {
//        __delay_cycles(1000);               // Wait for ADC Ref to settle
//        ADC10CTL0 |= ENC + ADC10SC;         // Sampling and conversion start
//        __bis_SR_register(CPUOFF + GIE);    // Low Power Mode 0 with interrupts enabled
//        ADC_value = ADC10MEM;
	__bis_SR_register(LPM0_bits + GIE);
    }
}

// Status LED derived from the watchdog timer
#pragma vector=WDT_VECTOR
__interrupt void WatchDog_Timer(void)
{
    /**
	if(P1IN & PUSH)
	{

	}
	else
	{
    LED_Count++;
    if (LED_Count == 100)
    {
        LED_Count = 0;
        P1OUT ^= LED;
    }
    */
//
//    ADC10CTL0 |= ENC + ADC10SC;             // Sampling and conversion start
//    if ((int)ADC10MEM  < 0){
//        LED_Count = 0;
//        P1OUT ^= LED;
//      }

    // Update PWMs
    if(counter >= SCALE){
        PWM_Sine_U++;
        if (PWM_Sine_U == SINE_TABLE_SZ)
            PWM_Sine_U = 0;
        PWM_Sine_V++;
        if (PWM_Sine_V == SINE_TABLE_SZ)
            PWM_Sine_V = 0;
        PWM_Sine_W++;
        if (PWM_Sine_W == SINE_TABLE_SZ)
            PWM_Sine_W = 0;

        // IN1
        TA0CCR1 = PWM_Sine_Table[PWM_Sine_U];
        // IN2
        TA1CCR1 = PWM_Sine_Table[PWM_Sine_V];
        // IN3
        TA1CCR2 = PWM_Sine_Table[PWM_Sine_W];
        counter = 0;
    }else{
        counter++;
    }


//    // IN1
//    TA0CCR1 = 400;
//    // IN2
//    TA1CCR1 = 400;
//    // IN3
//    TA1CCR2 = 800;

}

// ADC10 interrupt service routine
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR (void)
{
    __bic_SR_register_on_exit(CPUOFF);        // Return to active mode }
}

// Function containing ADC set-up
void ConfigureAdc(void)
{

    ADC10CTL1 = INCH_4 + ADC10DIV_3 ;         // Channel 3, ADC10CLK/3
    ADC10CTL0 = SREF_0 + ADC10SHT_3 + ADC10ON + ADC10IE;  // Vcc & Vss as reference, Sample and hold for 64 Clock cycles, ADC on, ADC interrupt enable
    ADC10AE0 |= POT;                         // ADC input enable P1.3
}

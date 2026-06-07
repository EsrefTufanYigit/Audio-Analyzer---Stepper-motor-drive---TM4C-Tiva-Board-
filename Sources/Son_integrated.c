#define PART_TM4C123GH6PM
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "inc/hw_ints.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/adc.h"
#include "driverlib/systick.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/ssi.h"
#include "driverlib/timer.h"
#include "arm_math.h"
#include "arm_const_structs.h"
#include <string.h>
#include "stepper.h"

// Constants
#define SAMPLES 512
#define SAMPLING_FREQ 4000
#define PWM_PERIOD 100 
#define FREQ_THRES_LOW  500
#define FREQ_THRES_HIGH 1500

// LCD Pins (Port A)
#define LCD_DC  GPIO_PIN_4
#define LCD_CE  GPIO_PIN_3
#define LCD_RST GPIO_PIN_6

// Global Variables
q15_t audioBuffer[SAMPLES * 2]; 
q15_t magnitudes[SAMPLES];
volatile uint16_t sampleIdx = 0;
volatile bool bufferReady = false;
volatile int timer0a_test = 0;

q15_t volatile maxAmplitude = 0;
float currentFreq = 0;
uint32_t volatile potThreshold = 0;
int volatile motorDirection = 1; 
// PWM Timing
volatile uint32_t pwmCounter = 0;
volatile int timer1a_test = 0;

// LED state
volatile uint8_t g_ledColor = 0;
volatile bool g_active = false;
extern uint16_t Read_ADC_Assembly(void);

// LCD Helper Functions
void LCD_Write(uint8_t data, bool isCommand) {
    GPIOPinWrite(GPIO_PORTA_BASE, LCD_DC, isCommand ? 0 : LCD_DC);
    GPIOPinWrite(GPIO_PORTA_BASE, LCD_CE, 0);
    SSIDataPut(SSI0_BASE, data);
    while(SSIBusy(SSI0_BASE));
    GPIOPinWrite(GPIO_PORTA_BASE, LCD_CE, LCD_CE);
}
static inline void LCD_SetCursor(uint8_t x, uint8_t y){
    LCD_Write(0x80 | (x % 84), true);
    LCD_Write(0x40 | (y % 6), true);
}
static void LCD_Clear(void){
    LCD_SetCursor(0,0);
    for(int i=0;i<504;i++) LCD_Write(0x00, false);
}

void LCD_Init(void) {
    GPIOPinWrite(GPIO_PORTA_BASE, LCD_RST, 0);
    SysCtlDelay(SysCtlClockGet()/30); // ~3ms
    GPIOPinWrite(GPIO_PORTA_BASE, LCD_RST, LCD_RST);
    LCD_Write(0x21, true); // Extended commands
    LCD_Write(0xBC, true); // Vop (Contrast)
    LCD_Write(0x04, true); // Temp coeff
    LCD_Write(0x13, true); // Bias
    LCD_Write(0x20, true); // Basic commands
    LCD_Write(0x0C, true); // Normal mode
		LCD_Clear();
}




//Font table
const uint8_t Font5x7[96][5] = {
{0x00,0x00,0x00,0x00,0x00}, // ' '
{0x00,0x00,0x5F,0x00,0x00}, // !
{0x00,0x07,0x00,0x07,0x00}, // "
{0x14,0x7F,0x14,0x7F,0x14}, // #
{0x24,0x2A,0x7F,0x2A,0x12}, // $
{0x23,0x13,0x08,0x64,0x62}, // %
{0x36,0x49,0x55,0x22,0x50}, // &
{0x00,0x05,0x03,0x00,0x00}, // '
{0x00,0x1C,0x22,0x41,0x00}, // (
{0x00,0x41,0x22,0x1C,0x00}, // )
{0x14,0x08,0x3E,0x08,0x14}, // *
{0x08,0x08,0x3E,0x08,0x08}, // +
{0x00,0x50,0x30,0x00,0x00}, // ,
{0x08,0x08,0x08,0x08,0x08}, // -
{0x00,0x60,0x60,0x00,0x00}, // .
{0x20,0x10,0x08,0x04,0x02}, // /
{0x3E,0x51,0x49,0x45,0x3E}, // 0
{0x00,0x42,0x7F,0x40,0x00}, // 1
{0x42,0x61,0x51,0x49,0x46}, // 2
{0x21,0x41,0x45,0x4B,0x31}, // 3
{0x18,0x14,0x12,0x7F,0x10}, // 4
{0x27,0x45,0x45,0x45,0x39}, // 5
{0x3C,0x4A,0x49,0x49,0x30}, // 6
{0x01,0x71,0x09,0x05,0x03}, // 7
{0x36,0x49,0x49,0x49,0x36}, // 8
{0x06,0x49,0x49,0x29,0x1E}, // 9
{0x00,0x36,0x36,0x00,0x00}, // :
{0x00,0x56,0x36,0x00,0x00}, // ;
{0x08,0x14,0x22,0x41,0x00}, // <
{0x14,0x14,0x14,0x14,0x14}, // =
{0x00,0x41,0x22,0x14,0x08}, // >
{0x02,0x01,0x51,0x09,0x06}, // ?
{0x32,0x49,0x79,0x41,0x3E}, // @
{0x7E,0x11,0x11,0x11,0x7E}, // A
{0x7F,0x49,0x49,0x49,0x36}, // B
{0x3E,0x41,0x41,0x41,0x22}, // C
{0x7F,0x41,0x41,0x22,0x1C}, // D
{0x7F,0x49,0x49,0x49,0x41}, // E
{0x7F,0x09,0x09,0x09,0x01}, // F
{0x3E,0x41,0x49,0x49,0x7A}, // G
{0x7F,0x08,0x08,0x08,0x7F}, // H
{0x00,0x41,0x7F,0x41,0x00}, // I
{0x20,0x40,0x41,0x3F,0x01}, // J
{0x7F,0x08,0x14,0x22,0x41}, // K
{0x7F,0x40,0x40,0x40,0x40}, // L
{0x7F,0x02,0x0C,0x02,0x7F}, // M
{0x7F,0x04,0x08,0x10,0x7F}, // N
{0x3E,0x41,0x41,0x41,0x3E}, // O
{0x7F,0x09,0x09,0x09,0x06}, // P
{0x3E,0x41,0x51,0x21,0x5E}, // Q
{0x7F,0x09,0x19,0x29,0x46}, // R
{0x46,0x49,0x49,0x49,0x31}, // S
{0x01,0x01,0x7F,0x01,0x01}, // T
{0x3F,0x40,0x40,0x40,0x3F}, // U
{0x1F,0x20,0x40,0x20,0x1F}, // V
{0x7F,0x20,0x18,0x20,0x7F}, // W
{0x63,0x14,0x08,0x14,0x63}, // X
{0x07,0x08,0x70,0x08,0x07}, // Y
{0x61,0x51,0x49,0x45,0x43}, // Z
{0x00,0x7F,0x41,0x41,0x00}, // [
{0x02,0x04,0x08,0x10,0x20}, // '\'
{0x00,0x41,0x41,0x7F,0x00}, // ]
{0x04,0x02,0x01,0x02,0x04}, // ^
{0x40,0x40,0x40,0x40,0x40}, // _
{0x00,0x01,0x02,0x04,0x00}, // `
{0x20,0x54,0x54,0x54,0x78}, // a
{0x7F,0x48,0x44,0x44,0x38}, // b
{0x38,0x44,0x44,0x44,0x20}, // c
{0x38,0x44,0x44,0x48,0x7F}, // d
{0x38,0x54,0x54,0x54,0x18}, // e
{0x08,0x7E,0x09,0x01,0x02}, // f
{0x0C,0x52,0x52,0x52,0x3E}, // g
{0x7F,0x08,0x04,0x04,0x78}, // h
{0x00,0x44,0x7D,0x40,0x00}, // i
{0x20,0x40,0x44,0x3D,0x00}, // j
{0x7F,0x10,0x28,0x44,0x00}, // k
{0x00,0x41,0x7F,0x40,0x00}, // l
{0x7C,0x04,0x18,0x04,0x78}, // m
{0x7C,0x08,0x04,0x04,0x78}, // n
{0x38,0x44,0x44,0x44,0x38}, // o
{0x7C,0x14,0x14,0x14,0x08}, // p
{0x08,0x14,0x14,0x18,0x7C}, // q
{0x7C,0x08,0x04,0x04,0x08}, // r
{0x48,0x54,0x54,0x54,0x20}, // s
{0x04,0x3F,0x44,0x40,0x20}, // t
{0x3C,0x40,0x40,0x20,0x7C}, // u
{0x1C,0x20,0x40,0x20,0x1C}, // v
{0x3C,0x40,0x30,0x40,0x3C}, // w
{0x44,0x28,0x10,0x28,0x44}, // x
{0x0C,0x50,0x50,0x50,0x3C}, // y
{0x44,0x64,0x54,0x4C,0x44}, // z
{0x00,0x08,0x36,0x41,0x00}, // {
{0x00,0x00,0x7F,0x00,0x00}, // |
{0x00,0x41,0x36,0x08,0x00}, // }
{0x08,0x04,0x08,0x10,0x08}  // ~
};

static void LCD_Char(char c){
    if(c < 0x20 || c > 0x7F) c = '?';
    const uint8_t *glyph = Font5x7[c - 0x20];
    for(int i=0;i<5;i++) LCD_Write(glyph[i], false);
    LCD_Write(0x00, false); // 1-col spacing
}

static void LCD_String(const char *s){
    while(*s) LCD_Char(*s++);
}

//ISRs

void SysTick_Handler(void)
{
    if (bufferReady) return;

    ADCProcessorTrigger(ADC0_BASE, 3);
    while(!ADCIntStatus(ADC0_BASE, 3, false));
    ADCIntClear(ADC0_BASE, 3);

    //Assembly will return 4-bit left shifted result
    int32_t rawScaled = (int32_t)Read_ADC_Assembly();

    //Mic DC bias is 1.25V
    const int32_t DC_BIAS_SCALED = (1551 << 4); // 24816

    // Do subtraction in 32-bit signed, then clamp to q15
    int32_t centered = rawScaled - DC_BIAS_SCALED;
		//Clamp to avoid edge cases from shifts
    if (centered > 32767)  centered = 32767;
    if (centered < -32768) centered = -32768;

    audioBuffer[sampleIdx * 2]     = (q15_t)centered;
    audioBuffer[sampleIdx * 2 + 1] = 0;

    sampleIdx++;
    if (sampleIdx >= SAMPLES) {
        sampleIdx = 0;
        bufferReady = true;
    }
}



void TIMER1A_Handler(void) {
    TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);

    //LED PWM Brightness
    pwmCounter = (pwmCounter + 1) % PWM_PERIOD;

    
    uint32_t mag = (uint32_t)maxAmplitude;
    uint32_t dutyCycle = (mag * PWM_PERIOD)/3000; //maxAmplitude was observed to be around 3k when max volume signal was given
    if (dutyCycle > PWM_PERIOD) dutyCycle = PWM_PERIOD;

    if (g_active && dutyCycle > 0 && pwmCounter < dutyCycle) {
        GPIOPinWrite(GPIO_PORTF_BASE, 0x0E, g_ledColor);
    } else {
        GPIOPinWrite(GPIO_PORTF_BASE, 0x0E, 0);
    }
}

void Init_Hardware(void) {
    SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);

    //LCD Control pins
    GPIOPinTypeGPIOOutput(GPIO_PORTA_BASE, LCD_DC | LCD_CE | LCD_RST);
    
    //SPI Pins
    GPIOPinConfigure(GPIO_PA2_SSI0CLK);
    GPIOPinConfigure(GPIO_PA5_SSI0TX);
    GPIOPinTypeSSI(GPIO_PORTA_BASE, GPIO_PIN_2 | GPIO_PIN_5);
    SSIConfigSetExpClk(SSI0_BASE, SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, 1000000, 8);
    SSIEnable(SSI0_BASE);

    //ADC Configuration, PE3 For mic and PE2 for POT
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3 | GPIO_PIN_2);
    ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 0);
    ADCSequenceStepConfigure(ADC0_BASE, 3, 0, ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END);
    ADCSequenceEnable(ADC0_BASE, 3);
    ADCSequenceConfigure(ADC0_BASE, 1, ADC_TRIGGER_PROCESSOR, 1);
    ADCSequenceStepConfigure(ADC0_BASE, 1, 0, ADC_CTL_CH1 | ADC_CTL_IE | ADC_CTL_END);
    ADCSequenceEnable(ADC0_BASE, 1);

    //Port F, buttons and RGB
    HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
    HWREG(GPIO_PORTF_BASE + GPIO_O_CR) |= 0x01;
    GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_0 | GPIO_PIN_4);
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_0 | GPIO_PIN_4, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, 0x0E);
    //Timer1 for LED PWM
    TimerConfigure(TIMER1_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER1_BASE, TIMER_A, (SysCtlClockGet() / 10000) - 1);
    TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    TimerIntEnable(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    IntEnable(INT_TIMER1A);
    TimerEnable(TIMER1_BASE, TIMER_A);
    // 4kHz systick
    SysTickPeriodSet(SysCtlClockGet() / SAMPLING_FREQ);
    SysTickIntEnable();
    SysTickEnable();
}

int main(void) {
    Init_Hardware();
    LCD_Init();

    
    Stepper_Init();
    Stepper_SetDirection(+1);
    Stepper_SetPeriodUs(10000); //Init around 10ms
    Stepper_Enable(true);

    IntMasterEnable();
	  int lcdUpdateCounter = 0;
    
    

    while(1) {
        //Potentiometer polling
			  uint32_t potVal[1];
        ADCProcessorTrigger(ADC0_BASE, 1);
        while(!ADCIntStatus(ADC0_BASE, 1, false));
        ADCIntClear(ADC0_BASE, 1);
        
				ADCSequenceDataGet(ADC0_BASE, 1, potVal);
        potThreshold = potVal[0]& 0x0FFF;

        //Button polling
        if (GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_4) == 0) motorDirection = 1;
        if (GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_0) == 0) motorDirection = -1;

        //Set the direction
        Stepper_SetDirection((int8_t)motorDirection);

        //Amplitude state check to use in logic
        


        if (bufferReady) {
            arm_cfft_q15(&arm_cfft_sR_q15_len512, audioBuffer, 0, 1);
            arm_cmplx_mag_q15(audioBuffer, magnitudes, SAMPLES);
            
            uint32_t peakIndex;
            arm_max_q15(&magnitudes[5], (SAMPLES/2)-6, &maxAmplitude, &peakIndex);
            peakIndex += 5; 
            currentFreq = (float)peakIndex * SAMPLING_FREQ / SAMPLES;
						g_active = (maxAmplitude > (q15_t)potThreshold);

            // Set global LED color variable based on currentFreq
            if (currentFreq < FREQ_THRES_LOW)       g_ledColor = 0x02; // Red
            else if (currentFreq < FREQ_THRES_HIGH) g_ledColor = 0x08; // Green
            else                         g_ledColor = 0x04; // Blue
            // Update motor period, clamp to 5ms / 50ms
            if (g_active) {
                
                float f = currentFreq;

    uint32_t period_us;
    if (f <= 200.0f)       period_us = 50000u;  // min speed
    else if (f >= 2000.0f) period_us = 5000u;   // max speed
    else                   period_us = (uint32_t)(10000000.0f / f);  
                Stepper_SetPeriodUs(period_us);
            }
						lcdUpdateCounter++;
						if (lcdUpdateCounter>= 8) {
							lcdUpdateCounter = 0;
            //LCD update, bufferReady is called with period >125ms so basic counter here achieves 1 second update delay for screen
            char lineBuff[16]; 

            // Row 0: Current Frequency
            LCD_SetCursor(0, 0);
            snprintf(lineBuff, sizeof(lineBuff), "Frq:%4d Hz", (int)currentFreq);
            LCD_String(lineBuff);

            // Row 1: Current Amplitude
            LCD_SetCursor(0, 1);
            snprintf(lineBuff, sizeof(lineBuff), "Amp:%5d", (int)maxAmplitude);
            LCD_String(lineBuff);

            // Row 2: Amplitude Threshold
            LCD_SetCursor(0, 2);
            snprintf(lineBuff, sizeof(lineBuff), "ThA:%5d", (int)potThreshold);
            LCD_String(lineBuff);

            // Row 3: Frequency Low Threshold
            LCD_SetCursor(0, 3);
            snprintf(lineBuff, sizeof(lineBuff), "MinF:%4d", FREQ_THRES_LOW);
            LCD_String(lineBuff);

            // Row 4: Frequency High Threshold
            LCD_SetCursor(0, 4);
            snprintf(lineBuff, sizeof(lineBuff), "MaxF:%4d", FREQ_THRES_HIGH);
            LCD_String(lineBuff);
					}
            
            // Clear buffer flag
            bufferReady = 0;
				}
    }
}
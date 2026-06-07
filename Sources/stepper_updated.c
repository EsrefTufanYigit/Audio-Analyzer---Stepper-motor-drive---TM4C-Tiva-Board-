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

//Defines
#define STEPPER_GPIO_PERIPH   SYSCTL_PERIPH_GPIOB
#define STEPPER_GPIO_BASE     GPIO_PORTB_BASE
#define STEPPER_PINS_MASK     (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3)

#define STEPPER_TIMER_PERIPH  SYSCTL_PERIPH_TIMER0
#define STEPPER_TIMER_BASE    TIMER0_BASE
#define STEPPER_TIMER_TIMEOUT TIMER_TIMA_TIMEOUT

#define STEPPER_TIMER_INT_NUM INT_TIMER0A

//Min step = 5ms
#define STEPPER_MIN_PERIOD_US 5000u

// Systemclock get
static uint32_t g_sysclk_hz = 40000000u;

//Internal state variables
static volatile uint8_t  g_step_idx = 0;
static volatile int8_t   g_dir = +1;
static volatile bool     g_enabled = false;
static volatile uint32_t g_period_us = 10000u; // default 10 ms

static const uint8_t g_seq_fullstep[4] = {
    0x09, // 1001
    0x0C, // 1100
    0x06, // 0110
    0x03  // 0011
};

static inline void Stepper_WriteNibble(uint8_t nibble)
{
    GPIOPinWrite(STEPPER_GPIO_BASE, STEPPER_PINS_MASK, (nibble & 0x0F));
}

static uint32_t Stepper_UsToTicks(uint32_t us)
{
    // ticks = Fclk * us / 1e6
    uint64_t ticks = ((uint64_t)g_sysclk_hz * (uint64_t)us) / 1000000ull;
    if (ticks < 2ull) ticks = 2ull;
    if (ticks > 0xFFFFFFFFull) ticks = 0xFFFFFFFFull;
    return (uint32_t)ticks;
}

static void Stepper_TimerSetPeriodTicks(uint32_t ticks)
{
    
    const uint32_t min_ticks = (g_sysclk_hz / 1000000u) * STEPPER_MIN_PERIOD_US;
    if (ticks < min_ticks) ticks = min_ticks;

    TimerLoadSet(STEPPER_TIMER_BASE, TIMER_A, ticks - 1u);
}

//Public functions
void Stepper_Init(void)
{
    SysCtlPeripheralEnable(STEPPER_GPIO_PERIPH);
    while(!SysCtlPeripheralReady(STEPPER_GPIO_PERIPH)) {}

    GPIOPinTypeGPIOOutput(STEPPER_GPIO_BASE, STEPPER_PINS_MASK);
    Stepper_WriteNibble(0x00);

    SysCtlPeripheralEnable(STEPPER_TIMER_PERIPH);
    while(!SysCtlPeripheralReady(STEPPER_TIMER_PERIPH)) {}

    TimerDisable(STEPPER_TIMER_BASE, TIMER_A);
    TimerConfigure(STEPPER_TIMER_BASE, TIMER_CFG_PERIODIC);

    
    TimerControlStall(STEPPER_TIMER_BASE, TIMER_A, true);

    
    g_sysclk_hz = SysCtlClockGet();

    // Clamp initial period
    uint32_t p = g_period_us;
    if (p < STEPPER_MIN_PERIOD_US) p = STEPPER_MIN_PERIOD_US;
    g_period_us = p;

    Stepper_TimerSetPeriodTicks(Stepper_UsToTicks(g_period_us));

    TimerIntClear(STEPPER_TIMER_BASE, STEPPER_TIMER_TIMEOUT);
    TimerIntEnable(STEPPER_TIMER_BASE, STEPPER_TIMER_TIMEOUT);

    IntDisable(STEPPER_TIMER_INT_NUM);
    IntPrioritySet(STEPPER_TIMER_INT_NUM, 0xE0); // low priority
    IntEnable(STEPPER_TIMER_INT_NUM);

    g_enabled = false;
}

void Stepper_Enable(bool en)
{
    g_enabled = en;

    if (!en) {
        TimerDisable(STEPPER_TIMER_BASE, TIMER_A);
        Stepper_WriteNibble(0x00);
    } else {
        Stepper_WriteNibble(g_seq_fullstep[g_step_idx & 3u]);
        TimerIntClear(STEPPER_TIMER_BASE, STEPPER_TIMER_TIMEOUT);
        TimerEnable(STEPPER_TIMER_BASE, TIMER_A);
    }
}

void Stepper_SetDirection(int8_t dir)
{
    g_dir = (dir < 0) ? -1 : +1;
}

void Stepper_SetPeriodUs(uint32_t period_us)
{
    if (period_us < STEPPER_MIN_PERIOD_US) period_us = STEPPER_MIN_PERIOD_US;
    g_period_us = period_us;

    uint32_t ticks = Stepper_UsToTicks(g_period_us);

    bool was_en = g_enabled;
    TimerDisable(STEPPER_TIMER_BASE, TIMER_A);
    Stepper_TimerSetPeriodTicks(ticks);
    TimerIntClear(STEPPER_TIMER_BASE, STEPPER_TIMER_TIMEOUT);
    if (was_en) TimerEnable(STEPPER_TIMER_BASE, TIMER_A);
}

uint32_t Stepper_GetPeriodUs(void)
{
    return g_period_us;
}

//ISR
void TIMER0A_Handler(void)
{
    TimerIntClear(STEPPER_TIMER_BASE, STEPPER_TIMER_TIMEOUT);

    if (!g_enabled) {
        Stepper_WriteNibble(0x00);
        return;
    }

    if (g_dir > 0) g_step_idx = (uint8_t)((g_step_idx + 1u) & 3u);
    else           g_step_idx = (uint8_t)((g_step_idx + 3u) & 3u);

    Stepper_WriteNibble(g_seq_fullstep[g_step_idx]);
}

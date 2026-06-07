// stepper.h
#pragma once
#include <stdint.h>
#include <stdbool.h>

void Stepper_Init(void);

// Enable/disable stepping
void Stepper_Enable(bool en);

// dir = +1 (CW), -1 (CCW). Invalid input treated as +1
void Stepper_SetDirection(int8_t dir);

// Step period in ms, clamped to min 5ms.
void Stepper_SetPeriodUs(uint32_t period_us);
uint32_t Stepper_GetPeriodUs(void);

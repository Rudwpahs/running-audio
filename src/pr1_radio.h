#pragma once

#include <RadioLib.h>

extern SX1280 radio;

void pr1ConfigureRadio();
void pr1HaltWithCode(const char* step, int16_t code);
void pr1CheckRadioStep(const char* step, int16_t code);

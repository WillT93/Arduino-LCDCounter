#ifndef _T93_LCD_COUNTER_LCD_h
#define _T93_LCD_COUNTER_LCD_h

#include <Arduino.h>
#include "globals_t93.h"

// The custom chars that make up the various animation frames.
const byte animationCustomChars[][8] =
{
  {
    0x1B,
    0x1B,
    0x1B,
    0x1B,
    0x1B,
    0x1B,
    0x1B,
    0x1B
  },
  {
    0x18,
    0x18,
    0x1B,
    0x1B,
    0x1B,
    0x1B,
    0x1B,
    0x1B
  },
  {
    0x00,
    0x00,
    0x00,
    0x00,
    0x18,
    0x18,
    0x1B,
    0x1B
  },
  {
    0x00,
    0x00,
    0x00,
    0x00,
    0x03,
    0x03,
    0x1B,
    0x1B
  },
  {
    0x03,
    0x03,
    0x1B,
    0x1B,
    0x1B,
    0x1B,
    0x1B,
    0x1B
  },
  {
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00
  }
};

// The characters that a single column must progress through in completion for one cycle of the animation. First digit is charater for upper row, second digit is for lower row.
const int animationSequence[ANIM_FRAME_COUNT][LCD_ROWS] =
{
  { 5, 3 },
  { 5, 4 },
  { 3, 0 },
  { 4, 0 },
  { 1, 0 },
  { 2, 0 },
  { 5, 1 },
  { 5, 2 },
};

void InitializeLCD();
void ProcessDisplayValueUpdate(bool = false);
void WriteToLCD(const char*, const char* = "", bool = false);
void PerformLCDAnimation();

#endif
//
// Created by castdata on 8/14/23.
//

#ifndef UIT_H
#define UIT_H

#include "extend.h"

TaskHandle_t uiTaskHandler;

static int getBit2Word(const uint8_t *word, int8_t position);
static void sendData2LCD(const uint8_t dataByte, const bool rW, const bool rs);
static void clearLcdScreen();
static void setLcdCursor2Home();
static void setLcdEntryMode(const bool incrementMode, const bool shift);
static void setLcdOptions(const bool displayOn, const bool cursorOn, const bool cursorBlink);
static void setLcdCursorOrShift(const bool displayShift, const bool rightShift);
static void setLcdFunction(const bool bitDL, const bool bitN, const bool bitF);
static void setLcdCGRAM_addr(uint8_t addr);
static void setLcdDDRAM_addr(uint8_t addr);
static void write2LCD(const char *dataStr, const uint8_t dataStrLength);
static void setLcdPos(const uint8_t x);

void initUiTask();
static void uiTask(void *pvParameters);




#endif //OPENCOLIBRI_UIT_H

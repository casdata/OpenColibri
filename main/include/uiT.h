//
// Created by castdata on 8/14/23.
//

#ifndef UIT_H
#define UIT_H

#include "extend.h"
#include "controlT.h"

typedef enum{
    IDLE_UI = 0,
    BOOTING_UI,
    MAINTENANCE_UI,
    CLEAN_UI,
    PRE_PREPARE_DRINK_UI,
    PREPARE_DRINK_UI,
    ERROR_UI
} UiState;

typedef struct UiUpdateStruct{
    bool updateTemp;
    bool updateDataStr;
} UiUpdate;


extern TaskHandle_t uiTaskH;
extern uint8_t lcdMemPos;

static int getBit2Word(const uint8_t *word, int8_t position);
static void setBit2Word(uint16_t *word, int bitValue, int8_t position);
static void sendData2LCD(const uint8_t dataByte, const bool rW, const bool rs);
static void fullClearLcdScreen();
static void clearLcdScreen();
static void setLcdCursor2Home();
static void setLcdEntryMode(const bool incrementMode, const bool shift);
static void setLcdOptions(const bool displayOn, const bool cursorOn, const bool cursorBlink);
static void setLcdCursorOrShift(const bool displayShift, const bool rightShift);
static void setLcdFunction(const bool bitDL, const bool bitN, const bool bitF);
static void setLcdCGRAM_addr(uint8_t addr);
static void setLcdDDRAM_addr(uint8_t addr);
static void write2LCD(const char *dataStr, const uint8_t dataStrLength, const uint8_t pos);
static void setLcdPos(uint8_t x);
static void getBoilerTemp(float *temperature);
static bool updateUI(InputBtnStruct *uiBtns);
static void checkNotifications4Ui(UiState *previousUiState, UiState *currentUiState, ErrorCode *errorCode);
static void checkQueue4Ui(UiData *myUiData, UiUpdate *myUiUpdate);
static void showPreparingDrinkName(UiData *myUiData);
static void inBootingCodeState(UiData *myUiData);
static void inErrorCodeState(const ErrorCode errorCode);

static void initLcd();
void initUiTask();
static void uiTask(void *pvParameters);




#endif //OPENCOLIBRI_UIT_H

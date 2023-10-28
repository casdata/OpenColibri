#ifndef CONTROLT_H
#define CONTROLT_H

#include "extend.h"
#include "boilerT.h"
#include "interruptsT.h"
#include "uiT.h"

typedef enum{
    IDLE_C,
    MAINTENANCE_C,
    CLEAN_C,
    DRINK_1_C,
    DRINK_2_C,
    DRINK_3_C,
    DRINK_4_C,
    DRINK_5_C,
    DRINK_6_C,
    DRINK_7_C,
    DRINK_8_C    
} ControlState;

extern TaskHandle_t controlTaskH;

static void runDrink1(uint8_t *dataBytes);
static void runDrink2(uint8_t *dataBytes);
static void runDrink3(uint8_t *dataBytes);
static void runDrink4(uint8_t *dataBytes);
static void runDrink5(uint8_t *dataBytes);
static void runDrink6(uint8_t *dataBytes);
static void runDrink7(uint8_t *dataBytes);
static void runDrink8(uint8_t *dataBytes);
void writeBytesMCP2307(const uint8_t i2cAddr, uint8_t regAddr, uint8_t *dataBuff, uint8_t numOfBytes);
static bool bitChangedInByte(uint8_t *byteData, const uint8_t bitPos, const bool newValue);
static bool differentRelayState(uint8_t *byteData, const OutputRelays outputRelays, const bool newValue);
static void setBit2Byte(uint8_t *byteData, const uint8_t bitPos);
static void clearBit2Byte(uint8_t *byteData, const uint8_t bitPos);
static void setRelay(uint8_t *byteData, const OutputRelays outputRelays);
static void resetRelay(uint8_t *byteData, const OutputRelays outputRelays);
static void resetPulseCount();
static void checkAirBreak(uint8_t *dataBytes);
static void resetBrewer(uint8_t *dataBytes);
static void setBrewer2StartPosition(uint8_t *dataBytes);
static void setBrewer2InjectPosition(uint8_t *dataBytes);
static void injecBrewerWater(uint8_t *dataBytes, uint16_t pulses);
static void grindAndDeliver(uint8_t *dataBytes);
static void waitMachine2Start(uint8_t *dataBytes);
static void waitBoiler2Start();
static void startBoilerTask();
static void syncronizeAllTasks();
static void checkQueuesFromUi(ControlState *controlState);

void initI2C_MCP23017_Out();
void initI2C_MCP23017_In();
void initControlTask();
static void controlTask(void *pvParameters);

#endif
#ifndef INTERRUPTST_H
#define INTERRUPTST_H

#include "extend.h"
#include "controlT.h"

extern TaskHandle_t intTaskH;

static void readInputSws(InputSwStruct *inputStruct, uint8_t *iBuff);
static void check4Notifications();
static void readBytesMCP2307(const uint8_t i2cAddr, uint8_t regAddr, uint8_t *dataBuff, uint8_t numOfBytes);
static bool readSW(uint8_t *byteData, const SensorSw swName);

void initInterruptsTask();
static void interruptsTask(void *pvParameters);


#endif
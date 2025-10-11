#ifndef INTERRUPTST_H
#define INTERRUPTST_H

#include "extend.h"
#include "controlT.h"

typedef struct WaterFlowStruct{
    bool    state;
    bool    alarm;
    bool    waterFill;
    bool    firstCheck;
    double  refTime;
} WaterFlowData;

typedef struct PulseTestStruct{
    bool    manualReset;
    bool    state;
    double  refTime;
    double  pulseTime;
} PulseTestData;

extern TaskHandle_t intTaskH;


static void checkAirBreakSw(uint8_t *iBuff);
static void readInputSws(InputSwStruct *inputStruct, uint8_t *iBuff, uint8_t *oBuff, WaterFlowData *wFlowData);
static void check4Notifications(WaterFlowData *wFlowData, PulseTestData *pulseData);
static bool check4PulseConut(uint16_t *ptrCount, uint16_t *ptrCountTarget);
static void readBytesMCP2307(const uint8_t i2cAddr, uint8_t regAddr, uint8_t *dataBuff, uint8_t numOfBytes);
static void writeBytesMCP2307Int(const uint8_t i2cAddr, uint8_t regAddr, uint8_t *dataBuff, uint8_t numOfBytes);
static void cleanReadByte4WaterInlet(uint8_t *byteData);
static void setWaterInletBit(uint8_t *byteData);
static bool readSW(uint8_t *byteData, const SensorSw swName);

void initInterruptsTask();
static void interruptsTask(void *pvParameters);


#endif
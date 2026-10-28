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
    double  checkRefTime;
    double  waterSafeTime;
    bool    failSafe;
} WaterFlowData;

typedef struct PulseTestStruct{
    bool    manualReset;
    bool    state;
    double  refTime;
    double  pulseTime;
} PulseTestData;

extern TaskHandle_t intTaskH;


static void checkAirBreakSw(uint8_t *iBuff);
static void readInputSws(InputSwStruct *inputStruct, uint8_t *iBuff, WaterFlowData *wFlowData);
static void check4Notifications(WaterFlowData *wFlowData, PulseTestData *pulseData);
static bool check4PulseConut(uint16_t *ptrCount, uint16_t *ptrCountTarget, uint16_t *ptrPreEndCount);
static bool readSW(uint8_t *byteData, const SensorSw swName);

void initInterruptsTask();
static void interruptsTask(void *pvParameters);


#endif
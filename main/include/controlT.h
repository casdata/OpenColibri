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
    DRINK_C,
    WATER_C,
    SWAP_PAGE_C
} ControlState;


typedef struct ControlDataStruct{
    ControlState    controlState;
    uint8_t         recipeIndex;
    Page            page;
} ControlData;

typedef struct ContainerPowStruct
{
    float grPerSecCon1;
    float grPerSecCon2;
    float grPerSecCon3;
    double timePerPulse;
} ContsPowderData;


extern TaskHandle_t controlTaskH;

static void runDrink1(uint8_t *dataBytes, ContsPowderData *contsPowData);   //2 delete
static void runDrink2(uint8_t *dataBytes, ContsPowderData *contsPowData);   //2 delete
static void runDrink3(uint8_t *dataBytes, ContsPowderData *contsPowData);   //2 delete
static void runDrink4(uint8_t *dataBytes, ContsPowderData *contsPowData);   //2 delete
static void runDrink5(uint8_t *dataBytes, ContsPowderData *contsPowData);   //2 delete
static void runDrink6(uint8_t *dataBytes, ContsPowderData *contsPowData);   //2 delete
static void runDrink7(uint8_t *dataBytes, ContsPowderData *contsPowData);   //2 delete
static void runDrink8(uint8_t *dataBytes, ContsPowderData *contsPowData);   //2 delete
static void runDrink(const Recipe *myRecipe, uint8_t *dataBytes, ContsPowderData *contsPowData);
void writeBytesMCP2307(const uint8_t i2cAddr, uint8_t regAddr, uint8_t *dataBuff, uint8_t numOfBytes);
static bool bitChangedInByte(uint8_t *byteData, const uint8_t bitPos, const bool newValue);
static bool differentRelayState(uint8_t *byteData, const OutputRelays outputRelays, const bool newValue);
static void setBit2Byte(uint8_t *byteData, const uint8_t bitPos);
static void clearBit2Byte(uint8_t *byteData, const uint8_t bitPos);
static void setRelay(uint8_t *byteData, const OutputRelays outputRelays);
static void resetRelay(uint8_t *byteData, const OutputRelays outputRelays);
static void checkAirBreak(uint8_t *dataBytes);
static void resetBrewer(uint8_t *dataBytes);
static void setBrewer2StartPosition(uint8_t *dataBytes);
static void setBrewer2InjectPosition(uint8_t *dataBytes);
static void injecBrewerWater(uint8_t *dataBytes, const uint16_t pulses);
static void injectPowderPlusWater(uint8_t *dataBytes, const uint16_t pulses, ContsPowderData *contsPowData, const OutputRelays outputRelay);
static void injectPowderPlusWaterExtraContainer(const uint16_t pulses, ContsPowderData *constPowData);
static void injectOnlyWaterLine(uint8_t *dataBytes, const uint16_t pulses);
static void injectOnlyWaterLineManual(uint8_t *dataBytes);
static bool grindAndDeliver(uint8_t *dataBytes, bool checkStop);
static void cleanMachine(uint8_t *dataBytes);
static void calculatePulseTime(uint8_t *dataBytes);
static void waitMachine2Start(uint8_t *dataBytes);
static void waitBoiler2Start();
static void startBoilerTask();
static void syncronizeAllTasks();
static bool checkWaterBtnFromUi();
static bool checkStopBtnFromUi();
static void checkQueuesFromUi(ControlData *controlData);
static void initMemData(Recipe *recipeData, SystemData *sysData);
static void checkMemContent(const Recipe *recipeData, const SystemData *sysData);
static void loadFakeMemContent(Recipe *recipeData, SystemData *sysData);

void initI2C_MCP23017_Out();
void initI2C_MCP23017_In();
void initControlTask();
static void controlTask(void *pvParameters);

#endif
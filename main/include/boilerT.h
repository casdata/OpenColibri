//
// Created by castdata on 8/14/23.
//

#ifndef OPENCOLIBRI_BOILERT_H
#define OPENCOLIBRI_BOILERT_H

#include "extend.h"
#include "controlT.h"

extern TaskHandle_t boilerTaskH;
extern QueueHandle_t xQueueUart;

/*
static float thermA =  0.6989527046e-3;              //Calculated Stainhart-Hart model coefficients
static float thermB =  2.948656661e-4;
static float thermC =  -1.381899722e-7;

 * 35875= 2.18  (0)
 * 12000= 25    (25)
 * 2900 = 62.45 (50)
 * 1475 = 84.41 (85)
 * 1260 = 90    (90)
 * 963 = 100    (100)
 */


static float thermA =  0.9430468202e-3;              //Calculated Stainhart-Hart model coefficients
static float thermB =  2.480482099e-4;
static float thermC =  1.009595903e-7;

/*
 * 35875= 0     (0)
 * 12000= 24.78 (25)
 * 2900 = 63.35 (50)
 * 1475 = 85    (85)
 * 1260 = 90.41 (90)
 * 963 =  100   (100)
 */


typedef enum{
    INIT_B = 0,
    IDLE_B,
    IDLE_2_HOT_B,
    IDLE_2_HOT_MAX_B,
    HOT_B,
    HOT_2_IDLE_B,
    HOT_MAX_B,
    HOT_B_2_GRADUAL
} BoilerState;

typedef enum{
    P_DATA = 0,
    I_DATA,
    D_DATA,
    S_DATA
} NextionData;

typedef enum {
    SHORT_T = 0,
    LONG_T,
} ExtraHeat;

typedef struct ExtraHeatStruct{
    bool enable;
    double refTime;
} ExtraHeatData;

typedef struct BoilerBoolStruct{
    bool boilerIsOn;
    bool boilerState;
    bool toSend;                            //Used for nextion pid tunner, true = thermistorTemp; false = setPoint
    float thermistorTemp;
    float boilerDuty;
} BoilerStructData;

typedef struct PID_Struct{
    bool  controllerState;
    float setPoint;
    float kP;
    float kI;
    float kD;
    float minOut;
    float maxOut;
    float offset;
    float outputSum;
    float lastInput;
    float error;
    float pError;
    float pidP;
    float pidI;
    float pidD;
    double cTime;
    double pTime;
} PID_DataStruct;

static bool readMCP3421_ADC(uint16_t *adcValue);
static void readTemp(BoilerStructData *boilerStructData);
static void wait4Control();
static void wait4BoilerTempValue(float *targetTemperature, PID_DataStruct *pidDataStruct);
static void check4BoilerTempUpdate(float *targetTemperature, PID_DataStruct *pidDataStruct);
static void checkNotifications4Boiler(BoilerState *bState);
static void getMessage2Send2Nextion(char *strData, BoilerStructData *boilerStructData);
static bool checkDataFromNextionPID_Tunner(PID_DataStruct *pidDataStruct);
static void computePID(PID_DataStruct *pidDataStruct, float inputData, float *outputData);

void initBoilerTask();
static void boilerTask(void *pvParameters);


#endif //OPENCOLIBRI_BOILERT_H

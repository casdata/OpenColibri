//
// Created by castdata on 8/14/23.
//

#ifndef OPENCOLIBRI_BOILERT_H
#define OPENCOLIBRI_BOILERT_H

#include "extend.h"
#include "controlT.h"

extern TaskHandle_t boilerTaskH;

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
    B_NORMAL_MODE = 0,
    B_NORMAL_2_OVERDRIVE,
    B_OVERDRIVE_MODE,
    B_OVERDRIVE_2_NORMAL
} BoilerStateMode;


typedef struct BoilerBoolStruct{
    bool boilerIsOn;
    bool boilerState;
    BoilerStateMode boilerStateMode;
    float thermistorTemp;
} BoilerStructData;


static bool readMCP3421_ADC(uint16_t *adcValue);
static void readTemp(BoilerStructData *boilerStructData);
static void innerBoilerControl(BoilerStructData *boilerStructData);
static void wait4Control();
static void checkNotifications4Boiler(BoilerStructData *boilerStructData);

void initBoilerTask();
static void boilerTask(void *pvParameters);


#endif //OPENCOLIBRI_BOILERT_H

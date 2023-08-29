//
// Created by castdata on 7/31/23.
//

#ifndef EXTEND_H
#define EXTEND_H

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "math.h"

#define CONTROL_TASK_SIZE           4096
#define CONTROL_TASK_PRIORITY       5
#define UI_TASK_SIZE                2048
#define UI_TASK_PRIORITY            4
#define INTERRUPTS_TASK_SIZE        4096
#define INTERRUPTS_TASK_PRIORITY    5
#define BOILER_TASK_SIZE            2048
#define BOILER_TASK_PRIORITY        4

#define STATUS_LED_PIN      GPIO_NUM_2
#define BOILER_PIN          GPIO_NUM_27
#define VOLUMETRIC_PIN      GPIO_NUM_34
#define MCP23017_INTB_PIN   GPIO_NUM_18
#define MCP23017_IN_RS_PIN  GPIO_NUM_19
#define MCP23017_OUT_RS_PIN GPIO_NUM_5
#define CLK_UI_PIN          GPIO_NUM_32
#define SH_LD_UI_PIN        GPIO_NUM_33         //SCLR_PIN
#define IN_SERIAL_UI_PIN    GPIO_NUM_35
#define RCK_DISPLAY_PIN     GPIO_NUM_25
#define SER_DISPLAY_PIN     GPIO_NUM_26

#define I2C_SCL_PIN         GPIO_NUM_22
#define I2C_SDA_PIN         GPIO_NUM_21
#define I2C_PORT_NUM        0
#define I2C_FREQ_HZ         100000
#define I2C_TX_BUFF_DISABLE 0
#define I2C_RX_BUFF_DISABLE 0
#define I2C_TIMEOUT_MS      1000

#define I2C_ACK             0x0
#define I2C_NACK            0x1

#define MCP23017_INPUT_ADDR     0x24
#define MCP23017_OUTPUT_ADDR    0x20
#define MCP3421_ADC_ADDR        0x68

#define MAIN_TASK_TAG           "MAIN_T"
#define CONTROL_TASK_TAG        "CONTROL_T"
#define INTERRUPTS_TASK_TAG     "INTERRUPTS_T"
#define UI_TASK_TAG             "UI_TASK_T"
#define BOILER_TASK_TAG         "BOILER_TASK_T"

typedef enum {
    SOLENOID_VALVE_2 = 0,   //RELAY 1 / GPB6 / O.5 BOILER 3 WAY-VALVE FAR
    SOLENOID_VALVE_1,       //RELAY 2 / GPB5 / O.6 BOILER 3 WAY-VALVE NEAR
    COFFEE_BREWER_MOTOR,    //RELAY 3 / GPB4 / O.9 ARM MOTOR
    PUMP,                   //RELAY 4 / GPB3 / O.3 WATER PUMP
    COFFEE_GRINDER_MOTOR,   //RELAY 5 / GPB2 / O.0 GRINDER
    COFFEE_RELEASE_MAGNET,  //RELAY 6 / GPB1 / COFFEE POWDER VALVE
    THREE_WAY_VALVE,        //RELAY 7 / GPB0 / BOILER 3 WAY-VALVE CENTER
    CUP_RELEASE_MOTOR,      //RELAY 8 / GPA7
    CUP_STACKER_SHIFT_MOTOR,//RELAY 9 / GPA6
    WATER_INLET_VALVE,      //RELAY 10 / GPA5
    DOSER_DEVICE_1,         //RELAY 11 / GPA4
    DOSER_DEVICE_2,         //RELAY 12 / GPA3
    WHIPPER                 //RELAY 14 / GPA1
} OutputRelays;

typedef enum {
    WASTE_OVERFLOW_SW = 0,      //GPB0 / IPF_SW
    COFFEE_BREWER_SW,           //GPB1 / COFFEE_MOTOR_SW    / invert
    AIR_BREAK_SW,               //GPB2 / IVA_SW
    COFFEE_RELEASE_MOTOR_CAM,   //GPB3 / COFFEE_CHAMBER_SW  / invert
    CUP_RELEASE_SW,             //GPB4 / CMSB_SW
    CUP_SENSOR_SW               //GPB5 / IVB_SW
} SensorSw;

typedef enum {
    NONE,
    NO_WATER,
    NO_COFFEE,
    BREWER_ISSUE
} ErrorCode;

typedef struct InputBoolStruct{
    bool    wasteOverflowSw;
    bool    coffeeBrewerSw;
    bool    airBreakSw;
    bool    coffeeReleaseSw;
    bool    cupReleaseSw;
    bool    cupSensorSw;
} InputSwStruct;

/*
typedef struct OutputBoolStruct{
    bool    solenoidValve2;
    bool    solenoidValve1;
    bool    coffeeBrewerMotor;
    bool    pump;
    bool    coffeeGrinderMotor;
    bool    coffeeReleaseMagnet;
    bool    threeWayValve;
    bool    cupReleaseMotor;
    bool    cupStackerShiftMotor;
    bool    waterInletValve;
    bool    doserDevice1;
    bool    doserDevice2;
    bool    whipper;
} OutputRelayStruct;
*/


extern QueueHandle_t xQueueIntB;
extern QueueHandle_t xQueueInputsSw;
extern QueueHandle_t xQueueInputPulse;
extern QueueHandle_t xQueueBoilerTemp;


#endif //OPENCOLIBRI_EXTEND_H

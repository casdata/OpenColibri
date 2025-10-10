//
// Created by castdata on 7/31/23.
//

#ifndef EXTEND_H
#define EXTEND_H

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"          //bt
#include "freertos/queue.h"
#include "esp_system.h"                     //bt
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_bt.h"                         //bt

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"

#include "esp_spp_api.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "math.h"
#include "pid_ctrl.h"



#define CONTROL_TASK_SIZE           6144   //6144
#define CONTROL_TASK_PRIORITY       5
#define UI_TASK_SIZE                2048
#define UI_TASK_PRIORITY            4
#define INTERRUPTS_TASK_SIZE        4096
#define INTERRUPTS_TASK_PRIORITY    6
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
#define POWDER_C_PIN        GPIO_NUM_17

#define TXD_2_PIN           GPIO_NUM_17
#define RXD_2_PIN           GPIO_NUM_16
#define UART_PORT_N         UART_NUM_2   
#define BUFF_SIZE           1024
#define NEXTION_D_CHAR      0x7E

#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL        LEDC_CHANNEL_0
#define LEDC_DUTY_RES       LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_FREQUENCY      5000 // Frequency in Hertz. Set frequency at 5 kHz

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

#define NEXTION_LCD                 0
#define SHOW_BOILER_TEMP            1

#define SPP_PROFILE_NUM             1
#define SPP_PROFILE_APP_IDX         0
#define ESP_SPP_APP_ID              0x56
#define SAMPLE_DEVICE_NAME          "ESP_SPP_SERVER"    //The Device Name Characteristics in GAP
#define SPP_SVC_INST_ID	            0

/// Characteristic UUID
#define ESP_GATT_UUID_SPP_DATA_RECEIVE      0xABF1
#define ESP_GATT_UUID_SPP_DATA_NOTIFY       0xABF2

#define spp_sprintf(s,...)         sprintf((char*)(s), ##__VA_ARGS__)
#define SPP_DATA_MAX_LEN           (512)
#define SPP_CMD_MAX_LEN            (20)
#define SPP_STATUS_MAX_LEN         (20)
#define SPP_DATA_BUFF_MAX_LEN      (2*1024)

typedef enum {
    SOLENOID_VALVE_2 = 0,   //RELAY 1 / GPB6 / O.5 BOILER 3 WAY-VALVE FAR / WHIPPER PIPE
    SOLENOID_VALVE_1,       //RELAY 2 / GPB5 / O.6 BOILER 3 WAY-VALVE NEAR / ONLY WATER PIPE
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

typedef enum {
    COFFEE_M = 0,
    COFFEE_PRE_M,
    POWDER_A_M,
    POWDER_B_M,
    POWDER_C_M,
    WATER_M
} RecipeModuleType;

typedef struct ModuleDataStruct{
    RecipeModuleType    moduleType;
    uint16_t            pulses;
    uint8_t             gr;                      
} RecipeModuleStruct;


typedef struct RecipeDataStruct{
    char                *recipeName;
    RecipeModuleStruct  *modulesArray;
    uint16_t            recipeCounter;
    size_t              moduleSize;
} Recipe;


typedef struct SystemDataSruct{
    uint8_t     boilerTemperature;
    uint16_t    password;
    uint32_t    mainCounter;
} SystemData;

typedef struct InputBoolStruct{
    bool    wasteOverflowSw;
    bool    coffeeBrewerSw;
    bool    airBreakSw;
    bool    previousAirBreakSw;
    bool    coffeeReleaseSw;
    bool    cupReleaseSw;
    bool    cupSensorSw;
    double  airBreakRefTime;
} InputSwStruct;

typedef struct Input2BoolStruct{
    bool    btn0;
    bool    btn1;
    bool    btn2;
    bool    btn3;
    bool    btn4;
    bool    btn5;
    bool    btn6;
    bool    btn7;
    bool    btnA;
    bool    btnB;
} InputBtnStruct;

typedef enum{
    PAGE_1 = 0,
    PAGE_2
} Page;

typedef struct UI_DataStruct{
    Page    page;
    float   boilerTempUi;
    char    *strData;
} UiData;

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
extern QueueHandle_t xQueueInputBtns;
extern QueueHandle_t xQueueInputPulse;
extern QueueHandle_t xQueueInputTimePerPulse;
extern QueueHandle_t xQueueBoilerTemp;
extern QueueHandle_t xQueueUI;
extern QueueHandle_t xQueueData2Boiler;


#endif //OPENCOLIBRI_EXTEND_H

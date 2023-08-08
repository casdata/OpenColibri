#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "extend.h"




uint8_t *inputIO_Buff;
uint8_t *outputIO_Buff;




QueueHandle_t xQueueIntB;


static void IRAM_ATTR interrupt_handler(void *args){
    int16_t pinNumber = (int16_t)args;
    xQueueSendFromISR(xQueueIntB, &pinNumber, NULL);
    //ESP_LOGI("INTR", "Interrupt intb %d", pinNumber);
}


void setBit2Word(uint16_t *word, int bitValue, int8_t position){

    uint16_t mask = 1 << position;
    mask = ~mask;
    *word &= mask;
    *word += (bitValue << position);
}

int getBit2Word(const uint16_t *word, int8_t position){
    int bitValue = *word >> position;

    bitValue &= 1;

    return bitValue;
}

void readBytesMCP2307(const uint8_t i2cAddr, uint8_t regAddr, uint8_t *dataBuff, uint8_t numOfBytes){
    int cmdReturn;

    i2c_cmd_handle_t cmdHandle = i2c_cmd_link_create();

    i2c_master_start(cmdHandle);
    i2c_master_write_byte(cmdHandle, i2cAddr << 1 | I2C_MASTER_WRITE, I2C_MASTER_ACK);
    i2c_master_write_byte(cmdHandle, regAddr, I2C_MASTER_ACK);
    

    i2c_master_start(cmdHandle);
    i2c_master_write_byte(cmdHandle, i2cAddr << 1 | I2C_MASTER_READ, I2C_MASTER_ACK);
    for(size_t i = 0; i < numOfBytes; i++)
        i2c_master_read_byte(cmdHandle, (dataBuff + i), I2C_MASTER_ACK);

    i2c_master_stop(cmdHandle);


    cmdReturn = i2c_master_cmd_begin(I2C_PORT_NUM, cmdHandle, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmdHandle);

}

void writeBytesMCP2307(const uint8_t i2cAddr, uint8_t regAddr, uint8_t *dataBuff, uint8_t numOfBytes){

    int cmdReturn;

    i2c_cmd_handle_t cmdHandle = i2c_cmd_link_create();

    i2c_master_start(cmdHandle);
    i2c_master_write_byte(cmdHandle, i2cAddr << 1 | I2C_MASTER_WRITE, I2C_MASTER_ACK);
    i2c_master_write_byte(cmdHandle, regAddr, I2C_MASTER_ACK);
    for(size_t i = 0; i < numOfBytes; i++)
        i2c_master_write_byte(cmdHandle, *(dataBuff + i), I2C_MASTER_ACK);
    
    i2c_master_stop(cmdHandle);

    cmdReturn = i2c_master_cmd_begin(I2C_PORT_NUM, cmdHandle, 1000 / portTICK_PERIOD_MS);

    i2c_cmd_link_delete(cmdHandle);


}

void initI2C_MCP23017_Out(){


    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x0, outputIO_Buff, 2);                //Set output pins

    vTaskDelay(pdMS_TO_TICKS(20));

    //*(data2Send) = 0x9;//0xb7;
    //*(data2Send + 1) = 0xe;//0xaa;

    //memset(outputIO_Buff, 0, 2);
 
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, outputIO_Buff, 2);               //Clear all output pins.

    vTaskDelay(pdMS_TO_TICKS(20));

    //memset(data2Send, 0, 3);
    //ESP_LOGW(MAIN_TASK_TAG, "pre -> %d - %d", *(data2Send), *(data2Send + 1));
    //ESP_LOGW(MAIN_TASK_TAG, "pos -> %d - %d", *(data2Send), *(data2Send + 1));
    

}

void initI2C_MCP23017_In(){
    uint8_t *dataBuff = malloc(1);

    memset(dataBuff, 0x3f, 1);

    writeBytesMCP2307(MCP23017_INPUT_ADDR, 0x05, dataBuff, 1);              //GPINTENB

    memset(dataBuff, 0x2, 1);

    writeBytesMCP2307(MCP23017_INPUT_ADDR, 0X0b, dataBuff, 1);


    //readBytesMCP2307(MCP23017_INPUT_ADDR, 0x13, inputIO_Buff, 1);

    //writeBytesMCP2307(MCP23017_INPUT_ADDR, 0x07, dataBuff, 1);              //DEFVALB

    //writeBytesMCP2307(MCP23017_INPUT_ADDR, 0x09, dataBuff, 1);              //INTCONB

    free(dataBuff);

}

void setBit2Byte(uint8_t *byteData, const uint8_t bitPos){
    *byteData |= (1 << bitPos);
}

void clearBit2Byte(uint8_t *byteData, const uint8_t bitPos){
    *byteData &= ~(1 << bitPos);
}

bool readSW(uint8_t *byteData, const SensorSw swName){
    return (bool)( (*byteData >> (uint8_t)swName) & 1);
}



void setRelay(uint8_t *byteData, const OutputRelays outputRelays){
    switch(outputRelays){
        case SOLENOID_VALVE_2:
            setBit2Byte(byteData + 1, 6);
            break;
        case SOLENOID_VALVE_1:
            setBit2Byte(byteData + 1, 5);
            break;
        case COFFEE_BREWER_MOTOR:
            setBit2Byte(byteData + 1, 4);
            break;
        case PUMP:
            setBit2Byte(byteData + 1, 3);
            break;
        case COFFEE_GRINDER_MOTOR:
            setBit2Byte(byteData + 1, 2);
            break;
        case COFFEE_RELEASE_MAGNET:
            setBit2Byte(byteData + 1, 1);
            break;
        case THREE_WAY_VALVE:
            setBit2Byte(byteData + 1, 0);
            break;
        case CUP_RELEASE_MOTOR:
            setBit2Byte(byteData, 7);
            break;
        case CUP_STACKER_SHIFT_MOTOR:
            setBit2Byte(byteData, 6);
            break;
        case WATER_INLET_VALVE:
            setBit2Byte(byteData, 5);
            break;
        case DOSER_DEVICE_1:
            setBit2Byte(byteData, 4);
            break;
        case DOSER_DEVICE_2:
            setBit2Byte(byteData, 3);
            break;
        case WHIPPER:
            setBit2Byte(byteData, 1);
            break;
    }
}

void resetRelay(uint8_t *byteData, const OutputRelays outputRelays){
    switch(outputRelays){
        case SOLENOID_VALVE_2:
            clearBit2Byte(byteData + 1, 6);
            break;
        case SOLENOID_VALVE_1:
            clearBit2Byte(byteData + 1, 5);
            break;
        case COFFEE_BREWER_MOTOR:
            clearBit2Byte(byteData + 1, 4);
            break;
        case PUMP:
            clearBit2Byte(byteData + 1, 3);
            break;
        case COFFEE_GRINDER_MOTOR:
            clearBit2Byte(byteData + 1, 2);
            break;
        case COFFEE_RELEASE_MAGNET:
            clearBit2Byte(byteData + 1, 1);
            break;
        case THREE_WAY_VALVE:
            clearBit2Byte(byteData + 1, 0);
            break;
        case CUP_RELEASE_MOTOR:
            clearBit2Byte(byteData, 7);
            break;
        case CUP_STACKER_SHIFT_MOTOR:
            clearBit2Byte(byteData, 6);
            break;
        case WATER_INLET_VALVE:
            clearBit2Byte(byteData, 5);
            break;
        case DOSER_DEVICE_1:
            clearBit2Byte(byteData, 4);
            break;
        case DOSER_DEVICE_2:
            clearBit2Byte(byteData, 3);
            break;
        case WHIPPER:
            clearBit2Byte(byteData, 1);
            break;
    }
}


void sendData2LCD(const uint8_t dataByte, const bool rW, const bool rs){

    int enableLatch = 0;

    //ESP_LOGI("DEBUG", "-> %d", dataByte);

    for(size_t j = 0; j < 2; j++){
        gpio_set_level(SER_DISPLAY_PIN, !(uint32_t)rs);
        gpio_set_level(CLK_UI_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_set_level(CLK_UI_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(1));

        gpio_set_level(SER_DISPLAY_PIN, !(uint32_t)rW);
        gpio_set_level(CLK_UI_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_set_level(CLK_UI_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(1));

        gpio_set_level(SER_DISPLAY_PIN, enableLatch);              //Enable (latch h)
        gpio_set_level(CLK_UI_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_set_level(CLK_UI_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(1));

        for(int i = 7; i >= 0; i--){

            //ESP_LOGI("DEBUG", "%d: %d - %d",j, i, getBit2Word(&dataByte, i));

            gpio_set_level(SER_DISPLAY_PIN, !getBit2Word(&dataByte, i));
            gpio_set_level(CLK_UI_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(1));
            gpio_set_level(CLK_UI_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(1));


        }

        //ESP_LOGI("DEBUG", " ");

        gpio_set_level(RCK_DISPLAY_PIN, 1);                         //Register transferred to output latches
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_set_level(RCK_DISPLAY_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(1));


        enableLatch = 1;



        //vTaskDelay(pdMS_TO_TICKS(5000));

    }


}


void write2LCD(){



}


void updateUI(uint16_t *uiSwitches){

    
    //Clear registers in the 74HC165 and 74HC595
    gpio_set_level(SH_LD_UI_PIN, 1);            //LOW

    vTaskDelay(pdMS_TO_TICKS(1));

    gpio_set_level(SH_LD_UI_PIN, 0);            //HIGH

    vTaskDelay(pdMS_TO_TICKS(1));


    //Shift 0 to the display shift register.
    gpio_set_level(SER_DISPLAY_PIN, 1);         //LOW
    


    setBit2Word(uiSwitches, !gpio_get_level(IN_SERIAL_UI_PIN), 0);


    for(size_t i = 1; i < 8; i++){
        gpio_set_level(CLK_UI_PIN, 0);

        vTaskDelay(pdMS_TO_TICKS(1));

        gpio_set_level(CLK_UI_PIN, 1);

        setBit2Word(uiSwitches, !gpio_get_level(IN_SERIAL_UI_PIN), i);

        vTaskDelay(pdMS_TO_TICKS(1));

    }

    for(size_t i = 0; i < 9; i++){
        gpio_set_level(CLK_UI_PIN, 0);

        vTaskDelay(pdMS_TO_TICKS(1));

        gpio_set_level(CLK_UI_PIN, 1);

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    for(size_t i = 9; i < 11; i++){
        gpio_set_level(CLK_UI_PIN, 0);

        vTaskDelay(pdMS_TO_TICKS(1));

        gpio_set_level(CLK_UI_PIN, 1);

        setBit2Word(uiSwitches, !gpio_get_level(IN_SERIAL_UI_PIN), i);

        vTaskDelay(pdMS_TO_TICKS(1)); 
    }

    


    //esp_log_level_set("MAIN_TASK", ESP_LOG_VERBOSE);    

    ESP_LOGI("MAIN_TASK", "%d", *uiSwitches);
    /*
    ESP_LOGE("MAIN_TASK", "error");
    ESP_LOGI("MAIN_TASK", "info");
    ESP_LOGW("MAIN_TASK", "warning!");
    ESP_LOGD("MAIN_TASK", "debug");
    ESP_LOGV("MAIN_TASK", "verbose");
    */

    write2LCD();
                                                            
                                                            
}

static void inputMonitorTask(void *pvParameters){

    int16_t pinNum, count = 0;

    while(1){

        if(xQueueReceive(xQueueIntB, &pinNum, pdMS_TO_TICKS(10))){

            bool readInput = false;

            switch(pinNum){
                case VOLUMETRIC_PIN:
                    ESP_LOGE(ALTERNATIVE_TASK_TAG, "Volumetric INTR");
                break;
                case MCP23017_INTB_PIN:
                    ESP_LOGE(ALTERNATIVE_TASK_TAG, "Pin B INTR");
                    
                    readBytesMCP2307(MCP23017_INPUT_ADDR, 0x13, inputIO_Buff, 1);

                    ESP_LOGE(ALTERNATIVE_TASK_TAG, "Read it!!");

                    ESP_LOGW(MAIN_TASK_TAG, "nIO -> %d %d %d %d %d %d %d %d", readSW(inputIO_Buff, 7),
                             readSW(inputIO_Buff, 6),
                             readSW(inputIO_Buff, 5),
                             readSW(inputIO_Buff, 4),
                             readSW(inputIO_Buff, 3),
                             readSW(inputIO_Buff, 2),
                             readSW(inputIO_Buff, 1),
                             readSW(inputIO_Buff, 0));


                    //gpio_set_level(BOILER_PIN, getBitFromByte(inputIO_Buff, 3));
                    gpio_set_level(STATUS_LED_PIN, getBitFromByte(inputIO_Buff, 2));


                break;
            }

            /*
            if(readInput){

                vTaskDelay(pdMS_TO_TICKS(20));                                

                readBytesMCP2307(MCP23017_INPUT_ADDR, 0x13, inputIO_Buff, 1);

                vTaskDelay(pdMS_TO_TICKS(20));   

                ESP_LOGE(ALTERNATIVE_TASK_TAG, "Read it!!");

                
            }
            */

            //ESP_LOGE(ALTERNATIVE_TASK_TAG, "INTR %d - %d", pinNum, ++count);  
        }
     
    }

}


static void mainTask(void* pvParameters){

    uint16_t uiSwitches = 0;


    sendData2LCD(0b00110000, 0, 0);             //Function set: 8bit-length, one-line, 5x8 dots

    vTaskDelay(pdMS_TO_TICKS(200));

    sendData2LCD(0b00001110, 0, 0);             //DISPLAY on, cursor on

    vTaskDelay(pdMS_TO_TICKS(200));
   
    sendData2LCD(0b00000110, 0, 0);             //ENTRY MODE display doesnt shifts - increment
    
    vTaskDelay(pdMS_TO_TICKS(200));

    sendData2LCD(0b01010111, 0, 1);             //Write 'W'

    vTaskDelay(pdMS_TO_TICKS(200));
    

     //sendData2LCD(0, 0, 0);




    while(1){

        updateUI(&uiSwitches);

        //ESP_LOGI(MAIN_TASK_TAG, "-> %d", gpio_get_level(MCP23017_INTB_PIN));




        /*
        countUp++;

        switch(countUp){
            case 1:
                setRelay(outputIO_Buff, SOLENOID_VALVE_2);
                resetRelay(outputIO_Buff, WHIPPER);
                ESP_LOGI("relay", "01 - SOLENOID_VALVE_2");
            break;
            case 2:
                setRelay(outputIO_Buff, SOLENOID_VALVE_1);
                resetRelay(outputIO_Buff, SOLENOID_VALVE_2);
                ESP_LOGI("relay", "02 - SOLENOID_VALVE_1");
            break;
            case 3:
                setRelay(outputIO_Buff, COFFEE_BREWER_MOTOR);
                resetRelay(outputIO_Buff, SOLENOID_VALVE_1);
                ESP_LOGI("relay", "03 - COFFEE_BREWER_MOTOR");
            break;
            case 4:
                setRelay(outputIO_Buff, PUMP);
                resetRelay(outputIO_Buff, COFFEE_BREWER_MOTOR);
                ESP_LOGI("relay", "04 - PUMP");
            break;
            case 5:
                setRelay(outputIO_Buff, COFFEE_GRINDER_MOTOR);
                resetRelay(outputIO_Buff, PUMP);
                ESP_LOGI("relay", "05 - COFFEE_GRINDER_MOTOR");
            break;
            case 6:
                setRelay(outputIO_Buff, COFFEE_RELEASE_MAGNET);
                resetRelay(outputIO_Buff, COFFEE_GRINDER_MOTOR);
                ESP_LOGI("relay", "06 - COFFEE_RELEASE_MAGNET");
            break;
            case 7:
                setRelay(outputIO_Buff, THREE_WAY_VALVE);
                resetRelay(outputIO_Buff, COFFEE_RELEASE_MAGNET);
                ESP_LOGI("relay", "07 - THREE_WAY_VALVE");
            break;
            case 8:
                setRelay(outputIO_Buff, CUP_RELEASE_MOTOR);
                resetRelay(outputIO_Buff, THREE_WAY_VALVE);
                ESP_LOGI("relay", "08 - CUP_RELEASE_MOTOR");
                break;
            case 9:
                setRelay(outputIO_Buff, CUP_STACKER_SHIFT_MOTOR);
                resetRelay(outputIO_Buff, CUP_RELEASE_MOTOR);
                ESP_LOGI("relay", "09 - CUP_STACKER_SHIFT_MOTOR");
                break;
            case 10:
                setRelay(outputIO_Buff, WATER_INLET_VALVE);
                resetRelay(outputIO_Buff, CUP_STACKER_SHIFT_MOTOR);
                ESP_LOGI("relay", "10 - WATER_INLET_VALVE");
                break;
            case 11:
                setRelay(outputIO_Buff, DOSER_DEVICE_1);
                resetRelay(outputIO_Buff, WATER_INLET_VALVE);
                ESP_LOGI("relay", "11 - DOSER_DEVICE_1");
                break;
            case 12:
                setRelay(outputIO_Buff, DOSER_DEVICE_2);
                resetRelay(outputIO_Buff, DOSER_DEVICE_1);
                ESP_LOGI("relay", "12 - DOSER_DEVICE_2");
                break;
            case 13:
                setRelay(outputIO_Buff, WHIPPER);
                resetRelay(outputIO_Buff, DOSER_DEVICE_2);
                ESP_LOGI("relay", "13 - WHIPPER");
                countUp = 0;
            break;
            default:
                countUp = 0;
            break;
        }

        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, outputIO_Buff, 2);                //write to output pins
        */


        vTaskDelay(pdMS_TO_TICKS(1000)); //800
    }
}


static void initQueues(){
    xQueueIntB = xQueueCreate(10, sizeof(int16_t));

    if(xQueueIntB == NULL)
        ESP_LOGE(INIT_ERROR_TAG, "IntB queue initialization error!");
}

static void initGPIO(){
    gpio_reset_pin(CLK_UI_PIN);
    gpio_reset_pin(SH_LD_UI_PIN);
    gpio_reset_pin(IN_SERIAL_UI_PIN);
    gpio_reset_pin(RCK_DISPLAY_PIN);
    gpio_reset_pin(SER_DISPLAY_PIN);
    gpio_reset_pin(MCP23017_IN_RS_PIN);
    gpio_reset_pin(MCP23017_OUT_RS_PIN);
    gpio_reset_pin(MCP23017_INTB_PIN);
    gpio_reset_pin(VOLUMETRIC_PIN);
    gpio_reset_pin(BOILER_PIN);
    gpio_reset_pin(STATUS_LED_PIN);

    gpio_set_direction(CLK_UI_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(SH_LD_UI_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(RCK_DISPLAY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(SER_DISPLAY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(MCP23017_IN_RS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(MCP23017_OUT_RS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(BOILER_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(STATUS_LED_PIN, GPIO_MODE_OUTPUT);

    gpio_set_direction(MCP23017_INTB_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(IN_SERIAL_UI_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(VOLUMETRIC_PIN, GPIO_MODE_INPUT);
    
    gpio_set_intr_type(MCP23017_INTB_PIN, GPIO_INTR_POSEDGE);
    gpio_set_intr_type(VOLUMETRIC_PIN, GPIO_INTR_POSEDGE);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(MCP23017_INTB_PIN, interrupt_handler, (void *)MCP23017_INTB_PIN);
    gpio_isr_handler_add(VOLUMETRIC_PIN, interrupt_handler, (void *)VOLUMETRIC_PIN);


    gpio_set_level(CLK_UI_PIN, 1);
    gpio_set_level(SH_LD_UI_PIN, 0);
    gpio_set_level(RCK_DISPLAY_PIN, 1);
    gpio_set_level(SER_DISPLAY_PIN, 1);
    gpio_set_level(MCP23017_IN_RS_PIN, 1);
    gpio_set_level(MCP23017_OUT_RS_PIN, 1);
    gpio_set_level(BOILER_PIN, 0);
    gpio_set_level(STATUS_LED_PIN, 0);
}

static void initI2C(){

    int i2c_port = I2C_PORT_NUM;

    i2c_config_t i2cConfig = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ
    };

    i2c_param_config(i2c_port, &i2cConfig);

    ESP_ERROR_CHECK(i2c_driver_install(i2c_port, i2cConfig.mode, I2C_RX_BUFF_DISABLE, I2C_TX_BUFF_DISABLE, 0));

}


void app_main(void)
{


    initQueues();

    initGPIO();
    initI2C();

    inputIO_Buff = (uint8_t *)malloc(1);
    outputIO_Buff = (uint8_t *)malloc(2);

    //memset(outputIO_Buff, 1, 2);
    
    memset(outputIO_Buff, 0, 2);

    initI2C_MCP23017_Out();
    initI2C_MCP23017_In();

    //*(outputIO_Buff) = 1;
    //*(outputIO_Buff + 1) = 2;

    if(xTaskCreate(mainTask, "Main task", MAIN_TASK_SIZE, (void*)NULL, MAIN_TASK_PRIORITY, (void*)NULL) != pdPASS)
        ESP_LOGE(MAIN_TASK_TAG, "task cant be created");
    else    
        ESP_LOGI(MAIN_TASK_TAG, "task created!");


    if(xTaskCreate(inputMonitorTask, "Alternative Task", ALTERNATIVE_TASK_SIZE, (void*)NULL,  ALTERNATIVE_TASK_PRIORITY, (void*)NULL) != pdPASS)
        ESP_LOGE(ALTERNATIVE_TASK_TAG, "task cant be created");
    else    
        ESP_LOGI(ALTERNATIVE_TASK_TAG, "task created!");


}

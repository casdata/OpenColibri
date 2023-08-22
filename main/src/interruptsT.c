#include "../include/interruptsT.h"

TaskHandle_t intTaskH = NULL;

static void check4Notifications(uint16_t *ptrCount){
    uint32_t ulNotifiedValue = 0;

    if(xTaskNotifyWait(0xFF, 0, &ulNotifiedValue, pdMS_TO_TICKS(10)) == pdPASS){
        if((ulNotifiedValue & 0x01)){
            *ptrCount = 0;
            ESP_LOGI(INTERRUPTS_TASK_TAG, "Notification: reset pulse count");
        }
    }
}

static void readBytesMCP2307(const uint8_t i2cAddr, uint8_t regAddr, uint8_t *dataBuff, uint8_t numOfBytes){
    i2c_cmd_handle_t cmdHandle = i2c_cmd_link_create();

    i2c_master_start(cmdHandle);
    i2c_master_write_byte(cmdHandle, i2cAddr << 1 | I2C_MASTER_WRITE, I2C_MASTER_ACK);
    i2c_master_write_byte(cmdHandle, regAddr, I2C_MASTER_ACK);
    

    i2c_master_start(cmdHandle);
    i2c_master_write_byte(cmdHandle, i2cAddr << 1 | I2C_MASTER_READ, I2C_MASTER_ACK);
    for(size_t i = 0; i < numOfBytes; i++)
        i2c_master_read_byte(cmdHandle, (dataBuff + i), I2C_MASTER_ACK);

    i2c_master_stop(cmdHandle);


    int cmdReturn = i2c_master_cmd_begin(I2C_PORT_NUM, cmdHandle, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmdHandle);

}


static bool readSW(uint8_t *byteData, const SensorSw swName){
    return (bool)( (*byteData >> (uint8_t)swName) & 1);
}


void initInterruptsTask(){
    BaseType_t xReturned = xTaskCreatePinnedToCore(interruptsTask, "interrupts_task", INTERRUPTS_TASK_SIZE, (void *)NULL, INTERRUPTS_TASK_PRIORITY, &intTaskH, 1);

    if(xReturned == pdFAIL){

        ESP_LOGE(INTERRUPTS_TASK_TAG, "Failed to create task.");

        while(true){

        }
    }
}

static void interruptsTask(void *pvParameters){

    int16_t pinNum;
    uint16_t count = 0;

    uint8_t *inputIO_Buff;

    InputSwStruct inputSwStruct = {false, false, false, false, false, false};

    inputIO_Buff = (uint8_t *)malloc(1);

    while(1){

        check4Notifications(&count);

        if(xQueueReceive(xQueueIntB, &pinNum, pdMS_TO_TICKS(10))){

            switch(pinNum){
                case VOLUMETRIC_PIN:
                    count++;
                    xQueueOverwrite(xQueueInputPulse, (void *) &count);
                break;
                case MCP23017_INTB_PIN:

                    readBytesMCP2307(MCP23017_INPUT_ADDR, 0x13, inputIO_Buff, 1);

                    inputSwStruct.wasteOverflowSw  = readSW(inputIO_Buff, WASTE_OVERFLOW_SW);
                    inputSwStruct.coffeeBrewerSw = readSW(inputIO_Buff, COFFEE_BREWER_SW);
                    inputSwStruct.airBreakSw = readSW(inputIO_Buff, AIR_BREAK_SW);
                    inputSwStruct.coffeeReleaseSw = readSW(inputIO_Buff, COFFEE_RELEASE_MOTOR_CAM);
                    inputSwStruct.cupReleaseSw = readSW(inputIO_Buff, CUP_RELEASE_SW);
                    inputSwStruct.cupSensorSw = readSW(inputIO_Buff, CUP_SENSOR_SW);

                    xQueueOverwrite(xQueueInputsSw, (void *) &inputSwStruct);

                    /*
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
                    */


                break;
            }
        }

     
    }

}
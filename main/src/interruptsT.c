#include "../include/interruptsT.h"

TaskHandle_t intTaskH = NULL;


static void readInputSws(InputSwStruct *inputStruct, uint8_t *iBuff, WaterFlowData *wFlowData){
    readBytesMCP2307(MCP23017_INPUT_ADDR, 0x13, iBuff, 1);

    inputStruct->wasteOverflowSw  = readSW(iBuff, WASTE_OVERFLOW_SW);
    inputStruct->coffeeBrewerSw = readSW(iBuff, COFFEE_BREWER_SW);
    inputStruct->airBreakSw = readSW(iBuff, AIR_BREAK_SW);
    inputStruct->coffeeReleaseSw = readSW(iBuff, COFFEE_RELEASE_MOTOR_CAM);
    inputStruct->cupReleaseSw = readSW(iBuff, CUP_RELEASE_SW);
    inputStruct->cupSensorSw = readSW(iBuff, CUP_SENSOR_SW);

    if(wFlowData->state && !inputStruct->airBreakSw){
        xTaskNotify(controlTaskH, 0x10, eSetBits);                          //close water inlet

        wFlowData->state = false;
        if(wFlowData->alarm){
            //wFlowData->alarm = false;
            xTaskNotify(uiTaskH, 0x01, eSetBits);
        }

        //ESP_LOGI(INTERRUPTS_TASK_TAG, "send noti close w_inlet");
    }

    //ESP_LOGE(INTERRUPTS_TASK_TAG, "wOver: %d brewer: %d air: %d releC: %d ", inputStruct->wasteOverflowSw , 
        //inputStruct->coffeeBrewerSw, inputStruct->airBreakSw, inputStruct->coffeeReleaseSw);

    xQueueOverwrite(xQueueInputsSw, (void *) inputStruct);
}

static void check4Notifications(WaterFlowData *wFlowData, PulseTestData *pulseData){
    uint32_t ulNotifiedValue = 0;

    if(xTaskNotifyWait(0xFF, 0, &ulNotifiedValue, pdMS_TO_TICKS(10)) == pdPASS){
        
        if((ulNotifiedValue & 0x01)){
            wFlowData->state = true;
            wFlowData->alarm = false;
            wFlowData->refTime = esp_timer_get_time();
            ESP_LOGI(INTERRUPTS_TASK_TAG, "Notification: control water flow");
        }
        else if((ulNotifiedValue & 0x02) >> 1){
            pulseData->state = true;
            ESP_LOGI(INTERRUPTS_TASK_TAG, "Notification: calculate pulse time");
        }
        else if((ulNotifiedValue & 0x04) >> 2){
            pulseData->manualReset = true;
            ESP_LOGI(INTERRUPTS_TASK_TAG, "Notification: prepare 4 manual");
        }
        
    }
}

static bool check4PulseCount(uint16_t *ptrCount, uint16_t *ptrCountTarget){
    if(xQueueReceive(xQueueInputPulse, (void *) ptrCountTarget, pdMS_TO_TICKS(10)) == pdPASS){
        *ptrCount = 0;
        return true;
    }
    
    return false;
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
    uint16_t targetCount = 40;
    bool checkingCount = false;

    uint8_t *inputIO_Buff;

    InputSwStruct inputSwStruct = {false, false, false, false, false, false};
    WaterFlowData waterFlowData = {false, false, 0.0f};
    PulseTestData pulseTestData = {false, 0, 0};

    inputIO_Buff = (uint8_t *)malloc(1);

    //xTaskNotify(controlTaskH, 0x02, eSetBits);              //Notify control task that is ready
    ESP_LOGI(INTERRUPTS_TASK_TAG, "ONLINE");

    //xQueueOverwrite(xQueueInputPulse, (void *) &count);
    readInputSws(&inputSwStruct, inputIO_Buff, &waterFlowData);

    while(1){

        check4Notifications(&waterFlowData, &pulseTestData);

        if(pulseTestData.manualReset){
            pulseTestData.manualReset = false;
            checkingCount = false;
        }

        if(!checkingCount){
            checkingCount = check4PulseCount(&count, &targetCount);
            if(checkingCount && pulseTestData.state)
                pulseTestData.refTime = esp_timer_get_time();

        }

        if(waterFlowData.state && !waterFlowData.alarm){
            
            if((esp_timer_get_time() - waterFlowData.refTime) > 8000000){
                waterFlowData.alarm = true;
                xTaskNotify(uiTaskH, 0x02, eSetBits); 
            }
        }
 
        if(xQueueReceive(xQueueIntB, &pinNum, pdMS_TO_TICKS(10))){

            switch(pinNum){
                case VOLUMETRIC_PIN:
                    count++;

                    if(count >= targetCount && checkingCount){
                        xTaskNotify(controlTaskH, 0x08, eSetBits);

                        if(pulseTestData.state){
                            pulseTestData.state = false;

                            pulseTestData.pulseTime = esp_timer_get_time() - pulseTestData.refTime;
                            pulseTestData.pulseTime /= targetCount;

                            xQueueSend(xQueueInputTimePerPulse, (void *) &pulseTestData.pulseTime, portMAX_DELAY);

                            //ESP_LOGI(INTERRUPTS_TASK_TAG, "pulseTime -> %lf", pulseTestData.pulseTime);
                        }

                        checkingCount = false;

                        //ESP_LOGI(INTERRUPTS_TASK_TAG, "%d - %d", count, targetCount);
                    }

                    //ESP_LOGI(INTERRUPTS_TASK_TAG, "%d - %d", count, targetCount);

                break;
                case MCP23017_INTB_PIN:

                    readInputSws(&inputSwStruct, inputIO_Buff, &waterFlowData);

                break;
            }
        }

     
    }

}
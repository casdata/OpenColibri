#include "../include/interruptsT.h"

TaskHandle_t intTaskH = NULL;


static void checkAirBreakSw(uint8_t *iBuff){
    readBytesMCP2307(MCP23017_INPUT_ADDR, 0x13, iBuff, 1);

    if(!readSW(iBuff, AIR_BREAK_SW)){
        xTaskNotify(controlTaskH, 0x10, eSetBits);                          //close water inlet
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


static void readInputSws(InputSwStruct *inputStruct, uint8_t *iBuff, uint8_t *oBuff, WaterFlowData *wFlowData, const bool withNotify){
    readBytesMCP2307(MCP23017_INPUT_ADDR, 0x13, iBuff, 1);

    inputStruct->previousAirBreakSw = inputStruct->airBreakSw;

    inputStruct->wasteOverflowSw  = readSW(iBuff, WASTE_OVERFLOW_SW);
    inputStruct->coffeeBrewerSw = readSW(iBuff, COFFEE_BREWER_SW);
    inputStruct->airBreakSw = readSW(iBuff, AIR_BREAK_SW);
    inputStruct->coffeeReleaseSw = readSW(iBuff, COFFEE_RELEASE_MOTOR_CAM);
    inputStruct->cupReleaseSw = readSW(iBuff, CUP_RELEASE_SW);
    inputStruct->cupSensorSw = readSW(iBuff, CUP_SENSOR_SW);


    if(wFlowData->waterFill){
        ESP_LOGI(INTERRUPTS_TASK_TAG, "on waterFill");

        if(!inputStruct->airBreakSw){

            ESP_LOGI(INTERRUPTS_TASK_TAG, "close water");

            wFlowData->waterFill = false;
            wFlowData->state = false;

            if(withNotify)
                xTaskNotify(controlTaskH, 0x10, eSetBits);                          //close water inlet

            //close valve
            readBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x12, oBuff, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            cleanReadByte4WaterInlet(oBuff);

            writeBytesMCP2307Int(MCP23017_OUTPUT_ADDR, 0x12, oBuff, 1);
            vTaskDelay(pdMS_TO_TICKS(100));

//            if(wFlowData->alarm){
//                wFlowData->alarm = false;
//                xTaskNotify(uiTaskH, 0x01, eSetBits);
//            }

        } else {
            double currentTime = esp_timer_get_time() - wFlowData->waterSafeTime;

            if(wFlowData->failSafe && currentTime > 6000000) {      //6 seg
                ESP_LOGI(INTERRUPTS_TASK_TAG, "emergency close water");

                wFlowData->waterFill = false;
                wFlowData->state = false;

                //close valve
                readBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x12, oBuff, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                cleanReadByte4WaterInlet(oBuff);

                writeBytesMCP2307Int(MCP23017_OUTPUT_ADDR, 0x12, oBuff, 1);
                vTaskDelay(pdMS_TO_TICKS(100));

//                if(wFlowData->alarm){
//                    wFlowData->alarm = false;
//                    xTaskNotify(uiTaskH, 0x01, eSetBits);
//                }

                vTaskDelay(pdMS_TO_TICKS(1000));

                esp_restart();

            }


        }

        //ESP_LOGI(INTERRUPTS_TASK_TAG, "send noti close w_inlet");
    }


    if(inputStruct->airBreakSw && !inputStruct->previousAirBreakSw){
        wFlowData->state = true;
        wFlowData->waterFill = false;

        ESP_LOGI(INTERRUPTS_TASK_TAG, "open water");

        wFlowData->refTime = esp_timer_get_time();

    }

//    if(wFlowData->state && !inputStruct->airBreakSw){
//        xTaskNotify(controlTaskH, 0x10, eSetBits);                          //close water inlet
//
//        wFlowData->state = false;
//        if(wFlowData->alarm){
//            //wFlowData->alarm = false;
//            xTaskNotify(uiTaskH, 0x01, eSetBits);
//        }
//
//        //ESP_LOGI(INTERRUPTS_TASK_TAG, "send noti close w_inlet");
//    }

    //ESP_LOGE(INTERRUPTS_TASK_TAG, "wOver: %d brewer: %d air: %d releC: %d ", inputStruct->wasteOverflowSw , 
        //inputStruct->coffeeBrewerSw, inputStruct->airBreakSw, inputStruct->coffeeReleaseSw);

    xQueueOverwrite(xQueueInputsSw, (void *) inputStruct);
}

static void check4Notifications(WaterFlowData *wFlowData, PulseTestData *pulseData){
    uint32_t ulNotifiedValue = 0;

    if(xTaskNotifyWait(0xFF, 0, &ulNotifiedValue, pdMS_TO_TICKS(10)) == pdPASS){
        
//        if((ulNotifiedValue & 0x01)){                                                               //here
//            wFlowData->state = false;
//            wFlowData->waterFill = false;
//
//            if(wFlowData->alarm){
//                wFlowData->alarm = false;
//                xTaskNotify(uiTaskH, 0x01, eSetBits);
//            }
//        }
        if((ulNotifiedValue & 0x02) >> 1){
            pulseData->state = true;
            ESP_LOGI(INTERRUPTS_TASK_TAG, "Notification: calculate pulse time");
        }
        else if((ulNotifiedValue & 0x04) >> 2){
            pulseData->manualReset = true;
            ESP_LOGI(INTERRUPTS_TASK_TAG, "Notification: prepare 4 manual");
        }
        else if((ulNotifiedValue & 0x08) >> 3){
            wFlowData->failSafe= false;
            ESP_LOGE(INTERRUPTS_TASK_TAG, "Notification: disable water fail-save");
        }
        else if((ulNotifiedValue & 0x10) >> 4){
            wFlowData->failSafe = true;
            wFlowData->waterSafeTime = esp_timer_get_time();
            ESP_LOGI(INTERRUPTS_TASK_TAG, "Notification: enable water fail-save");
        }

        
    }
}

static bool check4PulseCount(uint16_t *ptrCount, uint16_t *ptrCountTarget, uint16_t *ptrPreEndCount){
    if(xQueueReceive(xQueueInputPulse, (void *) ptrCountTarget, pdMS_TO_TICKS(10)) == pdPASS){
        if (*ptrCountTarget >= 1000){
            *ptrCountTarget -= 1000;

            if(*ptrCountTarget > 199)
                *ptrPreEndCount = *ptrCountTarget - ((*ptrCountTarget) * 0.40); //27
            else if (*ptrCountTarget > 149)
                *ptrPreEndCount = *ptrCountTarget - ((*ptrCountTarget) * 0.16); //
            else
                *ptrPreEndCount = *ptrCountTarget - ((*ptrCountTarget) * 0.11); //



//            ESP_LOGI(INTERRUPTS_TASK_TAG, "pre end calculated %d to %d", *ptrCountTarget, *ptrPreEndCount);
        } else
            *ptrPreEndCount = 0;

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

static void writeBytesMCP2307Int(const uint8_t i2cAddr, uint8_t regAddr, uint8_t *dataBuff, uint8_t numOfBytes){
    i2c_cmd_handle_t cmdHandle = i2c_cmd_link_create();

    i2c_master_start(cmdHandle);
    i2c_master_write_byte(cmdHandle, i2cAddr << 1 | I2C_MASTER_WRITE, I2C_MASTER_ACK);
    i2c_master_write_byte(cmdHandle, regAddr, I2C_MASTER_ACK);
    for(size_t i = 0; i < numOfBytes; i++)
        i2c_master_write_byte(cmdHandle, *(dataBuff + i), I2C_MASTER_ACK);

    i2c_master_stop(cmdHandle);

    int cmdReturn = i2c_master_cmd_begin(I2C_PORT_NUM, cmdHandle, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmdHandle);

}


static bool readSW(uint8_t *byteData, const SensorSw swName){
    return (bool)( (*byteData >> (uint8_t)swName) & 1);
}


static void cleanReadByte4WaterInlet(uint8_t *byteData){
    *byteData &= 0xDF;      //exclude byte 5
}

static void setWaterInletBit(uint8_t *byteData){
    *byteData += 0x20;
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
    uint16_t endCount = targetCount;
    bool endCountCheck = false;
    bool checkingCount = false;

    uint8_t *inputIO_Buff;
    uint8_t *outputIO_Buff;

    InputSwStruct inputSwStruct = {false, false, false, false,
                                   false, false, false, 0};
    WaterFlowData waterFlowData = {false, false, false, true, 0.0f, 0.0f, 0.0f, false};
    PulseTestData pulseTestData = {false, 0, 0};

    inputIO_Buff = (uint8_t *)malloc(1);
    outputIO_Buff = (uint8_t *)malloc(1);


    checkAirBreakSw(inputIO_Buff);
    //xTaskNotify(controlTaskH, 0x02, eSetBits);              //Notify control task that is ready
    ESP_LOGI(INTERRUPTS_TASK_TAG, "ONLINE");

    //xQueueOverwrite(xQueueInputPulse, (void *) &count);
    readInputSws(&inputSwStruct, inputIO_Buff, outputIO_Buff, &waterFlowData, true);

    while(1){

        check4Notifications(&waterFlowData, &pulseTestData);

        if(pulseTestData.manualReset){
            pulseTestData.manualReset = false;
            checkingCount = false;
        }

        if(!checkingCount){
            checkingCount = check4PulseCount(&count, &targetCount, &endCount);

            endCountCheck = endCount != 0;

            if(checkingCount && pulseTestData.state)
                pulseTestData.refTime = esp_timer_get_time();

        }

        if(waterFlowData.state){
            double currentTime = esp_timer_get_time() - waterFlowData.refTime;

//            if(!waterFlowData.alarm && currentTime > 8000000){      //8 seg
//                waterFlowData.alarm = true;
//                xTaskNotify(uiTaskH, 0x02, eSetBits);
//            } else
            if (!waterFlowData.waterFill && currentTime > 2000000) {                     //2 seg
                waterFlowData.waterFill = true;

                //open valve
                readBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x12, outputIO_Buff, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                cleanReadByte4WaterInlet(outputIO_Buff);

                setWaterInletBit(outputIO_Buff);
                writeBytesMCP2307Int(MCP23017_OUTPUT_ADDR, 0x12, outputIO_Buff, 1);
                vTaskDelay(pdMS_TO_TICKS(100));

                waterFlowData.waterSafeTime = esp_timer_get_time();

            }
        }


        if(xQueueReceive(xQueueIntB, &pinNum, pdMS_TO_TICKS(10))){

            switch(pinNum){
                case VOLUMETRIC_PIN:
                    count++;

                    if(checkingCount) {
                        if (count >= targetCount) {
                            xTaskNotify(controlTaskH, 0x08, eSetBits);

                            if (pulseTestData.state) {
                                pulseTestData.state = false;

                                pulseTestData.pulseTime = esp_timer_get_time() - pulseTestData.refTime;
                                pulseTestData.pulseTime /= targetCount;

                                xQueueSend(xQueueInputTimePerPulse, (void *) &pulseTestData.pulseTime, portMAX_DELAY);

                                //ESP_LOGI(INTERRUPTS_TASK_TAG, "pulseTime -> %lf", pulseTestData.pulseTime);
                            }

                            checkingCount = false;
                            //ESP_LOGI(INTERRUPTS_TASK_TAG, "%d - %d", count, targetCount);
                        } else if (endCountCheck) {
                            if (count >= endCount) {
                                xTaskNotify(controlTaskH, 0x20, eSetBits);
                                endCountCheck = false;
                            }
                        }
                    }

                    //ESP_LOGI(INTERRUPTS_TASK_TAG, "%d - %d", count, targetCount);

                break;
                case MCP23017_INTB_PIN:

                    ESP_LOGI(INTERRUPTS_TASK_TAG, "int pin detected");
                    readInputSws(&inputSwStruct, inputIO_Buff, outputIO_Buff, &waterFlowData, false);

                    waterFlowData.checkRefTime = esp_timer_get_time();

                break;
            }
        } else {
            double currentTime = esp_timer_get_time() - waterFlowData.checkRefTime;

            if(currentTime > 800000) {      //800 mseg
                waterFlowData.checkRefTime = esp_timer_get_time();
                readInputSws(&inputSwStruct, inputIO_Buff, outputIO_Buff, &waterFlowData, false);
            }

        }

     
    }

}
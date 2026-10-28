#include "../include/interruptsT.h"

TaskHandle_t intTaskH = NULL;


static void checkAirBreakSw(uint8_t *iBuff){
    for(int attempt = 0; attempt < 5; attempt++){
        if(!mcp23017Read(MCP23017_INPUT_ADDR, 0x13, iBuff, 1)){
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if(!readSW(iBuff, AIR_BREAK_SW)){
            setWaterInletOpen(false);
            xTaskNotify(controlTaskH, 0x10, eSetBits);                          //tank full, control may start
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        return;
    }

    ESP_LOGE(INTERRUPTS_TASK_TAG, "airbreak read failed at start");
}


static void readInputSws(InputSwStruct *inputStruct, uint8_t *iBuff, WaterFlowData *wFlowData){
    if(!mcp23017Read(MCP23017_INPUT_ADDR, 0x13, iBuff, 1))
        return;

    inputStruct->previousAirBreakSw = inputStruct->airBreakSw;

    inputStruct->wasteOverflowSw  = readSW(iBuff, WASTE_OVERFLOW_SW);
    inputStruct->coffeeBrewerSw = readSW(iBuff, COFFEE_BREWER_SW);
    inputStruct->airBreakSw = readSW(iBuff, AIR_BREAK_SW);
    inputStruct->coffeeReleaseSw = readSW(iBuff, COFFEE_RELEASE_MOTOR_CAM);
    inputStruct->cupReleaseSw = readSW(iBuff, CUP_RELEASE_SW);
    inputStruct->cupSensorSw = readSW(iBuff, CUP_SENSOR_SW);

    // Switch pressed (bit clear) means the airbreak is full. Close on that level, not on a one-shot flag.
    if(!inputStruct->airBreakSw){
        bool wasOpen = waterInletIsOpen();
        double now = esp_timer_get_time();
        static double lastCloseAttempt = 0;
        bool recheck = (now - lastCloseAttempt) > 2000000.0;

        wFlowData->waterFill = false;
        wFlowData->state = false;

        if(wasOpen || recheck){
            if(setWaterInletOpen(false)){
                lastCloseAttempt = now;
                if(wasOpen)
                    ESP_LOGW(INTERRUPTS_TASK_TAG, "airbreak full, inlet closed");
            }
        }

    } else {
        if(!inputStruct->previousAirBreakSw){
            wFlowData->state = true;
            wFlowData->refTime = esp_timer_get_time();
        }

        if(wFlowData->state){
            double currentTime = esp_timer_get_time() - wFlowData->refTime;

            if(!waterInletIsOpen() && currentTime > 2000000){                     //2 seg
                if(setWaterInletOpen(true)){
                    wFlowData->waterFill = true;
                    wFlowData->waterSafeTime = esp_timer_get_time();
                }

            } else if(waterInletIsOpen() && wFlowData->failSafe){
                double fillTime = esp_timer_get_time() - wFlowData->waterSafeTime;

                if(fillTime > 6000000){      //6 seg
                    ESP_LOGI(INTERRUPTS_TASK_TAG, "emergency close water");
                    setWaterInletOpen(false);
                    wFlowData->waterFill = false;
                    wFlowData->state = false;

                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                }
            }
        }
    }

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
//            ESP_LOGI(INTERRUPTS_TASK_TAG, "Notification: calculate pulse time");
        }
        if((ulNotifiedValue & 0x04) >> 2){
            pulseData->manualReset = true;
//            ESP_LOGI(INTERRUPTS_TASK_TAG, "Notification: prepare 4 manual");
        }
        if((ulNotifiedValue & 0x08) >> 3){
            wFlowData->failSafe= false;
//            ESP_LOGE(INTERRUPTS_TASK_TAG, "Notification: disable water fail-save");
        }
        if((ulNotifiedValue & 0x10) >> 4){
            wFlowData->failSafe = true;
            wFlowData->waterSafeTime = esp_timer_get_time();
//            ESP_LOGI(INTERRUPTS_TASK_TAG, "Notification: enable water fail-save");
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
    uint16_t endCount = targetCount;
    bool endCountCheck = false;
    bool checkingCount = false;

    uint8_t *inputIO_Buff;

    InputSwStruct inputSwStruct = {false, false, false, false,
                                   false, false, false, 0};
    WaterFlowData waterFlowData = {false, false, false, true, 0.0f, 0.0f, 0.0f, false};
    PulseTestData pulseTestData = {false, 0, 0};

    inputIO_Buff = (uint8_t *)malloc(1);

    enableWaterInletOwnership();
    checkAirBreakSw(inputIO_Buff);
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
            checkingCount = check4PulseCount(&count, &targetCount, &endCount);

            endCountCheck = endCount != 0;

            if(checkingCount && pulseTestData.state)
                pulseTestData.refTime = esp_timer_get_time();

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

//                    ESP_LOGI(INTERRUPTS_TASK_TAG, "%d - %d", count, targetCount);

                break;
                case MCP23017_INTB_PIN:

//                    ESP_LOGI(INTERRUPTS_TASK_TAG, "int pin detected");
                    readInputSws(&inputSwStruct, inputIO_Buff, &waterFlowData);

                    waterFlowData.checkRefTime = esp_timer_get_time();

                break;
            }
        } else {
            double currentTime = esp_timer_get_time() - waterFlowData.checkRefTime;

            if(currentTime > 800000) {      //800 mseg
                waterFlowData.checkRefTime = esp_timer_get_time();
                readInputSws(&inputSwStruct, inputIO_Buff, &waterFlowData);
            }

        }

     
    }

}

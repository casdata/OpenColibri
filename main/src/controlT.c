#include "controlT.h"


TaskHandle_t controlTaskH = NULL;

void runDrink1(uint8_t *dataBytes, ContsPowderData *contsPowData){

    grindAndDeliver(dataBytes);

    vTaskDelay(pdMS_TO_TICKS(1000));

    setBrewer2InjectPosition(dataBytes);

    vTaskDelay(pdMS_TO_TICKS(1000));

    xTaskNotify(boilerTaskH, 0x10, eSetBits);
    injecBrewerWater(dataBytes, 142);
    xTaskNotify(boilerTaskH, 0x08, eSetBits);

    vTaskDelay(pdMS_TO_TICKS(2500));

    setBrewer2StartPosition(dataBytes);  

}

void runDrink2(uint8_t *dataBytes, ContsPowderData *contsPowData){

    grindAndDeliver(dataBytes);

    vTaskDelay(pdMS_TO_TICKS(1000));

    setBrewer2InjectPosition(dataBytes);

    vTaskDelay(pdMS_TO_TICKS(1000));

    xTaskNotify(boilerTaskH, 0x10, eSetBits);
    injecBrewerWater(dataBytes, 220);                //78
    xTaskNotify(boilerTaskH, 0x08, eSetBits);

    vTaskDelay(pdMS_TO_TICKS(2500));

    setBrewer2StartPosition(dataBytes);  
      

}

void runDrink3(uint8_t *dataBytes, ContsPowderData *contsPowData){
    //injectPowderPlusWater(dataBytes, 100);
    /*
    setRelay(dataBytes, DOSER_DEVICE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    double tTime = esp_timer_get_time();

    vTaskDelay(pdMS_TO_TICKS(6000));   //11 gr

    ESP_LOGE(CONTROL_TASK_TAG, "time: %lf", (esp_timer_get_time() - tTime));

    resetRelay(dataBytes, DOSER_DEVICE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(100));
    */
   xTaskNotify(boilerTaskH, 0x20, eSetBits);
   injectPowderPlusWater(dataBytes, 60, contsPowData);
   xTaskNotify(boilerTaskH, 0x08, eSetBits);

}

void runDrink4(uint8_t *dataBytes, ContsPowderData *contsPowData){



    injectPowderPlusWater(dataBytes, 128, contsPowData);      
    xTaskNotify(boilerTaskH, 0x08, eSetBits);

}

void runDrink5(uint8_t *dataBytes, ContsPowderData *contsPowData){
    xTaskNotify(boilerTaskH, 0x20, eSetBits);

    vTaskDelay(pdMS_TO_TICKS(7000));

    setRelay(dataBytes, SOLENOID_VALVE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700));
    
    setRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 


    vTaskDelay(pdMS_TO_TICKS(22000));


    resetRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    xTaskNotify(boilerTaskH, 0x08, eSetBits);

    vTaskDelay(pdMS_TO_TICKS(1000));

    resetRelay(dataBytes, SOLENOID_VALVE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    ESP_LOGE(CONTROL_TASK_TAG, "Inject only water: OFF");


    /*
    setBrewer2InjectPosition(dataBytes);

    vTaskDelay(pdMS_TO_TICKS(1000));

    xTaskNotify(boilerTaskH, 0x10, eSetBits);
    injecBrewerWater(dataBytes, 280);
    xTaskNotify(boilerTaskH, 0x08, eSetBits);

    vTaskDelay(pdMS_TO_TICKS(2500));

    setBrewer2StartPosition(dataBytes);

    //injecBrewerWater(dataBytes, 280);   //280 is the max
    */
}

void runDrink6(uint8_t *dataBytes, ContsPowderData *contsPowData){
    setBrewer2InjectPosition(dataBytes);

    vTaskDelay(pdMS_TO_TICKS(5000));

    setBrewer2StartPosition(dataBytes);
}

void runDrink7(uint8_t *dataBytes, ContsPowderData *contsPowData){

}

void runDrink8(uint8_t *dataBytes, ContsPowderData *contsPowData){
    //calculatePulseTime(dataBytes);
}

void writeBytesMCP2307(const uint8_t i2cAddr, uint8_t regAddr, uint8_t *dataBuff, uint8_t numOfBytes){
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

static bool bitChangedInByte(uint8_t *byteData, const uint8_t bitPos, const bool newValue){
    bool bitA = (bool)(((*byteData) >> bitPos) & 1);

    if(bitA == newValue)
        return false;
    else
        return true;
}

//Used to check if the desires relay state has already changed, this to now over use setRelay function
static bool differentRelayState(uint8_t *byteData, const OutputRelays outputRelays, const bool newValue){
    bool itsDifferent = false;

    switch(outputRelays){
        case SOLENOID_VALVE_2:
            itsDifferent = bitChangedInByte(byteData + 1, 6, newValue);
            break;
        case SOLENOID_VALVE_1:
            itsDifferent = bitChangedInByte(byteData + 1, 5, newValue);
            break;
        case COFFEE_BREWER_MOTOR:
            itsDifferent = bitChangedInByte(byteData + 1, 4, newValue);
            break;
        case PUMP:
            itsDifferent = bitChangedInByte(byteData + 1, 3, newValue);
            break;
        case COFFEE_GRINDER_MOTOR:
            itsDifferent = bitChangedInByte(byteData + 1, 2, newValue);
            break;
        case COFFEE_RELEASE_MAGNET:
            itsDifferent = bitChangedInByte(byteData + 1, 1, newValue);
            break;
        case THREE_WAY_VALVE:
            itsDifferent = bitChangedInByte(byteData + 1, 0, newValue);
            break;
        case CUP_RELEASE_MOTOR:
            itsDifferent = bitChangedInByte(byteData, 7, newValue);
            break;
        case CUP_STACKER_SHIFT_MOTOR:
            itsDifferent = bitChangedInByte(byteData, 6, newValue);
            break;
        case WATER_INLET_VALVE:
            itsDifferent = bitChangedInByte(byteData, 5, newValue);
            break;
        case DOSER_DEVICE_1:
            itsDifferent = bitChangedInByte(byteData, 4, newValue);
            break;
        case DOSER_DEVICE_2:
            itsDifferent = bitChangedInByte(byteData, 3, newValue);
            break;
        case WHIPPER:
            itsDifferent = bitChangedInByte(byteData, 1, newValue);
            break;
    }

    return itsDifferent;
}

static void setBit2Byte(uint8_t *byteData, const uint8_t bitPos){
    *byteData |= (1 << bitPos);
}

static void clearBit2Byte(uint8_t *byteData, const uint8_t bitPos){
    *byteData &= ~(1 << bitPos);
}

static void setRelay(uint8_t *byteData, const OutputRelays outputRelays){
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

static void resetRelay(uint8_t *byteData, const OutputRelays outputRelays){
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

static void checkAirBreak(uint8_t *dataBytes){

    InputSwStruct inputSwStruct;    

    if(xQueuePeek(xQueueInputsSw, (void *) &inputSwStruct, pdMS_TO_TICKS(60))){

        if(inputSwStruct.airBreakSw){

            setRelay(dataBytes, WATER_INLET_VALVE);
            writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  
            ESP_LOGW(CONTROL_TASK_TAG, "OPEN WATER_INLET_VALVE");

            xTaskNotify(intTaskH, 0x01, eSetBits);                  //notify interrupt task for water flow control

            vTaskDelay(pdMS_TO_TICKS(100)); 

            bool onWait = true;
            uint32_t ulNotifiedValue = 0;
            
            do{

                xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, portMAX_DELAY);

                if((ulNotifiedValue & 0x10) >> 4){

                    resetRelay(dataBytes, WATER_INLET_VALVE);
                    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  
                    vTaskDelay(pdMS_TO_TICKS(30));

                    ESP_LOGW(CONTROL_TASK_TAG, "CLOSE WATER_INLET_VALVE");

                    onWait = false;
                }

            }while(onWait);

            ESP_LOGI(CONTROL_TASK_TAG, "END AIRBREAK CHECK");
        }
    }

}

void resetBrewer(uint8_t *dataBytes){
    InputSwStruct inputSwStruct;

    setRelay(dataBytes, COFFEE_BREWER_MOTOR);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  

    int countIt = 0;

    do{
        if(xQueuePeek(xQueueInputsSw, (void *) &inputSwStruct, pdMS_TO_TICKS(60))){
            if(!inputSwStruct.coffeeBrewerSw){
                countIt++;
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            if(countIt > 1){
                vTaskDelay(pdMS_TO_TICKS(2700));
                resetRelay(dataBytes, COFFEE_BREWER_MOTOR);
                writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  
            }

        }

        vTaskDelay(pdMS_TO_TICKS(10));

    }while(countIt < 2);

}

void setBrewer2StartPosition(uint8_t *dataBytes){

    setRelay(dataBytes, COFFEE_BREWER_MOTOR);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);     

    vTaskDelay(pdMS_TO_TICKS(3700));

    resetRelay(dataBytes, COFFEE_BREWER_MOTOR);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  
    
}

void setBrewer2InjectPosition(uint8_t *dataBytes){
    InputSwStruct inputSwStruct;

    setRelay(dataBytes, COFFEE_BREWER_MOTOR);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  

    bool waiting = true;

    do{
        if(xQueuePeek(xQueueInputsSw, (void *) &inputSwStruct, pdMS_TO_TICKS(60))){
            if(!inputSwStruct.coffeeBrewerSw)
                waiting = false;
        }    

        vTaskDelay(pdMS_TO_TICKS(10));

    }while(waiting);

    resetRelay(dataBytes, COFFEE_BREWER_MOTOR);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

}

void injecBrewerWater(uint8_t *dataBytes, const uint16_t pulses){

    uint32_t ulNotifiedValue = 0;
    bool onWait = true;

    ESP_LOGI(CONTROL_TASK_TAG, "Inject brewer water: ON");

    setRelay(dataBytes, THREE_WAY_VALVE);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700));
    
    setRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 


    //vTaskDelay(pdMS_TO_TICKS(22000));
    xQueueSend(xQueueInputPulse, (void *) &pulses, portMAX_DELAY);

    do{
        xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, portMAX_DELAY);

        if((ulNotifiedValue & 0x08) >> 3){
            onWait = false;
            ESP_LOGI(CONTROL_TASK_TAG, "TURNING OFF PUMP");
        }

    }while(onWait);


    resetRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(1000));

    resetRelay(dataBytes, THREE_WAY_VALVE);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    ESP_LOGE(CONTROL_TASK_TAG, "Inject brewer water: OFF");
}

static void injectPowderPlusWater(uint8_t *dataBytes, const uint16_t pulses, ContsPowderData *contsPowData){

    uint32_t ulNotifiedValue = 0;
    bool onWait = true; 
    bool gramsTimeGreaterThanHeatTime = true;
    bool withPreGramsTime = false;
    bool preHeatOn = false;
    bool prePowderOn = false;

    int targetGrams = 40;                                                       //** 40 gr
    double preHeatTime = 7000000;                                               //** 7 sec


    double refTime = 0;

    double totalTime = contsPowData->timePerPulse * pulses;
    double gramsTime = (targetGrams / contsPowData->grPerSecCon1) * 1000000;
    double onlyGramsTime = gramsTime - totalTime;

    ESP_LOGI(CONTROL_TASK_TAG, "Inject powder water: ON - %lf - %lf = %lf", totalTime, gramsTime, onlyGramsTime);


    setRelay(dataBytes, DOSER_DEVICE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(21818));

    resetRelay(dataBytes, DOSER_DEVICE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    /*

    setRelay(dataBytes, SOLENOID_VALVE_2);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700));

    if(onlyGramsTime > 0){
        withPreGramsTime = true;

        if(onlyGramsTime <= preHeatTime){
            gramsTimeGreaterThanHeatTime = false;
            refTime = preHeatTime;
            xTaskNotify(boilerTaskH, 0x20, eSetBits);                                       //set boiler to HOT MAX
        }
        else{
            refTime = onlyGramsTime;
            setRelay(dataBytes, DOSER_DEVICE_1);
            writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 
        }

    }
    else{
        refTime = preHeatTime;
        xTaskNotify(boilerTaskH, 0x20, eSetBits);                                       //set boiler to HOT MAX
    }



    do{
        vTaskDelay(pdMS_TO_TICKS(100));  

        refTime -= 100000;

        if(withPreGramsTime){
            if(gramsTimeGreaterThanHeatTime){
                if(refTime <= preHeatTime && !preHeatOn){
                    preHeatOn = true;
                    xTaskNotify(boilerTaskH, 0x20, eSetBits);                                       //set boiler to HOT MAX    
                }
            }
            else{
                if(refTime <= onlyGramsTime && !prePowderOn){
                    prePowderOn = true;
                    setRelay(dataBytes, DOSER_DEVICE_1);
                    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 
                }
            }
        
        }
        
        if(refTime <= 0)
            onWait = false;

    }while(onWait);

    onWait = true;

    //vTaskDelay(pdMS_TO_TICKS(7000));

    if(!withPreGramsTime){
        setRelay(dataBytes, DOSER_DEVICE_1);
        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

        vTaskDelay(pdMS_TO_TICKS(700)); 
    }

    
    setRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    refTime = esp_timer_get_time();

    vTaskDelay(pdMS_TO_TICKS(700));

    setRelay(dataBytes, WHIPPER);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 


    xQueueSend(xQueueInputPulse, (void *) &pulses, portMAX_DELAY);

    do{
        xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, portMAX_DELAY);

        if((ulNotifiedValue & 0x08) >> 3){
            onWait = false;

            ESP_LOGI(CONTROL_TASK_TAG, "TURNING OFF PUMP - %lf", (esp_timer_get_time() - refTime));
        }

    }while(onWait);


    resetRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700));

    resetRelay(dataBytes, DOSER_DEVICE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    ESP_LOGI(CONTROL_TASK_TAG, "EXTRA TIME - %lf", (esp_timer_get_time() - refTime));

    vTaskDelay(pdMS_TO_TICKS(700));

    resetRelay(dataBytes, SOLENOID_VALVE_2);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(2000));

    resetRelay(dataBytes, WHIPPER);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  
    */

    ESP_LOGE(CONTROL_TASK_TAG, "Inject powder water: OFF");
}



/*
static void injectPowderPlusWater(uint8_t *dataBytes, const uint16_t pulses, ContsPowderData *contsPowData){

    uint32_t ulNotifiedValue = 0;
    bool onWait = true;
    int targetGrams = 40;
    double refTime = 0;

    double totalTime = contsPowData->timePerPulse * pulses;

    ESP_LOGI(CONTROL_TASK_TAG, "Inject powder water: ON - %lf", totalTime);

    setRelay(dataBytes, SOLENOID_VALVE_2);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700)); //700

    setRelay(dataBytes, DOSER_DEVICE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 
    
    setRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    refTime = esp_timer_get_time();

    vTaskDelay(pdMS_TO_TICKS(700));

    setRelay(dataBytes, WHIPPER);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 


    //vTaskDelay(pdMS_TO_TICKS(22000));
    xQueueSend(xQueueInputPulse, (void *) &pulses, portMAX_DELAY);

    do{
        xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, portMAX_DELAY);

        if((ulNotifiedValue & 0x08) >> 3){
            onWait = false;

            ESP_LOGI(CONTROL_TASK_TAG, "TURNING OFF PUMP - %lf", (esp_timer_get_time() - refTime));
        }

    }while(onWait);


    resetRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700));

    resetRelay(dataBytes, DOSER_DEVICE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    ESP_LOGI(CONTROL_TASK_TAG, "EXTRA TIME - %lf", (esp_timer_get_time() - refTime));

    vTaskDelay(pdMS_TO_TICKS(700));

    resetRelay(dataBytes, SOLENOID_VALVE_2);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(2000));

    resetRelay(dataBytes, WHIPPER);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    ESP_LOGE(CONTROL_TASK_TAG, "Inject powder water: OFF");


}
*/


static void grindAndDeliver(uint8_t *dataBytes){

    bool wait2Finish = true;
    bool noCoffee = false;

    double refTime = 0;
    double cTime = 0;

    InputSwStruct inputSwStruct;

    setRelay(dataBytes, COFFEE_GRINDER_MOTOR);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    refTime = esp_timer_get_time();

    do{

        if(xQueuePeek(xQueueInputsSw, (void *) &inputSwStruct, pdMS_TO_TICKS(30))){
            
            if(inputSwStruct.coffeeReleaseSw)
                wait2Finish = false;
        }

        if(wait2Finish){
            cTime = esp_timer_get_time() - refTime;
            //ESP_LOGW(BOILER_TASK_TAG, "grinder time: %lf", cTime);
            
            if( cTime >= 16000000){      //16 seg
                noCoffee = true;
                wait2Finish = false;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200));

    }while(wait2Finish);

    resetRelay(dataBytes, COFFEE_GRINDER_MOTOR);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700));

    if(noCoffee)
        ESP_LOGE(CONTROL_TASK_TAG, "ERROR: NO COFFEE!!!");
    else{
        setRelay(dataBytes, COFFEE_RELEASE_MAGNET);
        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);    

        vTaskDelay(pdMS_TO_TICKS(1000));

        resetRelay(dataBytes, COFFEE_RELEASE_MAGNET);
        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  

        ESP_LOGI(CONTROL_TASK_TAG, "Coffee released :)");
    }

}

void calculatePulseTime(uint8_t *dataBytes){

    bool onWait = true;
    uint16_t pulses = 20;
    uint32_t ulNotifiedValue = 0;
    double pulseTimeArray[] = {0, 0, 0};


    for(size_t i = 0; i < 4; i++){

        if(i != 0)
            xTaskNotify(intTaskH, 0x02, eSetBits);

        setRelay(dataBytes, SOLENOID_VALVE_2);
        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

        vTaskDelay(pdMS_TO_TICKS(700)); 
    
        setRelay(dataBytes, PUMP);
        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

        xQueueSend(xQueueInputPulse, (void *) &pulses, portMAX_DELAY);

        do{
            xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, portMAX_DELAY);

            if((ulNotifiedValue & 0x08) >> 3)
                onWait = false;

        }while(onWait);

        resetRelay(dataBytes, PUMP);
        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

        vTaskDelay(pdMS_TO_TICKS(1000));

        resetRelay(dataBytes, SOLENOID_VALVE_2);
        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

        onWait = true;

        if(i != 0){
            do{
                vTaskDelay(pdMS_TO_TICKS(200));

                if(xQueueReceive(xQueueInputTimePerPulse, (void *) &pulseTimeArray[i-1], pdMS_TO_TICKS(10)) == pdPASS){
                    onWait = false;
                    ESP_LOGI(CONTROL_TASK_TAG, "pulseTime -> %lf", pulseTimeArray[i-1]);
                }

            }while(onWait);
        }

        pulses = 100;

        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    double totalTime = pulseTimeArray[0];

    for(size_t i = 1; i < 3; i++)
        totalTime += pulseTimeArray[i];

    totalTime /= 3;

    ESP_LOGW(CONTROL_TASK_TAG, "times: %lf - %lf - %lf = %lf", 
                pulseTimeArray[0], pulseTimeArray[1], pulseTimeArray[2], totalTime);


    ESP_LOGE(CONTROL_TASK_TAG, "Pulse time test: OFF");


}

static void waitMachine2Start(uint8_t *dataBytes){
    checkAirBreak(dataBytes);

}

static void startBoilerTask(){
    xTaskNotify(boilerTaskH, 0x04, eSetBits);         
}

static void waitBoiler2Start(){
    uint32_t ulNotifiedValue = 0;

    bool waiting = true;

    do{
        xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, portMAX_DELAY);

        if((ulNotifiedValue & 0x04) >> 2)
            waiting = false;

    }while(waiting);
}

static void syncronizeAllTasks(){
    uint32_t ulNotifiedValue = 0;
    bool uiT_off = true;            //ui task
    bool inT_off = true;            //interrupts task
    bool boT_off = true;            //boiler task


    do{
        xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, portMAX_DELAY);

        if(ulNotifiedValue & 0x01)
            uiT_off = false;

        if((ulNotifiedValue & 0x02) >> 1)
            inT_off = false;

        if((ulNotifiedValue & 0x04) >> 2)
            boT_off = false;


    }while(uiT_off || inT_off || boT_off);


}

static void checkQueuesFromUi(ControlState *controlState){

    InputBtnStruct tempInputBtnStruct;

    if(xQueueReceive(xQueueInputBtns, &tempInputBtnStruct, pdMS_TO_TICKS(10)) == pdPASS){

        if(*controlState == IDLE_C){

            if(tempInputBtnStruct.btn0)
                *controlState = DRINK_1_C;
            else if(tempInputBtnStruct.btn1)
                *controlState = DRINK_2_C;
            else if(tempInputBtnStruct.btn2)
                *controlState = DRINK_3_C;
            else if(tempInputBtnStruct.btn3)
                *controlState = DRINK_4_C;
            else if(tempInputBtnStruct.btn4)
                *controlState = DRINK_5_C;
            else if(tempInputBtnStruct.btn5)
                *controlState = DRINK_6_C;
            else if(tempInputBtnStruct.btn6)
                *controlState = DRINK_7_C;
            else if(tempInputBtnStruct.btn7)
                *controlState = DRINK_8_C;
            else if(tempInputBtnStruct.btnA)
                *controlState = CLEAN_C;
            else if(tempInputBtnStruct.btnB)
                *controlState = MAINTENANCE_C;
            
            /*
            ESP_LOGE(CONTROL_TASK_TAG, "-> %d %d %d %d %d %d %d %d %d %d",
                tempInputBtnStruct.btn0, tempInputBtnStruct.btn1, tempInputBtnStruct.btn2, 
                tempInputBtnStruct.btn3, tempInputBtnStruct.btn4, tempInputBtnStruct.btn5,
                tempInputBtnStruct.btn6, tempInputBtnStruct.btn7, tempInputBtnStruct.btnA, 
                tempInputBtnStruct.btnB);
                */
        }
        

    }
}


void initI2C_MCP23017_Out(){

    uint8_t *dataBuff = (uint8_t *)malloc(2);

    memset(dataBuff, 0, 2);

    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x0, dataBuff, 2);                //Set output pins

    vTaskDelay(pdMS_TO_TICKS(20));
 
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBuff, 2);               //Clear all output pins.

    vTaskDelay(pdMS_TO_TICKS(20));

    free(dataBuff);

}

void initI2C_MCP23017_In(){
    uint8_t *dataBuff = malloc(1);

    memset(dataBuff, 0x3f, 1);

    writeBytesMCP2307(MCP23017_INPUT_ADDR, 0x05, dataBuff, 1);              //GPINTENB

    memset(dataBuff, 0x2, 1);

    writeBytesMCP2307(MCP23017_INPUT_ADDR, 0X0b, dataBuff, 1);

    free(dataBuff);

}


void initControlTask(){
    BaseType_t xReturned = xTaskCreate(controlTask, "control_task", CONTROL_TASK_SIZE, (void *)NULL, CONTROL_TASK_PRIORITY, &controlTaskH);

    if(xReturned == pdFAIL){
        ESP_LOGE(CONTROL_TASK_TAG, "Failed to create task.");

        while(true){

        }
    }
}


static void controlTask(void *pvParameters){

    float boilerT = 0;
    ControlState controlState = IDLE_C;
    ContsPowderData conPowderData = {1.8333f, 1.8333f, 1.8333f, 80737.37};

    uint8_t *outputIO_Buff = (uint8_t *)malloc(2);

    memset(outputIO_Buff, 0, 2);

    checkAirBreak(outputIO_Buff);

    xTaskNotify(uiTaskH, 0x020, eSetBits);                  //Set ui task to booting state

    startBoilerTask();

    waitBoiler2Start();

    resetBrewer(outputIO_Buff);

    //syncronizeAllTasks();

    ESP_LOGI(CONTROL_TASK_TAG, "ONLINE");


    while(true){

        checkQueuesFromUi(&controlState);

        switch(controlState){
            case IDLE_C:

            break;
            case MAINTENANCE_C:
                controlState = IDLE_C;
            break;
            case CLEAN_C:
                controlState = IDLE_C;
            break;
            case DRINK_1_C:
                runDrink1(outputIO_Buff, &conPowderData);
                controlState = IDLE_C;
            break;
            case DRINK_2_C:
                runDrink2(outputIO_Buff, &conPowderData);
                controlState = IDLE_C;
            break;
            case DRINK_3_C:
                runDrink3(outputIO_Buff, &conPowderData);
                controlState = IDLE_C;
            break;
            case DRINK_4_C:
                runDrink4(outputIO_Buff, &conPowderData);
                controlState = IDLE_C;
            break;
            case DRINK_5_C:
                runDrink5(outputIO_Buff, &conPowderData);
                controlState = IDLE_C;
            break;
            case DRINK_6_C:
                runDrink6(outputIO_Buff, &conPowderData);
                controlState = IDLE_C;
            break;
            case DRINK_7_C:
                runDrink7(outputIO_Buff, &conPowderData);
                controlState = IDLE_C;
            break;
            case DRINK_8_C:
                runDrink8(outputIO_Buff, &conPowderData);
                controlState = IDLE_C;
            break;
        }

        checkAirBreak(outputIO_Buff);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    
    

}
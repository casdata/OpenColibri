#include "controlT.h"


TaskHandle_t controlTaskH = NULL;


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

static void resetPulseCount(){
    xTaskNotify(intTaskH, 0x01, eSetBits);
}

static void checkAirBreak(uint8_t *dataBytes){

    InputSwStruct inputSwStruct;    

    bool doCheck = true;

    do{
        if(xQueuePeek(xQueueInputsSw, (void *) &inputSwStruct, pdMS_TO_TICKS(60))){
            if(inputSwStruct.airBreakSw){

                if(differentRelayState(dataBytes, WATER_INLET_VALVE, true)){
                    setRelay(dataBytes, WATER_INLET_VALVE);
                    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  
                    ESP_LOGW(CONTROL_TASK_TAG, "OPEN WATER_INLET_VALVE");

                    vTaskDelay(pdMS_TO_TICKS(30));
                }
            }
            else{

                if(differentRelayState(dataBytes, WATER_INLET_VALVE, false)){
                    resetRelay(dataBytes, WATER_INLET_VALVE);
                    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  
                    ESP_LOGW(CONTROL_TASK_TAG, "CLOSE WATER_INLET_VALVE");

                    vTaskDelay(pdMS_TO_TICKS(30));
                }

                doCheck = false;
            }

        }

    }while(doCheck);

}

static void waitMachine2Start(uint8_t *dataBytes){
    void checkAirBreak(outputRlyStruct);

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

    ControlState controlState = IDLE_C;

    uint8_t *outputIO_Buff = (uint8_t *)malloc(2);

    memset(outputIO_Buff, 0, 2);

    waitMachine2Start(outputIO_Buff);

    while(true){

        switch(controlState){
            case IDLE_C:
            
            break;
            case MAINTENANCE_C:

            break;
            case CLEANING_C:

            break;
            case DRINK_1_C:

            break;
            case DRINK_2_C:

            break;
            case DRINK_3_C:

            break;
            case DRINK_4_C:

            break;
            case DRINK_5_C:

            break;
            case DRINK_6_C:

            break;
            case DRINK_7_C:

            break;
            case DRINK_8_C:

            break;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

}
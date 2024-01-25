//
// Created by castdata on 8/14/23.
//
#include "boilerT.h"

TaskHandle_t boilerTaskH = NULL;
QueueHandle_t xQueueUart = NULL;


static bool readMCP3421_ADC(uint16_t *adcValue){

    i2c_cmd_handle_t  cmdHandle = i2c_cmd_link_create();

    i2c_master_start(cmdHandle);
    i2c_master_write_byte(cmdHandle, MCP3421_ADC_ADDR << 1 | I2C_MASTER_WRITE, I2C_MASTER_ACK);
    i2c_master_write_byte(cmdHandle, (uint8_t)(0b00010000), I2C_MASTER_ACK);
    i2c_master_stop(cmdHandle);

    int cmdReturn  = i2c_master_cmd_begin(I2C_PORT_NUM, cmdHandle, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmdHandle);

    vTaskDelay(30 / portTICK_PERIOD_MS);

    if(cmdReturn == ESP_OK){
        uint8_t dataByte0, dataByte1, dataByte2;

        cmdHandle = i2c_cmd_link_create();
        i2c_master_start(cmdHandle);
        i2c_master_write_byte(cmdHandle, MCP3421_ADC_ADDR << 1 | I2C_MASTER_READ, I2C_MASTER_ACK);
        i2c_master_read_byte(cmdHandle, &dataByte2, I2C_MASTER_ACK);
        i2c_master_read_byte(cmdHandle, &dataByte1, I2C_MASTER_ACK);
        i2c_master_read_byte(cmdHandle, &dataByte0, I2C_MASTER_NACK);
        i2c_master_stop(cmdHandle);

        cmdReturn = i2c_master_cmd_begin(I2C_PORT_NUM, cmdHandle, 1000 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmdHandle);

        if(cmdReturn == ESP_OK){
            *adcValue = (dataByte2 << 8) + dataByte1;
            return true;
        }
    }

    return false;

}


static void readTemp(BoilerStructData *boilerStructData){
    uint16_t adcValue;

    if(readMCP3421_ADC(&adcValue)){

        float thermistorVoltage = adcValue * 0.001f;

        uint16_t thermistorValue = (thermistorVoltage * 12000)/(3.3f - thermistorVoltage);

        //ESP_LOGI(TEMP_TASK_TAG, "Raw temp: %d - %.4f - %d", adcValue, thermistorVoltage, thermistorValue); //1649

        float temp = log(thermistorValue);
        temp = (1.0f / (thermA + (thermB * temp + (thermC * temp * temp * temp))));
        temp -= 273.15f;

        //*thermTemp = temp;
        boilerStructData->thermistorTemp = temp;
    }
}



static void wait4Control(){
    uint32_t ulNotifiedValue = 0;
    bool onWait = true;

    do{

        xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, portMAX_DELAY);

        if((ulNotifiedValue & 0x04) >> 2)
            onWait = false;

    }while(onWait);

}


static void wait4BoilerTempValue(float *targetTemperature, PID_DataStruct *pidDataStruct){
    //portMAX_DELAY
    uint8_t tempTemp = 0;

    if(xQueueReceive(xQueueData2Boiler, &tempTemp, portMAX_DELAY) == pdPASS){
        *targetTemperature = (float)tempTemp;
        pidDataStruct->setPoint = *targetTemperature - 3.0f;
    }
    
}

static void check4BoilerTempUpdate(float *targetTemperature, PID_DataStruct *pidDataStruct){
    uint8_t tempTemp = 0;

    if(xQueueReceive(xQueueData2Boiler, &tempTemp, pdMS_TO_TICKS(15)) == pdPASS){
        *targetTemperature = (float)tempTemp;
        pidDataStruct->setPoint = *targetTemperature - 3.0f;
    }
}

static void checkNotifications4Boiler(BoilerState *bState){
    uint32_t ulNotifiedValue = 0;

    if(xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, pdMS_TO_TICKS(15))){
        if(ulNotifiedValue & 0x001){
            ESP_LOGW(UI_TASK_TAG, "Boiler Ready NOTIFICATION");
            
        }

        if((ulNotifiedValue & 0x002) >> 1){
            ESP_LOGW(UI_TASK_TAG, "Turn Boiler Off NOTIFICATION");

        }

        if((ulNotifiedValue & 0x004) >> 2){
            ESP_LOGW(UI_TASK_TAG, "Turn Boiler On NOTIFICATION");

        }

        if((ulNotifiedValue & 0x008) >> 3){
            ESP_LOGW(UI_TASK_TAG, "Set Boiler IDLE Mode NOTIFICATION");
            *bState = HOT_2_IDLE_B;
        }

        if((ulNotifiedValue & 0x010) >> 4){
            ESP_LOGW(UI_TASK_TAG, "Set Boiler HOT Mode NOTIFICATION");
            *bState = IDLE_2_HOT_B;
        }

        if((ulNotifiedValue & 0x020) >> 5){
            ESP_LOGW(UI_TASK_TAG, "Set Boiler HOT MAX Mode NOTIFICATION");
            *bState = IDLE_2_HOT_MAX_B;
        }
    }

}

void getMessage2Send2Nextion(char *strData, BoilerStructData *boilerStructData){
    memset(strData, 0, 60);

    static char tailStrChar[4] = {(char)255, (char)255, (char)255, '\0'};
    char *intData = (char *)malloc(10);
    memset(intData, 0, 10);

    memset(strData, 0, 10);
    if(boilerStructData->toSend){
        strcpy(strData, "x1.val=");
        itoa((int)boilerStructData->thermistorTemp, intData, 10);
    }
    else{
        strcpy(strData, "x0.val=");

        int inPercent = (int)((100 * boilerStructData->boilerDuty)/8191.0f);

        itoa(inPercent, intData, 10);

    }

    strcat(strData, intData);
    strcat(strData, tailStrChar);

    free(intData);
}


bool checkDataFromNextionPID_Tunner(PID_DataStruct *pidDataStruct){
    bool newData = false;
    static uart_event_t uEvent;

    if(xQueueReceive(xQueueUart, (void *)&uEvent, pdMS_TO_TICKS(10))){

        char *tempData = malloc(BUFF_SIZE);
        size_t buffSize = 0;

        memset(tempData, 0, BUFF_SIZE);

        switch(uEvent.type){
            case UART_FIFO_OVF:                                                                               
                uart_flush(UART_PORT_N);
                xQueueReset(xQueueUart);
            break;
            case UART_BUFFER_FULL:                                                                       
                uart_flush(UART_PORT_N);
                xQueueReset(xQueueUart);
            break;
            case UART_PATTERN_DET:
                uart_get_buffered_data_len(UART_PORT_N, &buffSize);

                if(uart_pattern_pop_pos(UART_PORT_N) != -1)
                    uart_read_bytes(UART_PORT_N, tempData, buffSize, pdMS_TO_TICKS(100));
                else
                    uart_flush(UART_PORT_N);

                //uart_flush(UART_PORT_N);
                //xQueueReset(xQueueUart);
            break;
            case UART_DATA:
                
            break;
            case UART_DATA_BREAK:

            break;
            case UART_PARITY_ERR:

            break;
            case UART_FRAME_ERR:
            
            break;
            case UART_BREAK:

            break;
            case UART_EVENT_MAX:

            break;
            default:
                ESP_LOGI(BOILER_TASK_TAG, "uart e-type: %d", uEvent.type);
            break;
        }

        if(buffSize > 0){
            int startIndex = -1;
            int endIndex = -1;
            NextionData nextionData;
            bool resetPID_Data = false;

            for(size_t i = 0; i < buffSize; i++){

                if(tempData[i] == 'r'){
                    resetPID_Data = true;
                    break;
                }
                else if(tempData[i] == '='){
                    if(pidDataStruct->controllerState)
                        pidDataStruct->controllerState = false;
                    else
                        pidDataStruct->controllerState = true;
                    break;
                }
                else if(tempData[i] == 'p'){
                    startIndex = i;
                    nextionData = P_DATA;
                }
                else if(tempData[i] == 'i'){
                    startIndex = i;
                    nextionData = I_DATA;
                }
                else if(tempData[i] == 'd'){
                    startIndex = i;
                    nextionData = D_DATA;
                }
                else if(tempData[i] == 's'){
                    startIndex = i;
                    nextionData = S_DATA;
                }
                else if(tempData[i] == '~'){
                    endIndex = i;
                    break;
                }
            }

            if(startIndex != -1){

                startIndex++;
                
                char *tempStr = malloc(10);
                memset(tempStr, 0, 10);


                for(size_t i = startIndex; i < endIndex; i++)
                    tempStr[i - startIndex] = tempData[i];

                tempStr[endIndex - startIndex] = '\0';

                int intData = atoi(tempStr);
                float floatData = 0;

            
                if(nextionData != S_DATA)
                    floatData = intData * 0.01f;
                

                switch(nextionData){
                    case P_DATA:
                        pidDataStruct->kP = floatData;   
                    break;
                    case I_DATA:
                        pidDataStruct->kI = floatData;
                    break;
                    case D_DATA:
                        pidDataStruct->kD = floatData;
                    break;
                    default:                //S_DATA
                        pidDataStruct->setPoint = intData;
                    break;
                }

                newData = true;
                free(tempStr);
            }
            else if(resetPID_Data){
                // pidDataStruct->kP = 0;
                // pidDataStruct->kI = 0;
                // pidDataStruct->kD = 0;
                pidDataStruct->outputSum = 0;
                pidDataStruct->lastInput = 0;
                pidDataStruct->error = 0;
                pidDataStruct->pError = 0;
                pidDataStruct->pidP = 0;
                pidDataStruct->pidI = 0;
                pidDataStruct->pidD = 0;

                ESP_LOGE(BOILER_TASK_TAG, "PID DATA CLEARED!!!");
                newData = true;
            }
        }
        free(tempData);
    }
    return newData;
}


void computePID(PID_DataStruct *pidDataStruct, float inputData, float *outputData){

    float eTime = 0;

    //ESP_LOGI(BOILER_TASK_TAG, "%lf - %lf", tempTime, eTime);
    
    pidDataStruct->error = pidDataStruct->setPoint - inputData;

    pidDataStruct->pidP = pidDataStruct->kP * pidDataStruct->error;

    //if(pidDataStruct->error > -3 && pidDataStruct->error < 3)
    pidDataStruct->pidI += (pidDataStruct->kI * pidDataStruct->error);

    
    float elapsedTime = 1;

    pidDataStruct->pTime = pidDataStruct->cTime;
    pidDataStruct->cTime = esp_timer_get_time();

    eTime = pidDataStruct->cTime - pidDataStruct->pTime;
    eTime /= 1000000;
    //ESP_LOGI(BOILER_TASK_TAG, "eTime %f", eTime);

    pidDataStruct->pidD = pidDataStruct->kD * ((pidDataStruct->error - pidDataStruct->pError)/eTime);
    
    float output = pidDataStruct->pidP + pidDataStruct->pidI + pidDataStruct->pidD + pidDataStruct->offset;

    if(output > pidDataStruct->maxOut)
        output = pidDataStruct->maxOut;
    else if(output < pidDataStruct->minOut)
        output = pidDataStruct->minOut;

    pidDataStruct->pError = pidDataStruct->error;

    *outputData = output;
}



void initBoilerTask(){
    BaseType_t xReturned = xTaskCreatePinnedToCore(boilerTask, "boiler_task", BOILER_TASK_SIZE, (void *)NULL, BOILER_TASK_PRIORITY, &boilerTaskH, 1);

    if(xReturned == pdFAIL){

        ESP_LOGE(BOILER_TASK_TAG, "Failed to create task.");

        while(true){

        }
    }
}


static void boilerTask(void *pvParameters){

    float targetTemp = 70.0f;       //91.0f

    BoilerState boilerState = INIT_B;
    BoilerStructData boilerStructData = {true, false, true, 0.0f, 0.0f};
    PID_DataStruct myPID_Data = {true, 87.0f, 220.0f, 0.0f, 120.0f, 0.0f, 8191.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0};
    myPID_Data.setPoint = targetTemp - 3.0f;
    

    char *tempBuff = malloc(60);

    wait4BoilerTempValue(&targetTemp, &myPID_Data);

    wait4Control();

    //xTaskNotify(controlTaskH, 0x04, eSetBits);              //Notify control task that is ready
    myPID_Data.cTime = esp_timer_get_time();

    ESP_LOGI(BOILER_TASK_TAG, "ONLINE");


    while(1){
        checkNotifications4Boiler(&boilerState);

        check4BoilerTempUpdate(&targetTemp, &myPID_Data);

        readTemp(&boilerStructData);

        xQueueOverwrite(xQueueBoilerTemp, (void *) &boilerStructData.thermistorTemp);


        switch(boilerState){
            case IDLE_B:
            case HOT_B:
            case HOT_MAX_B:

            break;
            case IDLE_2_HOT_B:
                myPID_Data.kI = 0.8f;           //0.5

                myPID_Data.outputSum = 0;
                myPID_Data.lastInput = 0;
                myPID_Data.error = 0;
                myPID_Data.pError = 0;
                myPID_Data.pidP = 0;
                myPID_Data.pidI = 0;
                myPID_Data.pidD = 0;

                myPID_Data.offset = 6553.0f;        //4096 - 5734
                
                boilerState = HOT_B;
            break;
            case IDLE_2_HOT_MAX_B:
                myPID_Data.kI = 0.8f;

                myPID_Data.outputSum = 0;
                myPID_Data.lastInput = 0;
                myPID_Data.error = 0;
                myPID_Data.pError = 0;
                myPID_Data.pidP = 0;
                myPID_Data.pidI = 0;
                myPID_Data.pidD = 0;

                myPID_Data.offset = 8191.0f;
                boilerState = HOT_MAX_B;
            break;
            case HOT_2_IDLE_B:
                myPID_Data.kI = 0.0f;

                myPID_Data.outputSum = 0;
                myPID_Data.lastInput = 0;
                myPID_Data.error = 0;
                myPID_Data.pError = 0;
                myPID_Data.pidP = 0;
                myPID_Data.pidI = 0;
                myPID_Data.pidD = 0;

                myPID_Data.offset = 0.0f;

                boilerState = IDLE_B;
            break;
            default:                        //INIT_B
                if(boilerStructData.thermistorTemp >= targetTemp - 3.0f){
                    xTaskNotify(controlTaskH, 0x04, eSetBits);                              //Notify control task that boiler is ready
                    vTaskDelay(pdMS_TO_TICKS(100));

                    myPID_Data.setPoint = targetTemp;
                    boilerState = IDLE_B;

                    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0));  
                    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
            
                    vTaskDelay(pdMS_TO_TICKS(7000));
                }
            break;
        }
        

        boilerStructData.toSend = true;
        getMessage2Send2Nextion(tempBuff, &boilerStructData);
        uart_write_bytes(UART_PORT_N, tempBuff, strlen(tempBuff));

        boilerStructData.toSend = false;
        getMessage2Send2Nextion(tempBuff, &boilerStructData);
        uart_write_bytes(UART_PORT_N, tempBuff, strlen(tempBuff));

        if(checkDataFromNextionPID_Tunner(&myPID_Data))
            ESP_LOGE(BOILER_TASK_TAG, "PID DATA UPDATED!! ");
    

        if(myPID_Data.controllerState)
            if(boilerState != HOT_MAX_B)
                computePID(&myPID_Data, boilerStructData.thermistorTemp, &boilerStructData.boilerDuty);
            else
                boilerStructData.boilerDuty = 8191.0f;
        else 
            boilerStructData.boilerDuty = 0;


        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, (uint32_t)boilerStructData.boilerDuty));  //2048 - 8191
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

        
        /*
        ESP_LOGW(BOILER_TASK_TAG, "TEMP: %f - %f - %f - %f - %d", 
            boilerStructData.thermistorTemp, myPID_Data.setPoint,
            boilerStructData.boilerDuty, myPID_Data.error, boilerState);

        if(boilerState != HOT_MAX_B){
            ESP_LOGW(BOILER_TASK_TAG, "PID0: %d - %f - %f - %f", 
                myPID_Data.controllerState, myPID_Data.kP, myPID_Data.kI, myPID_Data.kD);

            ESP_LOGI(BOILER_TASK_TAG, "PID1: %f - %f - %f", 
                myPID_Data.pidP, myPID_Data.pidI, myPID_Data.pidD);      
        }
        */
            
        

        vTaskDelay(pdMS_TO_TICKS(200));
    }

}


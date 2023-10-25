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

static void innerBoilerControl(BoilerStructData *boilerStructData){
    if(boilerStructData->boilerIsOn){


        switch(boilerStructData->boilerStateMode){
            case B_OVERDRIVE_MODE:

            break;
            case B_NORMAL_MODE:

            break;
            case B_NORMAL_2_OVERDRIVE:
                boilerStructData->boilerStateMode = B_OVERDRIVE_MODE;
                //gpio_set_level(STATUS_LED_PIN, 1);
            break;
            case B_OVERDRIVE_2_NORMAL:
                boilerStructData->boilerStateMode = B_NORMAL_MODE;
                //gpio_set_level(STATUS_LED_PIN, 0);
            break;
        }
        
    }
    else{
        if(boilerStructData->boilerState){
            boilerStructData->boilerState = false;
            //gpio_set_level(STATUS_LED_PIN, 0);
        }
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


static void checkNotifications4Boiler(BoilerStructData *boilerStructData){
    uint32_t ulNotifiedValue = 0;

    if(xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, pdMS_TO_TICKS(15) == pdPASS)){
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
            ESP_LOGW(UI_TASK_TAG, "Set Boiler Normal Mode NOTIFICATION");
            boilerStructData->boilerStateMode = B_OVERDRIVE_2_NORMAL;
        }

        if((ulNotifiedValue & 0x010) >> 4){
            ESP_LOGW(UI_TASK_TAG, "Set Boiler Overdrive Mode NOTIFICATION");
            boilerStructData->boilerStateMode = B_NORMAL_2_OVERDRIVE;
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


bool checkDataFromNextionPID_Tunner(BoilerStructData *boilerStructData, pid_ctrl_config_t *pidConfig){
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

            for(size_t i = 0; i < buffSize; i++){

                if(tempData[i] == 'p'){
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
                        boilerStructData->kP = floatData;   
                    break;
                    case I_DATA:
                        boilerStructData->kI = floatData;
                    break;
                    case D_DATA:
                        boilerStructData->kD = floatData;
                    break;
                    default:                //S_DATA
                        boilerStructData->setPoint = intData;
                    break;
                }

                newData = true;
                free(tempStr);
            }
        }

        free(tempData);
    }

    return newData;

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

    BoilerStructData boilerStructData = {true, false, true, B_NORMAL_MODE, 0.0f, 0, 0, 0, 0, 0};
    char *tempBuff = malloc(60);

    wait4Control();

    //xTaskNotify(controlTaskH, 0x04, eSetBits);              //Notify control task that is ready
    ESP_LOGI(BOILER_TASK_TAG, "ONLINE");

    pid_ctrl_block_handle_t myPidHandle;
    pid_ctrl_config_t myPidConfig; 
    myPidConfig.init_param.cal_type = PID_CAL_TYPE_INCREMENTAL;
    myPidConfig.init_param.min_output = 0.0f;
    myPidConfig.init_param.max_output = 8191.0f;


    ESP_ERROR_CHECK(pid_new_control_block(&myPidConfig, &myPidHandle));


    while(1){
        checkNotifications4Boiler(&boilerStructData);

        readTemp(&boilerStructData);

        innerBoilerControl(&boilerStructData);

        xQueueOverwrite(xQueueBoilerTemp, (void *) &boilerStructData.thermistorTemp);

        boilerStructData.toSend = true;
        getMessage2Send2Nextion(tempBuff, &boilerStructData);
        uart_write_bytes(UART_PORT_N, tempBuff, strlen(tempBuff));

        boilerStructData.toSend = false;
        getMessage2Send2Nextion(tempBuff, &boilerStructData);
        uart_write_bytes(UART_PORT_N, tempBuff, strlen(tempBuff));

        if(checkDataFromNextionPID_Tunner(&boilerStructData, &myPidConfig))
            pid_update_parameters(myPidHandle, &myPidConfig);

        //pid_compute(myPidHandle, 
          //          (boilerStructData.setPoint - boilerStructData.thermistorTemp), 
            //        &boilerStructData.thermistorTemp);

        //ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 2048));  //2048 - 8191
        //ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

        ESP_LOGW(BOILER_TASK_TAG, "%f - %d - %f - %f - %f", 
                boilerStructData.thermistorTemp, boilerStructData.setPoint,
                boilerStructData.kP, boilerStructData.kI, boilerStructData.kD);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}


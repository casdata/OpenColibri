//
// Created by castdata on 8/14/23.
//
#include "boilerT.h"

TaskHandle_t boilerTaskH = NULL;


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


static void readTemp(float *thermTemp){
    uint16_t adcValue;

    if(readMCP3421_ADC(&adcValue)){

        float thermistorVoltage = adcValue * 0.001f;

        uint16_t thermistorValue = (thermistorVoltage * 12000)/(3.3f - thermistorVoltage);

        //ESP_LOGI(TEMP_TASK_TAG, "Raw temp: %d - %.4f - %d", adcValue, thermistorVoltage, thermistorValue); //1649

        float temp = log(thermistorValue);
        temp = (1.0f / (thermA + (thermB * temp + (thermC * temp * temp * temp))));
        temp -= 273.15f;

    }

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

    while(1){


        float thermistorTemp = 0;

        readTemp(&thermistorTemp);


        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}


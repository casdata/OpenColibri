#include "btT.h"

TaskHandle_t btTaskH = NULL;

void initBtTask(){
    BaseType_t xReturned = xTaskCreatePinnedToCore(btTask, "bt_task", BT_TASK_SIZE, (void *)NULL, BT_TASK_PRIORITY, &btTaskH, 1);

    if(xReturned == pdFAIL){

        ESP_LOGE(BT_TASK_TAG, "Failed to create task.");

        while(true){

        }
    }
}


static void btTask(void *pvParameters){


    while(1){
        vTaskDelay(1000);
    }

}
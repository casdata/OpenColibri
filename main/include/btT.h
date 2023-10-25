#ifndef OPENCOLIBRI_BTT_H
#define OPENCOLIBRI_BTT_H

#include "extend.h"
#include "controlT.h"

typedef struct {
    uint32_t handle;
    uint16_t len;
    uint8_t in_data[ESP_SPP_MAX_MTU];
} spp_queue_item_t;

typedef struct _tbd{
    uint32_t handle;
    uint8_t bd_client_name[CLIENT_NAME_MAX+1];
    uint8_t len;
    struct _tbd *next;
} bd_client_t;

extern TaskHandle_t btTaskH;


void initBtTask();
static void btTask(void *pvParameters);


#endif
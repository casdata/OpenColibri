//
// Created by castdata on 8/14/23.
//
#include "../include/uiT.h"

TaskHandle_t uiTaskH = NULL;
uint8_t lcdMemPos = 0;

static int getBit2Word(const uint8_t *word, int8_t position){
    int bitValue = *word >> position;

    bitValue &= 1;

    return bitValue;
}

static void setBit2Word(uint16_t *word, int bitValue, int8_t position){

    uint16_t mask = 1 << position;
    mask = ~mask;
    *word &= mask;
    *word += (bitValue << position);
}

static void sendData2LCD(const uint8_t dataByte, const bool rW, const bool rs){

    int enableLatch = 0;

    //ESP_LOGI("DEBUG", "-> %d", dataByte);

    for(size_t j = 0; j < 2; j++){
        gpio_set_level(SER_DISPLAY_PIN, !(uint32_t)rs);
        gpio_set_level(CLK_UI_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_set_level(CLK_UI_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(1));

        gpio_set_level(SER_DISPLAY_PIN, !(uint32_t)rW);
        gpio_set_level(CLK_UI_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_set_level(CLK_UI_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(1));

        gpio_set_level(SER_DISPLAY_PIN, enableLatch);              //Enable (latch h)
        gpio_set_level(CLK_UI_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_set_level(CLK_UI_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(1));

        for(int i = 7; i >= 0; i--){

            //ESP_LOGI("DEBUG", "%d: %d - %d",j, i, getBit2Word(&dataByte, i));

            gpio_set_level(SER_DISPLAY_PIN, !getBit2Word(&dataByte, i));
            gpio_set_level(CLK_UI_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(1));
            gpio_set_level(CLK_UI_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(1));


        }

        //ESP_LOGI("DEBUG", " ");

        gpio_set_level(RCK_DISPLAY_PIN, 1);                         //Register transferred to output latches
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_set_level(RCK_DISPLAY_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(1));


        enableLatch = 1;



        //vTaskDelay(pdMS_TO_TICKS(5000));

    }
}

static void fullClearLcdScreen(){
    clearLcdScreen();
    vTaskDelay(pdMS_TO_TICKS(30));

    setLcdCursor2Home();
    vTaskDelay(pdMS_TO_TICKS(30));
}

static void clearLcdScreen(){
    sendData2LCD(0b00000001, 0, 0);
}

static void setLcdCursor2Home(){
    sendData2LCD(0b00000010, 0, 0);
    lcdMemPos = 0;
}

/*  setLcdEntryMode
 * incrementMode:   Increment / decrement of DDRAM address
 *                  true : shift display to left
 *                  fales: shift display to right
 * shift:           Enables the shift function of the display
 */

static void setLcdEntryMode(const bool incrementMode, const bool shift){
    uint8_t dataByte = 0b00000100;

    uint8_t dataBit = (uint8_t)incrementMode;
    dataBit <<= 1;
    dataByte += dataBit;

    dataBit = (uint8_t)shift;
    dataByte += dataBit;

    sendData2LCD(dataByte, 0, 0);

}

/*  setLcdOptions
 * displayOn:   true = display on
 *              false = dispaly off
 * cursorOn:    true = cursor on
 *              fales = cursor off
 * cursorBlink: true = on
 *              false = off
 */

static void setLcdOptions(const bool displayOn, const bool cursorOn, const bool cursorBlink) {
    uint8_t dataByte = 0b00001000;

    uint8_t dataBit = (uint8_t)displayOn;
    dataBit <<= 2;
    dataByte += dataBit;

    dataBit = (uint8_t)cursorOn;
    dataBit <<= 1;
    dataByte += dataBit;

    dataBit = (uint8_t)cursorBlink;
    dataByte += dataBit;

    sendData2LCD(dataByte, 0, 0);
}

/*  setLcdCursorOrShift
 *  displayShift (S/C):     true = display shift
 *                          false = cursor
 *  rightShift (R/L):       true = right shift
 *                          false = left shift
 *
 *  S/C     |   R/L   |
 *  false       false   Shift cursor to left
 *  false       true    Shift cursor to the right
 *  true        false   Shift display to the left. Cursor follows the display shift
 *  true        true    Shift display to the right. Cursor follows the display shift
 */

static void setLcdCursorOrShift(const bool displayShift, const bool rightShift){
    uint8_t dataByte = 0b00010000;

    uint8_t dataBit = (uint8_t)displayShift;
    dataBit <<= 3;
    dataByte += dataBit;

    dataBit = (uint8_t)rightShift;
    dataBit <<= 2;
    dataByte += dataBit;

    sendData2LCD(dataByte, 0, 0);

}

/*  setLcdFunction
 *  bitDL:      Interface data lenght control bit
 *              true = 8-bit bus mode
 *              false = 4-bit bus mode
 *  bitN:       Display line number control bit
 *              true = 2-line display mode
 *              false = 1-line display mode
 *  bitF:       Display font type control bit
 *              true = 5x11 dots format
 *              false = 5x8 dots format
 */

static void setLcdFunction(const bool bitDL, const bool bitN, const bool bitF){
    uint8_t dataByte = 0b00100000;

    uint8_t dataBit = (uint8_t)bitDL;
    dataBit <<= 4;
    dataByte += dataBit;

    dataBit = (uint8_t)bitN;
    dataBit <<= 3;
    dataByte += dataBit;

    dataBit = (uint8_t)bitF;
    dataBit <<= 2;
    dataByte += dataBit;

    sendData2LCD(dataByte, 0, 0);
}

/*  setLcdCGRAM_addr
 *  addr:   CGRAM address to AC
 *          CGRAM adderss is from 0x0 to 0x7
 */

static void setLcdCGRAM_addr(uint8_t addr){
    uint8_t dataByte = 0b01000000;

    addr &= 0x3f;
    dataByte += addr;

    sendData2LCD(dataByte, 0, 0);
}

/*  setLcdDDRAM_addr
 *  addr:   DDRAM address to AC
 *          DDRAM address is from 0x0 to 0x27
 */
static void setLcdDDRAM_addr(uint8_t addr){
    uint8_t dataByte = 0b10000000;

    addr &= 0x7f;
    dataByte += addr;

    sendData2LCD(dataByte, 0, 0);
}


static void write2LCD(const char *dataStr, const uint8_t dataStrLength, const uint8_t pos){

    setLcdPos(pos);

    for(size_t i = 0; i < dataStrLength; i++){

        sendData2LCD(*(dataStr + i), 0, 1);
        vTaskDelay(pdMS_TO_TICKS(5));

        if(lcdMemPos++ == 7)
            setLcdPos(8);

    }

}


static void setLcdPos(uint8_t x){

    if(x > 7)
        x += 56;

    lcdMemPos = x;

    setLcdDDRAM_addr(x);
}


static void getBoilerTemp(float *temperature){
    xQueuePeek(xQueueBoilerTemp, temperature, pdMS_TO_TICKS(60));
}


bool updateUI(InputBtnStruct *uiBtns){
    bool changed = false;

    uint16_t uiSwitches = 0; 
    static uint16_t preUiSwitches = 0;
    
    //Clear registers in the 74HC165 and 74HC595
    gpio_set_level(SH_LD_UI_PIN, 1);            //LOW

    vTaskDelay(pdMS_TO_TICKS(1));

    gpio_set_level(SH_LD_UI_PIN, 0);            //HIGH

    vTaskDelay(pdMS_TO_TICKS(1));


    //Shift 0 to the display shift register.
    gpio_set_level(SER_DISPLAY_PIN, 1);         //LOW
    


    setBit2Word(&uiSwitches, !gpio_get_level(IN_SERIAL_UI_PIN), 0);


    for(size_t i = 1; i < 8; i++){
        gpio_set_level(CLK_UI_PIN, 0);

        vTaskDelay(pdMS_TO_TICKS(1));

        gpio_set_level(CLK_UI_PIN, 1);

        setBit2Word(&uiSwitches, !gpio_get_level(IN_SERIAL_UI_PIN), i);

        vTaskDelay(pdMS_TO_TICKS(1));

    }

    for(size_t i = 0; i < 9; i++){
        gpio_set_level(CLK_UI_PIN, 0);

        vTaskDelay(pdMS_TO_TICKS(1));

        gpio_set_level(CLK_UI_PIN, 1);

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    for(size_t i = 9; i < 11; i++){
        gpio_set_level(CLK_UI_PIN, 0);

        vTaskDelay(pdMS_TO_TICKS(1));

        gpio_set_level(CLK_UI_PIN, 1);

        setBit2Word(&uiSwitches, !gpio_get_level(IN_SERIAL_UI_PIN), i);

        vTaskDelay(pdMS_TO_TICKS(1)); 
    }


    if(uiSwitches != preUiSwitches){
        preUiSwitches = uiSwitches;
        changed = true;

        if(uiSwitches & 1)
            uiBtns->btn0 = true;
        else
            uiBtns->btn0 = false;

        if((uiSwitches >> 1) & 1)
            uiBtns->btn1 = true;
        else
            uiBtns->btn1 = false;

        if((uiSwitches >> 2) & 1)
            uiBtns->btn2 = true;
        else
            uiBtns->btn2 = false;

        if((uiSwitches >> 3) & 1)
            uiBtns->btn3 = true;
        else
            uiBtns->btn3 = false;

        if((uiSwitches >> 4) & 1)
            uiBtns->btn4 = true;
        else
            uiBtns->btn4 = false;
        
        if((uiSwitches >> 5) & 1)
            uiBtns->btn5 = true;
        else
            uiBtns->btn5 = false;

        if((uiSwitches >> 6) & 1)
            uiBtns->btn6 = true;
        else
            uiBtns->btn6 = false;

        if((uiSwitches >> 7) & 1)
            uiBtns->btn7 = true;
        else
            uiBtns->btn7 = false;

        if((uiSwitches >> 9) & 1)
            uiBtns->btnA = true;
        else
            uiBtns->btnA = false;
        
        if((uiSwitches >> 10) & 1)
            uiBtns->btnB = true;
        else
            uiBtns->btnB = false;

        /*
        ESP_LOGE(UI_TASK_TAG, "->%d = %d %d %d %d %d %d %d %d %d %d", uiSwitches,
                uiBtns->btn0, uiBtns->btn1, uiBtns->btn2, uiBtns->btn3, uiBtns->btn4, uiBtns->btn5,
                uiBtns->btn6, uiBtns->btn7, uiBtns->btnA, uiBtns->btnB);
        */

    }                                                  

    return changed;
                                                            
}

static void checkNotifications4Ui(UiState *previousUiState, UiState *currentUiState, ErrorCode *errorCode){
    uint32_t ulNotifiedValue = 0;

    if(xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, pdMS_TO_TICKS(15) == pdPASS)){

        if(ulNotifiedValue & 0x001){
            ESP_LOGW(UI_TASK_TAG, "Clear Error Notification");
            
            
            if(*previousUiState == BOOTING_UI){
                *previousUiState = ERROR_UI;
                *currentUiState = BOOTING_UI; 

                fullClearLcdScreen();
            }
        

            *errorCode = NONE;
        }

        if((ulNotifiedValue & 0x002) >> 1){
            ESP_LOGE(UI_TASK_TAG, "AIR BREAK ERROR NOTIFICATION");

            *previousUiState = *currentUiState;
            *currentUiState = ERROR_UI;

            fullClearLcdScreen();

            *errorCode = NO_WATER;
        }

        if((ulNotifiedValue & 0x004) >> 2){
            ESP_LOGE(UI_TASK_TAG, "COFFEE ERROR NOTIFICATION");

            *previousUiState = *currentUiState;
            *currentUiState = ERROR_UI;

            fullClearLcdScreen();

            *errorCode = NO_COFFEE;
        }

        if((ulNotifiedValue & 0x008) >> 3){
            ESP_LOGE(UI_TASK_TAG, "BREWER ERROR NOTIFICATION");

            *previousUiState = *currentUiState;
            *currentUiState = ERROR_UI;

            fullClearLcdScreen();

            *errorCode = BREWER_ISSUE;
        }

        if((ulNotifiedValue & 0x010) >> 4){
            ESP_LOGW(UI_TASK_TAG, "idleState NOTIFICATION");

            *previousUiState = *currentUiState;
            *currentUiState = IDLE_UI;
        }

        if((ulNotifiedValue & 0x020) >> 5){
            ESP_LOGW(UI_TASK_TAG, "bootingState NOTIFICATION");

            *previousUiState = *currentUiState;
            *currentUiState = BOOTING_UI;
        }

        if((ulNotifiedValue & 0x040) >> 6){
            ESP_LOGW(UI_TASK_TAG, "maintenanceState NOTIFICATION");

            *previousUiState = *currentUiState;
            *currentUiState = MAINTENANCE_UI;
        }

        if((ulNotifiedValue & 0x080) >> 7){
            ESP_LOGW(UI_TASK_TAG, "cleanState NOTIFICATION");

            *previousUiState = *currentUiState;
            *currentUiState = CLEAN_UI;
        }

        if((ulNotifiedValue & 0x100) >> 8){
            ESP_LOGW(UI_TASK_TAG, "prepareDrinkState NOTIFICATION");

            *previousUiState = *currentUiState;
            *currentUiState = PREPARE_DRINK_UI;

        }
        

    }
}


static void checkQueue4Ui(UiData *myUiData, UiUpdate *myUiUpdate){
    UiData tempUiData;


    if(xQueueReceive(xQueueUI, &tempUiData, pdMS_TO_TICKS(10)) == pdPASS){

        if(myUiData->page != tempUiData.page){
            myUiData->page = tempUiData.page;
            myUiUpdate->updatePage = true;
        }


        if(strcmp(myUiData->strData, tempUiData.strData) != 0){
            strcpy(myUiData->strData, tempUiData.strData);
            myUiUpdate->updateDataStr = true;
        }

    }

}


static void inBootingCodeState(UiData *myUiData){

    float newBoilerTemp = 0;

    getBoilerTemp(&newBoilerTemp);

    if(newBoilerTemp != myUiData->boilerTempUi){

        myUiData->boilerTempUi = newBoilerTemp;

        char *strTemperature = (char *) malloc(16);
        
        memset(strTemperature, 0, 16);

        sprintf(strTemperature, "%.2f", newBoilerTemp);
    

        write2LCD(strTemperature, strlen(strTemperature), 8);

        sendData2LCD(0b11011111, 0, 1);    
        lcdMemPos++; 
        sendData2LCD(0b01000011, 0, 1);   
        lcdMemPos++;

        free(strTemperature);
    }

}

static void showPreparingDrinkName(UiData *myUiData){

}




static void inErrorCodeState(const ErrorCode errorCode){

    static int blinkCount = 0;
    static bool showMessage = true;

    bool updateLcd = false;

    blinkCount++;

    if(showMessage){
        if(blinkCount > 4){
            updateLcd = true;
            showMessage = false;
        }
    }
    else{
        if(blinkCount > 1){
            updateLcd = true;
            showMessage = true;
        }
    }

    if(updateLcd){
        blinkCount = 0;

        fullClearLcdScreen();
    }

    if(showMessage){
        switch(errorCode){
            case NO_WATER:
                write2LCD("  E01: NO WATER ", 15, 0);
            break;
            case NO_COFFEE:
                write2LCD(" E02: NO COFFEE", 15, 0);
            break;
            case BREWER_ISSUE:
                write2LCD("  E03: BREWER", 13, 0);
            break;
            case NONE:
                write2LCD("----------------", 16, 0);
            break;
        }
    }

}


static void initLcd(){
    setLcdFunction(true, true, false);

    vTaskDelay(pdMS_TO_TICKS(30));

    setLcdOptions(true, false, false);

    vTaskDelay(pdMS_TO_TICKS(30));

    setLcdEntryMode(true, false);

    vTaskDelay(pdMS_TO_TICKS(30));

    clearLcdScreen();

    vTaskDelay(pdMS_TO_TICKS(30));
}


void initUiTask(){
    BaseType_t xReturned = xTaskCreate(uiTask, "ui_task", UI_TASK_SIZE, (void *)NULL, UI_TASK_PRIORITY, &uiTaskH);

    if(xReturned == pdFAIL){

        ESP_LOGE(UI_TASK_TAG, "Failed to create task.");

        while(true){

        }
    }


}

static void uiTask(void *pvParameters){

    uint16_t uiSwitches = 0;

    UiUpdate uiUpdate = {false, false, false};

    UiData uiData = {PAGE_1, 0.0f, NULL};
    uiData.strData = (char *)malloc(17);
    memset(uiData.strData, 0, 17);

    InputBtnStruct inputBtnStruct;

    UiState currentUiState = IDLE_UI;
    UiState previousUiState = IDLE_UI;

    ErrorCode errorCode = NONE;

    //double refTimeUi, cTimeUi;

    initLcd();
    write2LCD("OpenColibri V003", 16, 0);
    vTaskDelay(pdMS_TO_TICKS(3000));

    //xTaskNotify(controlTaskH, 0x01, eSetBits);              //Notify control task that is ready
    ESP_LOGI(UI_TASK_TAG, "ONLINE");

    while(true){

        checkNotifications4Ui(&previousUiState, &currentUiState, &errorCode);

        checkQueue4Ui(&uiData, &uiUpdate);

        if(updateUI(&inputBtnStruct))
            xQueueSend(xQueueInputBtns, (void *) &inputBtnStruct, portMAX_DELAY);

        switch(currentUiState){
            case IDLE_UI:
                if(previousUiState != IDLE_UI){
                    previousUiState = currentUiState;

                    fullClearLcdScreen(); 
                    write2LCD("?", 1, 8);      
                }

                if(uiUpdate.updatePage){
                    uiUpdate.updatePage = false;

                    fullClearLcdScreen();

                    if(uiData.page == PAGE_1)
                        write2LCD("1", 1, 8);
                    else
                        write2LCD("2", 1, 8);
                }

            break;
            case BOOTING_UI:
                if(previousUiState != BOOTING_UI){
                    previousUiState = currentUiState;

                    fullClearLcdScreen();
                    write2LCD("Boiler: ", 8, 0);
                }

                inBootingCodeState(&uiData);
            break;
            case MAINTENANCE_UI:
                if(previousUiState != MAINTENANCE_UI){
                    previousUiState = currentUiState;

                    fullClearLcdScreen();
                    write2LCD("Maintanance", 11, 1);
                }
            break;
            case CLEAN_UI:
                if(previousUiState != CLEAN_UI){
                    previousUiState = currentUiState;

                    fullClearLcdScreen();
                    write2LCD("CLEANING....", 12, 1);
                }
            break;
            case PREPARE_DRINK_UI:
                if(previousUiState != PREPARE_DRINK_UI && uiUpdate.updateDataStr){
                    previousUiState = currentUiState;
                    uiUpdate.updateDataStr = false;

                    fullClearLcdScreen();
                    write2LCD(uiData.strData, strlen(uiData.strData), 0);

                    /*
                    if(uiUpdate.updateDataStr){
                        uiUpdate.updateDataStr = false;

                        fullClearLcdScreen();
                        write2LCD(uiData.strData, strlen(uiData.strData), 0);

                        refTimeUi = esp_timer_get_time();
                    }
                    else{
                        cTime = esp_timer_get_time() - refTime;
            
                        if( cTime >= 15000000){      //15 seg
                            previousUiState = currentUiState;

                            fullClearLcdScreen();
                            write2LCD("Preparing drink", 15, 0);

                        }
                    }
                    */
                }
            break;
            case ERROR_UI:
                inErrorCodeState(errorCode);
            break;
        }
       
        vTaskDelay(pdMS_TO_TICKS(1000));//500
    }


}
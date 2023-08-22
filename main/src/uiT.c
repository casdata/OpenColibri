//
// Created by castdata on 8/14/23.
//
#include "../include/uiT.h"

TaskHandle_t uiTaskH = NULL;

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

static void clearLcdScreen(){
    sendData2LCD(0b00000001, 0, 0);
}

static void setLcdCursor2Home(){
    sendData2LCD(0b00000010, 0, 0);
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


static void write2LCD(const char *dataStr, const uint8_t dataStrLength){

    for(size_t i = 0; i < dataStrLength; i++){

        sendData2LCD(*(dataStr + i), 0, 1);

        vTaskDelay(pdMS_TO_TICKS(5));
    }

    //sendData2LCD(0b01010111, 0, 1);             //Write 'W'

}


static void setLcdPos(const uint8_t x){
    setLcdDDRAM_addr(x);
}


void updateUI(uint16_t *uiSwitches){

    
    //Clear registers in the 74HC165 and 74HC595
    gpio_set_level(SH_LD_UI_PIN, 1);            //LOW

    vTaskDelay(pdMS_TO_TICKS(1));

    gpio_set_level(SH_LD_UI_PIN, 0);            //HIGH

    vTaskDelay(pdMS_TO_TICKS(1));


    //Shift 0 to the display shift register.
    gpio_set_level(SER_DISPLAY_PIN, 1);         //LOW
    


    setBit2Word(uiSwitches, !gpio_get_level(IN_SERIAL_UI_PIN), 0);


    for(size_t i = 1; i < 8; i++){
        gpio_set_level(CLK_UI_PIN, 0);

        vTaskDelay(pdMS_TO_TICKS(1));

        gpio_set_level(CLK_UI_PIN, 1);

        setBit2Word(uiSwitches, !gpio_get_level(IN_SERIAL_UI_PIN), i);

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

        setBit2Word(uiSwitches, !gpio_get_level(IN_SERIAL_UI_PIN), i);

        vTaskDelay(pdMS_TO_TICKS(1)); 
    }


    ESP_LOGI(UI_TASK_TAG, "%d", *uiSwitches);                                                     
                                                            
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

    setLcdFunction(true, false, false);

    vTaskDelay(pdMS_TO_TICKS(200));

    setLcdOptions(true, false, false);

    vTaskDelay(pdMS_TO_TICKS(200));

    setLcdEntryMode(true, false);

    vTaskDelay(pdMS_TO_TICKS(200));

    clearLcdScreen();

    vTaskDelay(pdMS_TO_TICKS(200));

    write2LCD("Open Colibri V0.00 2023", 23);

    for(size_t i = 0; i < 7; i++) {

        vTaskDelay(pdMS_TO_TICKS(1000));

        setLcdCursorOrShift(true, false);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
    setLcdCursor2Home();


    while(true){

        updateUI(&uiSwitches);
       
        vTaskDelay(pdMS_TO_TICKS(1000));
    }


}
#include "extend.h"
#include "controlT.h"
#include "uiT.h"
#include "boilerT.h"
#include "interruptsT.h"


QueueHandle_t xQueueIntB = NULL;
QueueHandle_t xQueueInputsSw = NULL;
QueueHandle_t xQueueInputBtns = NULL;
QueueHandle_t xQueueInputPulse = NULL;
QueueHandle_t xQueueInputTimePerPulse = NULL;
QueueHandle_t xQueueBoilerTemp = NULL;
QueueHandle_t xQueueUI = NULL;
QueueHandle_t xQueueData2Boiler = NULL;



static void IRAM_ATTR interrupt_handler(void *args){
    int16_t pinNumber = (int16_t)args;
    xQueueSendFromISR(xQueueIntB, &pinNumber, NULL);
    //ESP_LOGI("INTR", "Interrupt intb %d", pinNumber);
}


static void initQueues(){
    xQueueIntB = xQueueCreate(10, sizeof(int16_t));
    xQueueInputsSw = xQueueCreate(1, sizeof(InputSwStruct));
    xQueueInputBtns = xQueueCreate(1, sizeof(InputBtnStruct));
    xQueueInputPulse = xQueueCreate(1, sizeof(uint16_t));
    xQueueInputTimePerPulse = xQueueCreate(1, sizeof(double));
    xQueueBoilerTemp = xQueueCreate(1, sizeof(float));
    xQueueUI = xQueueCreate(5, sizeof(UiData));
    xQueueData2Boiler = xQueueCreate(5, sizeof(uint8_t));

    if(xQueueIntB == NULL)
        ESP_LOGE(MAIN_TASK_TAG, "xQueueIntB initialization error!");
    
    if(xQueueInputsSw == NULL)
        ESP_LOGE(MAIN_TASK_TAG, "xQueueInputsSw initialization error!");

    if(xQueueInputBtns == NULL)
        ESP_LOGE(MAIN_TASK_TAG, "xQueueInputBtns initialization error!");

    if(xQueueInputPulse == NULL)
        ESP_LOGE(MAIN_TASK_TAG, "xQueueInputPulse initialization error!");

    if(xQueueInputTimePerPulse == NULL)
        ESP_LOGE(MAIN_TASK_TAG, "xQueueInputTimePerPulse initialization error!");

    if(xQueueUI == NULL)
        ESP_LOGE(MAIN_TASK_TAG, "xQueueUI initialization error!");

    if(xQueueData2Boiler == NULL)
        ESP_LOGE(MAIN_TASK_TAG, "xQueueData2Boiler initialization error!");

    if(xQueueBoilerTemp == NULL)
        ESP_LOGE(MAIN_TASK_TAG, "xQueueBoilerTemp initialization error!");



}

static void initGPIO(){
    gpio_reset_pin(CLK_UI_PIN);
    gpio_reset_pin(SH_LD_UI_PIN);
    gpio_reset_pin(IN_SERIAL_UI_PIN);
    gpio_reset_pin(RCK_DISPLAY_PIN);
    gpio_reset_pin(SER_DISPLAY_PIN);
    gpio_reset_pin(MCP23017_IN_RS_PIN);
    gpio_reset_pin(MCP23017_OUT_RS_PIN);
    gpio_reset_pin(MCP23017_INTB_PIN);
    gpio_reset_pin(VOLUMETRIC_PIN);
    gpio_reset_pin(BOILER_PIN);
    gpio_reset_pin(STATUS_LED_PIN);


    gpio_set_direction(CLK_UI_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(SH_LD_UI_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(RCK_DISPLAY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(SER_DISPLAY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(MCP23017_IN_RS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(MCP23017_OUT_RS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(BOILER_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(STATUS_LED_PIN, GPIO_MODE_OUTPUT);

    gpio_set_direction(MCP23017_INTB_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(IN_SERIAL_UI_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(VOLUMETRIC_PIN, GPIO_MODE_INPUT);
    
    gpio_set_intr_type(MCP23017_INTB_PIN, GPIO_INTR_POSEDGE);
    gpio_set_intr_type(VOLUMETRIC_PIN, GPIO_INTR_POSEDGE);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(MCP23017_INTB_PIN, interrupt_handler, (void *)MCP23017_INTB_PIN);
    gpio_isr_handler_add(VOLUMETRIC_PIN, interrupt_handler, (void *)VOLUMETRIC_PIN);


    gpio_set_level(CLK_UI_PIN, 1);
    gpio_set_level(SH_LD_UI_PIN, 0);
    gpio_set_level(RCK_DISPLAY_PIN, 1);
    gpio_set_level(SER_DISPLAY_PIN, 1);
    gpio_set_level(MCP23017_IN_RS_PIN, 1);
    gpio_set_level(MCP23017_OUT_RS_PIN, 1);
    //gpio_set_level(BOILER_PIN, 0);
    gpio_set_level(STATUS_LED_PIN, 0);

    if(!NEXTION_LCD){
        gpio_reset_pin(POWDER_C_PIN);

        gpio_set_direction(POWDER_C_PIN, GPIO_MODE_OUTPUT);

        gpio_set_level(POWDER_C_PIN, 0);
    }
}

static void initI2C(){

    int i2c_port = I2C_PORT_NUM;

    i2c_config_t i2cConfig = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ
    };

    i2c_param_config(i2c_port, &i2cConfig);

    ESP_ERROR_CHECK(i2c_driver_install(i2c_port, i2cConfig.mode, I2C_RX_BUFF_DISABLE, I2C_TX_BUFF_DISABLE, 0));

}

static void initLEDC(){

    ledc_timer_config_t ledcConfig = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_13_BIT,
        .freq_hz          = 1000,
        .clk_cfg          = LEDC_AUTO_CLK
    };

    ESP_ERROR_CHECK(ledc_timer_config(&ledcConfig));

    ledc_channel_config_t ledcChannel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = BOILER_PIN,//STATUS_LED_PIN,       //BOILER_PIN
        .duty           = 0,
        .hpoint         = 0
    };

    ESP_ERROR_CHECK(ledc_channel_config(&ledcChannel));

}

static void initNextion(){
    const uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_N, BUFF_SIZE * 2, BUFF_SIZE * 2, 10, &xQueueUart, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_N, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_N, TXD_2_PIN, RXD_2_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_enable_pattern_det_baud_intr(UART_PORT_N, NEXTION_D_CHAR, 1, 9, 0, 0));
    ESP_ERROR_CHECK(uart_pattern_queue_reset(UART_PORT_N, 10));
}

static void initPID(){
    
}

static void initNVS(){
    esp_err_t err = nvs_flash_init();

    if(err != ESP_OK){
        ESP_LOGI(MAIN_TASK_TAG, "nvs flash init error");
        return;
    }
}


void app_main(void)
{

    initQueues();

    initGPIO();
    initI2C();
    initLEDC();

    if(NEXTION_LCD)
        initNextion();

    initPID();
    initNVS();

    initI2C_MCP23017_Out();
    initI2C_MCP23017_In();

    initControlTask();
    initUiTask();
    initBoilerTask();
    initInterruptsTask();


}

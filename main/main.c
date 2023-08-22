#include "extend.h"
#include "controlT.h"
#include "uiT.h"
#include "boilerT.h"
#include "interruptsT.h"


QueueHandle_t xQueueIntB = NULL;
QueueHandle_t xQueueInputsSw = NULL;
QueueHandle_t xQueueInputPulse = NULL;


static void IRAM_ATTR interrupt_handler(void *args){
    int16_t pinNumber = (int16_t)args;
    xQueueSendFromISR(xQueueIntB, &pinNumber, NULL);
    //ESP_LOGI("INTR", "Interrupt intb %d", pinNumber);
}


static void initQueues(){
    xQueueIntB = xQueueCreate(10, sizeof(int16_t));
    xQueueInputsSw = xQueueCreate(1, sizeof(InputSwStruct));
    xQueueInputPulse = xQueueCreate(1, sizeof(uint16_t));

    if(xQueueIntB == NULL)
        ESP_LOGE(MAIN_TASK_TAG, "xQueueIntB initialization error!");
    
    if(xQueueInputsSw == NULL)
        ESP_LOGE(MAIN_TASK_TAG, "xQueueInputsSw initialization error!");

    if(xQueueInputPulse == NULL)
        ESP_LOGE(MAIN_TASK_TAG, "xQueueInputPulse initialization error!");

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
    gpio_set_level(BOILER_PIN, 0);
    gpio_set_level(STATUS_LED_PIN, 0);
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


void app_main(void)
{

    initQueues();

    initGPIO();
    initI2C();

    initI2C_MCP23017_Out();
    initI2C_MCP23017_In();

    initControlTask();
    initUiTask();
    initBoilerTask();
    initInterruptsTask();

}

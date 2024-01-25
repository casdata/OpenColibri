#include "extend.h"
#include "controlT.h"
#include "uiT.h"
#include "boilerT.h"
#include "interruptsT.h"
#include "btT.h"


QueueHandle_t xQueueIntB = NULL;
QueueHandle_t xQueueInputsSw = NULL;
QueueHandle_t xQueueInputBtns = NULL;
QueueHandle_t xQueueInputPulse = NULL;
QueueHandle_t xQueueInputTimePerPulse = NULL;
QueueHandle_t xQueueBoilerTemp = NULL;
QueueHandle_t xQueueData2Boiler = NULL;
QueueHandle_t xQueueSpp = NULL;


bool pannel_connected=false;
uint32_t pannel_handle = 0;

char rmt_bd_name[ESP_BT_GAP_MAX_BDNAME_LEN+1];
bd_client_t *bd_link_header=NULL;

spp_queue_item_t spp_queue_item;

void my_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    
    switch(event) {
        case ESP_BT_GAP_DISC_RES_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_BT_GAP_DISC_RES_EVT");
        break;
        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_BT_GAP_DISC_STATE_CHANGED_EVT");
        break;
        case ESP_BT_GAP_RMT_SRVCS_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_BT_GAP_RMT_SRVCS_EVT");
        break; 
        case ESP_BT_GAP_RMT_SRVC_REC_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_BT_GAP_RMT_SRVC_REC_EVT");
        break;
        case ESP_BT_GAP_AUTH_CMPL_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_BT_GAP_AUTH_CMPL_EVT");
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(MAIN_TASK_TAG, "bda:%x%x%x%x%x%x, device_name:%s--", 
                param->auth_cmpl.bda[0],param->auth_cmpl.bda[1],param->auth_cmpl.bda[2],param->auth_cmpl.bda[3],param->auth_cmpl.bda[4],param->auth_cmpl.bda[5],
                (char*)param->auth_cmpl.device_name);
            }
        break;
        case ESP_BT_GAP_PIN_REQ_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_BT_GAP_PIN_REQ_EVT");
        break;
        case ESP_BT_GAP_CFM_REQ_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_BT_GAP_CFM_REQ_EVT");
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;
        case ESP_BT_GAP_KEY_NOTIF_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT");
        break;
        case ESP_BT_GAP_KEY_REQ_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_BT_GAP_KEY_REQ_EVT");
        break;
        case ESP_BT_GAP_READ_RSSI_DELTA_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_BT_GAP_READ_RSSI_DELTA_EVT");
        break;
        case ESP_BT_GAP_CONFIG_EIR_DATA_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_BT_GAP_CONFIG_EIR_DATA_EVT");
        break;
        case ESP_BT_GAP_SET_AFH_CHANNELS_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "");
        break;
        case ESP_BT_GAP_READ_REMOTE_NAME_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_BT_GAP_READ_REMOTE_NAME_EVT");
        break;
        case ESP_BT_GAP_MODE_CHG_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_BT_GAP_MODE_CHG_EVT");
        break;
        case ESP_BT_GAP_REMOVE_BOND_DEV_COMPLETE_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_BT_GAP_REMOVE_BOND_DEV_COMPLETE_EVT");
        break;
        case ESP_BT_GAP_QOS_CMPL_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_BT_GAP_QOS_CMPL_EVT");
        break;
        default:
        break;
    }
    
}

char msg[100];
char temp_name[30];
void my_bt_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    
    switch(event) {
        case ESP_SPP_INIT_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_SPP_INIT_EVT");
        break;
        case ESP_SPP_UNINIT_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_SPP_UNINIT_EVT");
        break;
        case ESP_SPP_DISCOVERY_COMP_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_SPP_DISCOVERY_COMP_EVT");
        break;
        case ESP_SPP_OPEN_EVT:
            ESP_LOGI(MAIN_TASK_TAG,"ESP_SPP_OPEN_EVT");
        break;
        case ESP_SPP_CLOSE_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "close handle:%li, status:%d", param->close.handle, param->close.status);
            spp_queue_item.handle = param->close.handle;
            spp_queue_item.len = 6;
            memcpy(spp_queue_item.in_data, (uint8_t*)"AT+DIS", 6);
            xQueueSend(xQueueSpp, &spp_queue_item, portMAX_DELAY);
        break;
        case ESP_SPP_START_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "SPP_START_EVENT:status:%d, scn:%d", param->start.status, param->start.scn);
        break;
        case ESP_SPP_CL_INIT_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_SPP_CL_INIT_EVT");
        break;
        case ESP_SPP_DATA_IND_EVT:
            spp_queue_item.handle = param->data_ind.handle;
            spp_queue_item.len = param->data_ind.len;
            memcpy(spp_queue_item.in_data, param->data_ind.data, param->data_ind.len);
            if (spp_queue_item.in_data[spp_queue_item.len-2] == (uint8_t)'\r' && spp_queue_item.in_data[spp_queue_item.len-1] == (uint8_t)'\n') {
                spp_queue_item.len -= 2;
            }
            xQueueSend(xQueueSpp, &spp_queue_item, portMAX_DELAY);
        break;
        case ESP_SPP_CONG_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "cong handle:%li, cong:%d, status:%d", param->cong.handle, param->cong.cong, param->cong.status);
        break;
        case ESP_SPP_WRITE_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_SPP_WRITE_EVT");
        break;
        case ESP_SPP_SRV_OPEN_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_SPP_SRV_OPEN_EVT:rem_bda:%x:%x:%x:%x:%x:%x",
            param->srv_open.rem_bda[0],param->srv_open.rem_bda[1],param->srv_open.rem_bda[2],param->srv_open.rem_bda[3],param->srv_open.rem_bda[4],param->srv_open.rem_bda[5]);
            ESP_LOGI(MAIN_TASK_TAG, "handle:%ld, new handle:%ld", param->srv_open.handle, param->srv_open.new_listen_handle);
        break;
        case ESP_SPP_SRV_STOP_EVT:
            ESP_LOGI(MAIN_TASK_TAG, "ESP_SPP_SRV_STOP_EVT");
        break;
        default:
        break;
    }
    
}


void bd_send_txt(spp_queue_item_t item) {
    bd_client_t *end = bd_link_header;
    int colon=0;
    for (int i=0; i < item.len; i++) {
        if (item.in_data[i] == (uint8_t)':') {
            colon = i;
            break;
        }
    }
    if (colon == 0) return;
    char name[10];
    while(end) {
        memcpy((uint8_t*)name, end->bd_client_name, end->len);
        name[end->len]='\0';
        printf("%s:%li\n", name, end->handle);
        if (memcmp(end->bd_client_name, item.in_data+6, end->len)==0) {
            esp_spp_write(end->handle, item.len-(colon+1), item.in_data+colon+1);
            break;
        }
        end=end->next;
    }
    
}


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
    xQueueData2Boiler = xQueueCreate(5, sizeof(uint8_t));
    xQueueSpp = xQueueCreate(10, sizeof(spp_queue_item_t));

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

    if(xQueueData2Boiler == NULL)
        ESP_LOGE(MAIN_TASK_TAG, "xQueueData2Boiler initialization error!");

    if(xQueueBoilerTemp == NULL)
        ESP_LOGE(MAIN_TASK_TAG, "xQueueBoilerTemp initialization error!");

    if(xQueueSpp == NULL)
        ESP_LOGE(MAIN_TASK_TAG, "xQueueSpp initialization error!");

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

static void initBT(){

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_bt_controller_init(&bt_cfg);
    err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);

    err = esp_bluedroid_init();
    err = esp_bluedroid_enable();

    err = esp_spp_register_callback(my_bt_spp_cb);
    err = esp_spp_init(ESP_SPP_MODE_CB);

    err = esp_spp_start_srv(ESP_SPP_SEC_AUTHENTICATE, ESP_SPP_ROLE_MASTER, 1 , SPP_SERVER_1);

    err = esp_bt_gap_register_callback(my_bt_gap_cb);

    esp_bt_cod_t cod;
    cod.major = 0x1;
    cod.minor = 0x0;
    err = esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_MAJOR_MINOR);

    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
    err = esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, sizeof(esp_bt_io_cap_t));
    err = esp_bt_dev_set_device_name(BT_DEVICE_NAME);
    err = esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
    esp_bt_pin_code_t pin_code;
    esp_bt_gap_set_pin(pin_type, 0, pin_code);

}

void QueueReceiveTask(void* param) {
    spp_queue_item_t item;

    int count = 0;
    bool turnOff = true;

    while(1) {
        
        if (xQueueReceive(xQueueSpp, &item, portMAX_DELAY) == pdTRUE) {

            ESP_LOGW(MAIN_TASK_TAG, "x> %s", item.in_data);
            //esp_spp_write(end->handle, item.len-1, item.in_data+colon+1);
        }   
        vTaskDelay(100);
        /*
        ESP_LOGE(MAIN_TASK_TAG, "%d", count);
        if(count++ > 3){
            count = 0;
            if(turnOff){
                esp_bluedroid_disable();
                esp_bluedroid_deinit();
                esp_bt_controller_disable();
                turnOff = false;
                ESP_LOGW(MAIN_TASK_TAG, "blue disabled");
            }
            else{
                esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);

                esp_bluedroid_init();
                esp_bluedroid_enable();

                turnOff = true;
                ESP_LOGW(MAIN_TASK_TAG, "blue enabled");
            }
        }
        */
    } 
}

void app_main(void)
{

    initQueues();

    initGPIO();
    initI2C();
    initLEDC();
    initNextion();
    initPID();
    initNVS();
    initBT();

    initI2C_MCP23017_Out();
    initI2C_MCP23017_In();

    initControlTask();
    initUiTask();
    initBoilerTask();
    initInterruptsTask();
    initBtTask();

    xTaskCreatePinnedToCore(QueueReceiveTask, "TASK", 5120, 0, 10,0,1);


}

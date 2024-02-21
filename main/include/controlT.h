#ifndef CONTROLT_H
#define CONTROLT_H

#include "extend.h"
#include "boilerT.h"
#include "interruptsT.h"
#include "uiT.h"



typedef enum{
    IDLE_C = 0,
    MAINTENANCE_C,
    CLEAN_C,
    DRINK_C,
    WATER_C,
    SWAP_PAGE_C
} ControlState;

typedef enum{
    IDLE_SYNC_C = 0,
    SEND_RECIPES_C,
    POST_SEND_RECIPES_C,
    SEND_TEMPERATURE_C,
    POST_TEMPERATURE_C,
    END_SYNC_C
} SyncroState;


typedef struct ControlDataStruct{
    ControlState    controlState;
    uint8_t         recipeIndex;
    Page            page;
} ControlData;



typedef struct ContainerPowStruct{
    uint8_t refGrPowA;
    uint8_t refGrPowB;
    uint8_t refGrPowC
} PowderGramsRefData;


extern TaskHandle_t controlTaskH;

enum{
    SPP_IDX_SVC,

    SPP_IDX_SPP_DATA_RECV_CHAR,
    SPP_IDX_SPP_DATA_RECV_VAL,

    SPP_IDX_SPP_DATA_NOTIFY_CHAR,
    SPP_IDX_SPP_DATA_NTY_VAL,
    SPP_IDX_SPP_DATA_NTF_CFG,

    SPP_IDX_NB,
};




/// SPP Service
static const uint16_t spp_service_uuid = 0xABF0;
static const uint8_t spp_adv_data[23] = {
    /* Flags */
    0x02,0x01,0x06,
    /* Complete List of 16-bit Service Class UUIDs */
    0x03,0x03,0xF0,0xAB,
    /* Complete Local Name in advertising */
    0x0F,0x09, 'O', 'p', 'e', 'n', 'C', 'o', 'l', 'i', 'b', 'r', 'i'
};

static uint16_t spp_mtu_size = 23;
static uint16_t spp_conn_id = 0xffff;
static esp_gatt_if_t spp_gatts_if = 0xff;


static bool enable_data_ntf = false;
static bool is_connected = false;
static esp_bd_addr_t spp_remote_bda = {0x0,};

static uint16_t spp_handle_table[SPP_IDX_NB];

static esp_ble_adv_params_t spp_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};




/*
static void runDrink1(uint8_t *dataBytes, ContsPowderData *contsPowData);   //2 delete
static void runDrink2(uint8_t *dataBytes, ContsPowderData *contsPowData);   //2 delete
static void runDrink3(uint8_t *dataBytes, ContsPowderData *contsPowData);   //2 delete
static void runDrink4(uint8_t *dataBytes, ContsPowderData *contsPowData);   //2 delete
static void runDrink5(uint8_t *dataBytes, ContsPowderData *contsPowData);   //2 delete
static void runDrink6(uint8_t *dataBytes, ContsPowderData *contsPowData);   //2 delete
static void runDrink7(uint8_t *dataBytes, ContsPowderData *contsPowData);   //2 delete
static void runDrink8(uint8_t *dataBytes, ContsPowderData *contsPowData);   //2 delete
*/
static void runDrink(const Recipe *myRecipe, uint8_t *dataBytes, PowderGramsRefData *powderData);
void writeBytesMCP2307(const uint8_t i2cAddr, uint8_t regAddr, uint8_t *dataBuff, uint8_t numOfBytes);
static bool bitChangedInByte(uint8_t *byteData, const uint8_t bitPos, const bool newValue);
static bool differentRelayState(uint8_t *byteData, const OutputRelays outputRelays, const bool newValue);
static void setBit2Byte(uint8_t *byteData, const uint8_t bitPos);
static void clearBit2Byte(uint8_t *byteData, const uint8_t bitPos);
static void setRelay(uint8_t *byteData, const OutputRelays outputRelays);
static void resetRelay(uint8_t *byteData, const OutputRelays outputRelays);
static void checkAirBreak(uint8_t *dataBytes);
static void resetBrewer(uint8_t *dataBytes);
static void clearCoffeeChamber(uint8_t *dataBytes);
static void setBrewer2StartPosition(uint8_t *dataBytes);
static void setBrewer2InjectPosition(uint8_t *dataBytes);
static void injecBrewerWater(uint8_t *dataBytes, const uint16_t pulses);
static void injectPowderPlusWater(uint8_t *dataBytes, const uint16_t pulses, uint8_t gr, uint8_t powderRefGram, const OutputRelays outputRelay);
static void injectPowderPlusWaterExtraContainer(const uint16_t pulses, const uint8_t gr, uint8_t powderRefGram);
static void injectOnlyWaterLine(uint8_t *dataBytes, const uint16_t pulses);
static void injectOnlyWaterLineManual(uint8_t *dataBytes);
static bool grindAndDeliver(uint8_t *dataBytes, bool checkStop);
static void cleanMachine(uint8_t *dataBytes);
static void calculatePulseTime(uint8_t *dataBytes);
static void waitMachine2Start(uint8_t *dataBytes);
static void waitBoiler2Start();
static void startBoilerTask();
static void syncronizeAllTasks();
static bool checkWaterBtnFromUi();
static bool checkStopBtnFromUi();

static void checkQueuesFromUi(ControlData *controlData);
static void initMemData(Recipe *recipeData, SystemData *sysData, PowderGramsRefData *powderData);
static void checkMemContent(const Recipe *recipeData, const SystemData *sysData, PowderGramsRefData *powderData);
static void checkOnlyRecipeMemContent(const Recipe *recipeData);
static void loadFakeMemContent(Recipe *recipeData, SystemData *sysData, PowderGramsRefData *powderData);
static void btSubTask(bool *btEnabled);



static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void initBLUE();

void initI2C_MCP23017_Out();
void initI2C_MCP23017_In();
void initControlTask();
static void controlTask(void *pvParameters);

#endif
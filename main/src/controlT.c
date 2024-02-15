#include "controlT.h"


TaskHandle_t controlTaskH = NULL;

static struct gatts_profile_inst spp_profile_tab[SPP_PROFILE_NUM] = {
    [SPP_PROFILE_APP_IDX] = {
        .gatts_cb = gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

/*
 *  SPP PROFILE ATTRIBUTES
 ****************************************************************************************
 */

#define CHAR_DECLARATION_SIZE   (sizeof(uint8_t))
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

static const uint8_t char_prop_read_notify = ESP_GATT_CHAR_PROP_BIT_READ|ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_read_write = ESP_GATT_CHAR_PROP_BIT_WRITE_NR|ESP_GATT_CHAR_PROP_BIT_READ;

///SPP Service - data receive characteristic, read&write without response
static const uint16_t spp_data_receive_uuid = ESP_GATT_UUID_SPP_DATA_RECEIVE;
static const uint8_t  spp_data_receive_val[20] = {0x00};

///SPP Service - data notify characteristic, notify&read
static const uint16_t spp_data_notify_uuid = ESP_GATT_UUID_SPP_DATA_NOTIFY;
static const uint8_t  spp_data_notify_val[20] = {0x00};
static const uint8_t  spp_data_notify_ccc[2] = {0x00, 0x00};


char *recvBuff;
Recipe *recipeList;

///Full HRS Database Description - Used to add attributes into the database
static const esp_gatts_attr_db_t spp_gatt_db[SPP_IDX_NB] =
{
    //SPP -  Service Declaration
    [SPP_IDX_SVC]                      	=
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
    sizeof(spp_service_uuid), sizeof(spp_service_uuid), (uint8_t *)&spp_service_uuid}},

    //SPP -  data receive characteristic Declaration
    [SPP_IDX_SPP_DATA_RECV_CHAR]            =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
    CHAR_DECLARATION_SIZE,CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},

    //SPP -  data receive characteristic Value
    [SPP_IDX_SPP_DATA_RECV_VAL]             	=
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&spp_data_receive_uuid, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
    SPP_DATA_MAX_LEN,sizeof(spp_data_receive_val), (uint8_t *)spp_data_receive_val}},

    //SPP -  data notify characteristic Declaration
    [SPP_IDX_SPP_DATA_NOTIFY_CHAR]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
    CHAR_DECLARATION_SIZE,CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_notify}},

    //SPP -  data notify characteristic Value
    [SPP_IDX_SPP_DATA_NTY_VAL]   =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&spp_data_notify_uuid, ESP_GATT_PERM_READ,
    SPP_DATA_MAX_LEN, sizeof(spp_data_notify_val), (uint8_t *)spp_data_notify_val}},

    //SPP -  data notify characteristic - Client Characteristic Configuration Descriptor
    [SPP_IDX_SPP_DATA_NTF_CFG]         =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
    sizeof(uint16_t),sizeof(spp_data_notify_ccc), (uint8_t *)spp_data_notify_ccc}},

};

static void processBlueData(uint8_t hIndex, uint8_t tIndex){

    uint8_t tempU8 = 0;

    char *tempStr = malloc(17);
    memset(tempStr, 0, 17);

    hIndex += 2;

    tempStr[0] = recvBuff[hIndex++];
    tempStr[1] = recvBuff[hIndex++];
    tempStr[2] = '\0';

    uint8_t rSlot = atoi(tempStr);

    memset(tempStr, 0, 17);

    for(size_t i = hIndex; i < tIndex; i++){

        if(recvBuff[i] == '|'){
            tempStr[i - hIndex] = '\0';
            hIndex = (i + 1);
            break;
        }
        else{
            tempStr[i - hIndex] = recvBuff[i];
        }

    }

    strcpy(recipeList[rSlot].recipeName, tempStr);
  
    memset(tempStr, 0, 17);


    for(size_t i = hIndex; i < tIndex; i++){
        if(recvBuff[i] == '|')
            tempU8++;
    }

    if(recipeList[rSlot].moduleSize != 0)
        free(recipeList[rSlot].modulesArray);


    recipeList[rSlot].moduleSize = tempU8;

    recipeList[rSlot].modulesArray = (RecipeModuleStruct *)malloc(sizeof(RecipeModuleStruct) * tempU8);


    size_t j = hIndex;

    do{

        switch(recvBuff[j++]){
            case 'C':                       //coffee
                
            break;
            case 'r':                       //pre coffee

            break;
            case 'a':                       //powder a

            break;
            case 'b':                       //powder b

            break;
            case 'c':                       //powder c

            break;
            case 'w':                       //water

            break;
        }

    }while(j < tIndex);




    
    free(tempStr);
}


static bool append2Command(uint8_t *dataFromBlue, uint16_t dataSize){
    bool commandFound = false;
    static int appendPC = 0;


    for(size_t i = 0; i < dataSize; i++)
        *(recvBuff + (appendPC++)) = *(dataFromBlue + i);
        

    uint8_t headIndex = -1;
    uint8_t tailIndex = -1;

    for(size_t i = 0; i < 64; i++){
        if((headIndex == -1) && (*(recvBuff + i) == '#'))     
            headIndex = i;
        else if(headIndex != -1 && (*(recvBuff + i) == '~')){
            tailIndex = i;
            commandFound = true;
            break;
        }

    }


    if(commandFound){

        switch(recvBuff[headIndex + 1]){
            case 'z':                                               //get all data

            break;
            case 'd':                                               //data recipe
                processBlueData(headIndex, tailIndex);
            break;
        }

        appendPC = 0;

        memset(recvBuff, 0, 64);
        //*(recvBuff + (tailIndex + 1)) = '\0';
    }
    


    return commandFound;
}


static uint8_t find_char_and_desr_index(uint16_t handle)
{
    uint8_t error = 0xff;

    for(int i = 0; i < SPP_IDX_NB ; i++){
        if(handle == spp_handle_table[i]){
            return i;
        }
    }

    return error;
}


static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;

    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&spp_adv_params);
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        //advertising start complete event to indicate advertising start successfully or failed
        if((err = param->adv_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(CONTROL_TASK_TAG, "Advertising start failed: %s\n", esp_err_to_name(err));
        }
        break;
    default:
        break;
    }
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    esp_ble_gatts_cb_param_t *p_data = (esp_ble_gatts_cb_param_t *) param;
    uint8_t res = 0xff;


    switch (event) {
    	case ESP_GATTS_REG_EVT:
        	esp_ble_gap_set_device_name(SAMPLE_DEVICE_NAME);
        	esp_ble_gap_config_adv_data_raw((uint8_t *)spp_adv_data, sizeof(spp_adv_data));
        	esp_ble_gatts_create_attr_tab(spp_gatt_db, gatts_if, SPP_IDX_NB, SPP_SVC_INST_ID);
       	break;
    	case ESP_GATTS_WRITE_EVT: {
    	    res = find_char_and_desr_index(p_data->write.handle);
            if(p_data->write.is_prep == false){
                
                if(res == SPP_IDX_SPP_DATA_NTF_CFG){
                    if((p_data->write.len == 2)&&(p_data->write.value[0] == 0x01)&&(p_data->write.value[1] == 0x00)){
                        enable_data_ntf = true;
                    }else if((p_data->write.len == 2)&&(p_data->write.value[0] == 0x00)&&(p_data->write.value[1] == 0x00)){
                        enable_data_ntf = false;
                    }

                    ESP_LOGW("ME", "Ntf: %d", enable_data_ntf);
                }
                else if(res == SPP_IDX_SPP_DATA_RECV_VAL){

                    

                    ESP_LOGW("ME", "%s --- %i", p_data->write.value, p_data->write.len);

                    char *sendBuffer = malloc(18);

                    memset(sendBuffer, 0, 18);
                    
                    if(append2Command(p_data->write.value, p_data->write.len)){
                        ESP_LOGI("BLUE", "%s", recvBuff);
                        strcpy(sendBuffer, "#f~");
                    }
                    else{
                        strcpy(sendBuffer, "#n~");
                    }
                    

                    esp_ble_gatts_set_attr_value(spp_handle_table[SPP_IDX_SPP_DATA_NTY_VAL], strlen(sendBuffer), (uint8_t *)sendBuffer);
    

                    free(sendBuffer);
                    

                }else{
                    //TODO:
                }
            }else if((p_data->write.is_prep == true)&&(res == SPP_IDX_SPP_DATA_RECV_VAL)){

                
            }
      	 	break;
    	}
    	case ESP_GATTS_EXEC_WRITE_EVT:{
    	    if(p_data->exec_write.exec_write_flag){
                
                ESP_LOGE("NANI", "What!!!!");
            
    	    }
    	    break;
    	}
    	case ESP_GATTS_MTU_EVT:
    	    spp_mtu_size = p_data->mtu.mtu;
    	    break;
    	case ESP_GATTS_CONF_EVT:
    	    break;
    	case ESP_GATTS_UNREG_EVT:
        	break;
    	case ESP_GATTS_DELETE_EVT:
        	break;
    	case ESP_GATTS_START_EVT:
        	break;
    	case ESP_GATTS_STOP_EVT:
        	break;
    	case ESP_GATTS_CONNECT_EVT:
    	    spp_conn_id = p_data->connect.conn_id;
    	    spp_gatts_if = gatts_if;
    	    is_connected = true;
    	    memcpy(&spp_remote_bda,&p_data->connect.remote_bda,sizeof(esp_bd_addr_t));
        	break;
    	case ESP_GATTS_DISCONNECT_EVT:
    	    is_connected = false;
    	    enable_data_ntf = false;
    	    esp_ble_gap_start_advertising(&spp_adv_params);
    	    break;
    	case ESP_GATTS_OPEN_EVT:
    	    break;
    	case ESP_GATTS_CANCEL_OPEN_EVT:
    	    break;
    	case ESP_GATTS_CLOSE_EVT:
    	    break;
    	case ESP_GATTS_LISTEN_EVT:
    	    break;
    	case ESP_GATTS_CONGEST_EVT:
    	    break;
    	case ESP_GATTS_CREAT_ATTR_TAB_EVT:{

    	    if (param->add_attr_tab.status != ESP_GATT_OK){

    	    }
    	    else if (param->add_attr_tab.num_handle != SPP_IDX_NB){

    	    }
    	    else {
    	        memcpy(spp_handle_table, param->add_attr_tab.handles, sizeof(spp_handle_table));
    	        esp_ble_gatts_start_service(spp_handle_table[SPP_IDX_SVC]);
    	    }
    	    break;
    	}
    	default:
    	    break;
    }
}


static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param){

    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            spp_profile_tab[SPP_PROFILE_APP_IDX].gatts_if = gatts_if;
        } else {
            return;
        }
    }

    do {
        int idx;
        for (idx = 0; idx < SPP_PROFILE_NUM; idx++) {
            if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                    gatts_if == spp_profile_tab[idx].gatts_if) {
                if (spp_profile_tab[idx].gatts_cb) {
                    spp_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}


void runDrink(const Recipe *myRecipe, uint8_t *dataBytes, PowderGramsRefData *powderData){
    
    bool runIt = true;
    bool preCoffee = false;


    if(myRecipe->moduleSize > 1){
        for(size_t i = 1; i < myRecipe->moduleSize; i++){
            if(myRecipe->modulesArray[i].moduleType == COFFEE_PRE_M){
                preCoffee = true;
                break;
            }
        }
    }


    if(preCoffee){
        runIt = grindAndDeliver(dataBytes, true);

        vTaskDelay(pdMS_TO_TICKS(1000));

        if(runIt){
            setBrewer2InjectPosition(dataBytes);

            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    if(runIt){
        
        for(size_t i = 0; i < myRecipe->moduleSize; i++){
        
            switch(myRecipe->modulesArray[i].moduleType){
                case COFFEE_M:
            
                    if(i == 0){
                        runIt = grindAndDeliver(dataBytes, true);

                        vTaskDelay(pdMS_TO_TICKS(1000));

                        if(!runIt)
                            break;

                        setBrewer2InjectPosition(dataBytes);

                        vTaskDelay(pdMS_TO_TICKS(1000));     
                    }
                    else{
                        grindAndDeliver(dataBytes, false);

                        vTaskDelay(pdMS_TO_TICKS(1000));

                        setBrewer2InjectPosition(dataBytes);

                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }

                    injecBrewerWater(dataBytes, myRecipe->modulesArray[i].pulses);

                    vTaskDelay(pdMS_TO_TICKS(2500));

                    setBrewer2StartPosition(dataBytes);
                break;
                case COFFEE_PRE_M:
                    injecBrewerWater(dataBytes, myRecipe->modulesArray[i].pulses);

                    vTaskDelay(pdMS_TO_TICKS(2500));

                    setBrewer2StartPosition(dataBytes);
                break;
                case POWDER_A_M:
                    injectPowderPlusWater(dataBytes, myRecipe->modulesArray[i].pulses, myRecipe->modulesArray[i].gr, powderData->refGrPowA, DOSER_DEVICE_1);
                break;
                case POWDER_B_M:
                    injectPowderPlusWater(dataBytes, myRecipe->modulesArray[i].pulses, myRecipe->modulesArray[i].gr, powderData->refGrPowB, DOSER_DEVICE_2);
                break;
                case POWDER_C_M:
                    injectPowderPlusWaterExtraContainer(myRecipe->modulesArray[i].pulses, myRecipe->modulesArray[i].gr, powderData->refGrPowC);
                break;
                case WATER:
                    injectOnlyWaterLine(dataBytes, myRecipe->modulesArray[i].pulses);
                break;
            }

            if(!runIt)
                break;

            vTaskDelay(pdMS_TO_TICKS(1000));   

        }
    }


}

/*

void runDrink1(uint8_t *dataBytes, ContsPowderData *contsPowData){

    grindAndDeliver(dataBytes, false);

    vTaskDelay(pdMS_TO_TICKS(1000));

    setBrewer2InjectPosition(dataBytes);

    vTaskDelay(pdMS_TO_TICKS(1000));

    
    injecBrewerWater(dataBytes, 142);
    

    vTaskDelay(pdMS_TO_TICKS(2500));

    setBrewer2StartPosition(dataBytes);  

}

void runDrink2(uint8_t *dataBytes, ContsPowderData *contsPowData){

    grindAndDeliver(dataBytes, false);

    vTaskDelay(pdMS_TO_TICKS(1000));

    setBrewer2InjectPosition(dataBytes);

    vTaskDelay(pdMS_TO_TICKS(1000));

    xTaskNotify(boilerTaskH, 0x10, eSetBits);
    injecBrewerWater(dataBytes, 220);                //78
    xTaskNotify(boilerTaskH, 0x08, eSetBits);

    vTaskDelay(pdMS_TO_TICKS(2500));

    setBrewer2StartPosition(dataBytes);  
      

}

void runDrink3(uint8_t *dataBytes, ContsPowderData *contsPowData){

    */
    //injectPowderPlusWater(dataBytes, 100);
    /*
    setRelay(dataBytes, DOSER_DEVICE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    double tTime = esp_timer_get_time();

    vTaskDelay(pdMS_TO_TICKS(6000));   //11 gr

    ESP_LOGE(CONTROL_TASK_TAG, "time: %lf", (esp_timer_get_time() - tTime));

    resetRelay(dataBytes, DOSER_DEVICE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(100));
    */

/*
   xTaskNotify(boilerTaskH, 0x20, eSetBits);
   injectPowderPlusWater(dataBytes, 60, contsPowData, POWDER_A_M);
   xTaskNotify(boilerTaskH, 0x08, eSetBits);

}

void runDrink4(uint8_t *dataBytes, ContsPowderData *contsPowData){

    injectPowderPlusWater(dataBytes, 128, contsPowData, POWDER_A_M);      
    xTaskNotify(boilerTaskH, 0x08, eSetBits);

}

void runDrink5(uint8_t *dataBytes, ContsPowderData *contsPowData){
    xTaskNotify(boilerTaskH, 0x20, eSetBits);

    vTaskDelay(pdMS_TO_TICKS(7000));

    setRelay(dataBytes, SOLENOID_VALVE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700));
    
    setRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 


    vTaskDelay(pdMS_TO_TICKS(22000));


    resetRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    xTaskNotify(boilerTaskH, 0x08, eSetBits);

    vTaskDelay(pdMS_TO_TICKS(1000));

    resetRelay(dataBytes, SOLENOID_VALVE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    ESP_LOGE(CONTROL_TASK_TAG, "Inject only water: OFF");
*/

    /*
    setBrewer2InjectPosition(dataBytes);

    vTaskDelay(pdMS_TO_TICKS(1000));

    xTaskNotify(boilerTaskH, 0x10, eSetBits);
    injecBrewerWater(dataBytes, 280);
    xTaskNotify(boilerTaskH, 0x08, eSetBits);

    vTaskDelay(pdMS_TO_TICKS(2500));

    setBrewer2StartPosition(dataBytes);

    //injecBrewerWater(dataBytes, 280);   //280 is the max
    */
//}

/*
void runDrink6(uint8_t *dataBytes, ContsPowderData *contsPowData){
    setBrewer2InjectPosition(dataBytes);

    vTaskDelay(pdMS_TO_TICKS(5000));

    setBrewer2StartPosition(dataBytes);
}

void runDrink7(uint8_t *dataBytes, ContsPowderData *contsPowData){

}

void runDrink8(uint8_t *dataBytes, ContsPowderData *contsPowData){
    calculatePulseTime(dataBytes);
}
*/

void writeBytesMCP2307(const uint8_t i2cAddr, uint8_t regAddr, uint8_t *dataBuff, uint8_t numOfBytes){
    i2c_cmd_handle_t cmdHandle = i2c_cmd_link_create();

    i2c_master_start(cmdHandle);
    i2c_master_write_byte(cmdHandle, i2cAddr << 1 | I2C_MASTER_WRITE, I2C_MASTER_ACK);
    i2c_master_write_byte(cmdHandle, regAddr, I2C_MASTER_ACK);
    for(size_t i = 0; i < numOfBytes; i++)
        i2c_master_write_byte(cmdHandle, *(dataBuff + i), I2C_MASTER_ACK);
    
    i2c_master_stop(cmdHandle);

    int cmdReturn = i2c_master_cmd_begin(I2C_PORT_NUM, cmdHandle, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmdHandle);

}

static bool bitChangedInByte(uint8_t *byteData, const uint8_t bitPos, const bool newValue){
    bool bitA = (bool)(((*byteData) >> bitPos) & 1);

    if(bitA == newValue)
        return false;
    else
        return true;
}

//Used to check if the desires relay state has already changed, this to now over use setRelay function
static bool differentRelayState(uint8_t *byteData, const OutputRelays outputRelays, const bool newValue){
    bool itsDifferent = false;

    switch(outputRelays){
        case SOLENOID_VALVE_2:
            itsDifferent = bitChangedInByte(byteData + 1, 6, newValue);
            break;
        case SOLENOID_VALVE_1:
            itsDifferent = bitChangedInByte(byteData + 1, 5, newValue);
            break;
        case COFFEE_BREWER_MOTOR:
            itsDifferent = bitChangedInByte(byteData + 1, 4, newValue);
            break;
        case PUMP:
            itsDifferent = bitChangedInByte(byteData + 1, 3, newValue);
            break;
        case COFFEE_GRINDER_MOTOR:
            itsDifferent = bitChangedInByte(byteData + 1, 2, newValue);
            break;
        case COFFEE_RELEASE_MAGNET:
            itsDifferent = bitChangedInByte(byteData + 1, 1, newValue);
            break;
        case THREE_WAY_VALVE:
            itsDifferent = bitChangedInByte(byteData + 1, 0, newValue);
            break;
        case CUP_RELEASE_MOTOR:
            itsDifferent = bitChangedInByte(byteData, 7, newValue);
            break;
        case CUP_STACKER_SHIFT_MOTOR:
            itsDifferent = bitChangedInByte(byteData, 6, newValue);
            break;
        case WATER_INLET_VALVE:
            itsDifferent = bitChangedInByte(byteData, 5, newValue);
            break;
        case DOSER_DEVICE_1:
            itsDifferent = bitChangedInByte(byteData, 4, newValue);
            break;
        case DOSER_DEVICE_2:
            itsDifferent = bitChangedInByte(byteData, 3, newValue);
            break;
        case WHIPPER:
            itsDifferent = bitChangedInByte(byteData, 1, newValue);
            break;
    }

    return itsDifferent;
}

static void setBit2Byte(uint8_t *byteData, const uint8_t bitPos){
    *byteData |= (1 << bitPos);
}

static void clearBit2Byte(uint8_t *byteData, const uint8_t bitPos){
    *byteData &= ~(1 << bitPos);
}

static void setRelay(uint8_t *byteData, const OutputRelays outputRelays){
    switch(outputRelays){
        case SOLENOID_VALVE_2:
            setBit2Byte(byteData + 1, 6);
            break;
        case SOLENOID_VALVE_1:
            setBit2Byte(byteData + 1, 5);
            break;
        case COFFEE_BREWER_MOTOR:
            setBit2Byte(byteData + 1, 4);
            break;
        case PUMP:
            setBit2Byte(byteData + 1, 3);
            break;
        case COFFEE_GRINDER_MOTOR:
            setBit2Byte(byteData + 1, 2);
            break;
        case COFFEE_RELEASE_MAGNET:
            setBit2Byte(byteData + 1, 1);
            break;
        case THREE_WAY_VALVE:
            setBit2Byte(byteData + 1, 0);
            break;
        case CUP_RELEASE_MOTOR:
            setBit2Byte(byteData, 7);
            break;
        case CUP_STACKER_SHIFT_MOTOR:
            setBit2Byte(byteData, 6);
            break;
        case WATER_INLET_VALVE:
            setBit2Byte(byteData, 5);
            break;
        case DOSER_DEVICE_1:
            setBit2Byte(byteData, 4);
            break;
        case DOSER_DEVICE_2:
            setBit2Byte(byteData, 3);
            break;
        case WHIPPER:
            setBit2Byte(byteData, 1);
            break;
    }
}

static void resetRelay(uint8_t *byteData, const OutputRelays outputRelays){
    switch(outputRelays){
        case SOLENOID_VALVE_2:
            clearBit2Byte(byteData + 1, 6);
            break;
        case SOLENOID_VALVE_1:
            clearBit2Byte(byteData + 1, 5);
            break;
        case COFFEE_BREWER_MOTOR:
            clearBit2Byte(byteData + 1, 4);
            break;
        case PUMP:
            clearBit2Byte(byteData + 1, 3);
            break;
        case COFFEE_GRINDER_MOTOR:
            clearBit2Byte(byteData + 1, 2);
            break;
        case COFFEE_RELEASE_MAGNET:
            clearBit2Byte(byteData + 1, 1);
            break;
        case THREE_WAY_VALVE:
            clearBit2Byte(byteData + 1, 0);
            break;
        case CUP_RELEASE_MOTOR:
            clearBit2Byte(byteData, 7);
            break;
        case CUP_STACKER_SHIFT_MOTOR:
            clearBit2Byte(byteData, 6);
            break;
        case WATER_INLET_VALVE:
            clearBit2Byte(byteData, 5);
            break;
        case DOSER_DEVICE_1:
            clearBit2Byte(byteData, 4);
            break;
        case DOSER_DEVICE_2:
            clearBit2Byte(byteData, 3);
            break;
        case WHIPPER:
            clearBit2Byte(byteData, 1);
            break;
    }
}

static void checkAirBreak(uint8_t *dataBytes){

    InputSwStruct inputSwStruct;    

    if(xQueuePeek(xQueueInputsSw, (void *) &inputSwStruct, pdMS_TO_TICKS(60))){

        if(inputSwStruct.airBreakSw){

            setRelay(dataBytes, WATER_INLET_VALVE);
            writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  
            ESP_LOGW(CONTROL_TASK_TAG, "OPEN WATER_INLET_VALVE");

            xTaskNotify(intTaskH, 0x01, eSetBits);                  //notify interrupt task for water flow control

            vTaskDelay(pdMS_TO_TICKS(100)); 

            bool onWait = true;
            uint32_t ulNotifiedValue = 0;
            
            do{

                xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, portMAX_DELAY);

                if((ulNotifiedValue & 0x10) >> 4){

                    resetRelay(dataBytes, WATER_INLET_VALVE);
                    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  
                    vTaskDelay(pdMS_TO_TICKS(30));

                    ESP_LOGW(CONTROL_TASK_TAG, "CLOSE WATER_INLET_VALVE");

                    onWait = false;
                }

            }while(onWait);

            ESP_LOGI(CONTROL_TASK_TAG, "END AIRBREAK CHECK");
        }
    }

}

void resetBrewer(uint8_t *dataBytes){
    InputSwStruct inputSwStruct;

    setRelay(dataBytes, COFFEE_BREWER_MOTOR);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  

    int countIt = 0;

    do{
        if(xQueuePeek(xQueueInputsSw, (void *) &inputSwStruct, pdMS_TO_TICKS(60))){
            if(!inputSwStruct.coffeeBrewerSw){
                countIt++;
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            if(countIt > 1){
                vTaskDelay(pdMS_TO_TICKS(2700));
                resetRelay(dataBytes, COFFEE_BREWER_MOTOR);
                writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  
            }

        }

        vTaskDelay(pdMS_TO_TICKS(10));

    }while(countIt < 2);

}

void clearCoffeeChamber(uint8_t *dataBytes){

    vTaskDelay(pdMS_TO_TICKS(700));

    setRelay(dataBytes, COFFEE_RELEASE_MAGNET);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);    

    vTaskDelay(pdMS_TO_TICKS(1000));

    resetRelay(dataBytes, COFFEE_RELEASE_MAGNET);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  

    ESP_LOGI(CONTROL_TASK_TAG, "Coffee released :)");

    vTaskDelay(pdMS_TO_TICKS(700));
}

void setBrewer2StartPosition(uint8_t *dataBytes){

    setRelay(dataBytes, COFFEE_BREWER_MOTOR);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);     

    vTaskDelay(pdMS_TO_TICKS(3700));

    resetRelay(dataBytes, COFFEE_BREWER_MOTOR);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  
    
}

void setBrewer2InjectPosition(uint8_t *dataBytes){
    InputSwStruct inputSwStruct;

    setRelay(dataBytes, COFFEE_BREWER_MOTOR);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  

    bool waiting = true;

    do{
        if(xQueuePeek(xQueueInputsSw, (void *) &inputSwStruct, pdMS_TO_TICKS(60))){
            if(!inputSwStruct.coffeeBrewerSw)
                waiting = false;
        }    

        vTaskDelay(pdMS_TO_TICKS(10));

    }while(waiting);

    resetRelay(dataBytes, COFFEE_BREWER_MOTOR);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

}

void injecBrewerWater(uint8_t *dataBytes, const uint16_t pulses){

    uint32_t ulNotifiedValue = 0;
    bool onWait = true;

    double startTime = 0;
    double endTime = 0;

    ESP_LOGI(CONTROL_TASK_TAG, "Inject brewer water: ON");

    xTaskNotify(boilerTaskH, 0x10, eSetBits);

    setRelay(dataBytes, THREE_WAY_VALVE);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700));
    
    setRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    startTime = esp_timer_get_time();

    xQueueSend(xQueueInputPulse, (void *) &pulses, portMAX_DELAY);

    do{
        xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, portMAX_DELAY);

        if((ulNotifiedValue & 0x08) >> 3){
            onWait = false;
            endTime = esp_timer_get_time() - startTime;
            ESP_LOGI(CONTROL_TASK_TAG, "TURNING OFF PUMP %lf", endTime);
        }

    }while(onWait);


    resetRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(1000));

    resetRelay(dataBytes, THREE_WAY_VALVE);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    ESP_LOGE(CONTROL_TASK_TAG, "Inject brewer water: OFF");

    xTaskNotify(boilerTaskH, 0x08, eSetBits);
}

static void injectPowderPlusWater(uint8_t *dataBytes, const uint16_t pulses, uint8_t gr, uint8_t powderRefGram, const OutputRelays outputRelay){

    uint32_t ulNotifiedValue = 0;
    bool onWait = true; 
    bool injectPowder = true;


    bool gramsTimeGreaterThanHeatTime = true;
    bool withPreGramsTime = false;
    bool preHeatOn = false;
    bool prePowderOn = false;


    //15g -> 10s
    int targetGrams = 40;                                                       //** 40 gr
    double preHeatTime = 7000000;                                               //** 7 sec


    double rTime = 0;
    double cTime = 0;

    //16000000 16 seg * 1000000

    //10 seg = 15 gr
    //12 seg = 18 gr
    double tTime = ((double)((float)gr * 10.0f)/((float)powderRefGram)) * 1000000;


    
    ESP_LOGW(CONTROL_TASK_TAG, "powder1: %d - %lf", gr, tTime);
    /*
    gr = 18;
    tempValue = ((float)gr * 10.0f)/((float)powderRefGram);
    tTime = tempValue * 1000000;

    ESP_LOGW(CONTROL_TASK_TAG, "powder2: %f - %lf", tempValue, tTime);

    gr = 20;
    tempValue = ((float)gr * 10.0f)/((float)powderRefGram);
    tTime = tempValue * 1000000;

    ESP_LOGW(CONTROL_TASK_TAG, "powder3: %f - %lf", tempValue, tTime);

    gr = 30;
    tempValue = ((float)gr * 10.0f)/((float)powderRefGram);
    tTime = tempValue * 1000000;

    ESP_LOGW(CONTROL_TASK_TAG, "powder4: %f - %lf", tempValue, tTime);

    gr = 10;
    tempValue = ((float)gr * 10.0f)/((float)powderRefGram);
    tTime = tempValue * 1000000;

    ESP_LOGW(CONTROL_TASK_TAG, "powder5: %f - %lf", tempValue, tTime);

    gr = 5;
    tempValue = ((float)gr * 10.0f)/((float)powderRefGram);
    tTime = tempValue * 1000000;

    ESP_LOGW(CONTROL_TASK_TAG, "powder5: %f - %lf", tempValue, tTime);
    */

    /*
    double totalTime = contsPowData->timePerPulse * pulses;
    double gramsTime = (targetGrams / contsPowData->grPerSecCon1) * 1000000;
    double onlyGramsTime = gramsTime - totalTime;


    ESP_LOGI(CONTROL_TASK_TAG, "Inject powder water: ON - %lf - %lf = %lf", totalTime, gramsTime, onlyGramsTime);
    */

    
    ESP_LOGI(CONTROL_TASK_TAG, "Inject powder water: ON ");

    xTaskNotify(boilerTaskH, 0x20, eSetBits);

    setRelay(dataBytes, SOLENOID_VALVE_2);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700));

    setRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700));

    setRelay(dataBytes, WHIPPER);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);

    xQueueSend(xQueueInputPulse, (void *) &pulses, portMAX_DELAY);

    vTaskDelay(pdMS_TO_TICKS(400));

    setRelay(dataBytes, outputRelay);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    rTime = esp_timer_get_time();

    do{
        xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, pdMS_TO_TICKS(200));

        if((ulNotifiedValue & 0x08) >> 3){
            onWait = false;

            ESP_LOGI(CONTROL_TASK_TAG, "TURNING OFF PUMP");
        }

        if(injectPowder && onWait){
            cTime = esp_timer_get_time() - rTime;

            if(cTime >= tTime){
                injectPowder = false;
        
                ESP_LOGI(CONTROL_TASK_TAG, "TURNING DOSER OFF");
                resetRelay(dataBytes, outputRelay);
                writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 
            }
        }



    }while(onWait);

    if(injectPowder){
        resetRelay(dataBytes, outputRelay);
        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

        vTaskDelay(pdMS_TO_TICKS(400));
    }
    else
        vTaskDelay(pdMS_TO_TICKS(200));

    resetRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700));

    resetRelay(dataBytes, SOLENOID_VALVE_2);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(2000));

    resetRelay(dataBytes, WHIPPER);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  

    ESP_LOGE(CONTROL_TASK_TAG, "Inject powder water: OFF - %lf", cTime);

    xTaskNotify(boilerTaskH, 0x08, eSetBits);

}


static void injectPowderPlusWaterExtraContainer(const uint16_t pulses, const uint8_t gr, uint8_t powderRefGram){

}


static void injectOnlyWaterLine(uint8_t *dataBytes, const uint16_t pulses){

    uint32_t ulNotifiedValue = 0;
    bool onWait = true;

    ESP_LOGI(CONTROL_TASK_TAG, "Inject only water line: ON");

    xTaskNotify(boilerTaskH, 0x20, eSetBits);

    setRelay(dataBytes, SOLENOID_VALVE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700));
    
    setRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 


    xQueueSend(xQueueInputPulse, (void *) &pulses, portMAX_DELAY);

    do{
        xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, portMAX_DELAY);

        if((ulNotifiedValue & 0x08) >> 3){
            onWait = false;
            ESP_LOGI(CONTROL_TASK_TAG, "TURNING OFF PUMP");
        }

    }while(onWait);


    resetRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(1000));

    resetRelay(dataBytes, SOLENOID_VALVE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    ESP_LOGE(CONTROL_TASK_TAG, "Inject only water line: OFF");

    xTaskNotify(boilerTaskH, 0x08, eSetBits);

}

static void injectOnlyWaterLineManual(uint8_t *dataBytes){
    static const uint16_t maxPulse = 140;

    uint32_t ulNotifiedValue = 0;
    bool onWait = true;

    ESP_LOGI(CONTROL_TASK_TAG, "Inject only water line: ON");

    xTaskNotify(boilerTaskH, 0x20, eSetBits);

    setRelay(dataBytes, SOLENOID_VALVE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700));
    
    setRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 


    xQueueSend(xQueueInputPulse, (void *) &maxPulse, portMAX_DELAY);

    do{
        xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, pdMS_TO_TICKS(100));

        if((ulNotifiedValue & 0x08) >> 3){
            onWait = false;
            ESP_LOGI(CONTROL_TASK_TAG, "TURNING OFF PUMP");
        }
        
        if(checkWaterBtnFromUi()){
            onWait = false;
            ESP_LOGI(CONTROL_TASK_TAG, "TURNING OFF PUMP");
        }
        

    }while(onWait);


    resetRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(1000));

    resetRelay(dataBytes, SOLENOID_VALVE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    ESP_LOGE(CONTROL_TASK_TAG, "Inject only water line: OFF");

    xTaskNotify(boilerTaskH, 0x08, eSetBits);
}

/*
static void injectPowderPlusWater(uint8_t *dataBytes, const uint16_t pulses, ContsPowderData *contsPowData){

    uint32_t ulNotifiedValue = 0;
    bool onWait = true;
    int targetGrams = 40;
    double refTime = 0;

    double totalTime = contsPowData->timePerPulse * pulses;

    ESP_LOGI(CONTROL_TASK_TAG, "Inject powder water: ON - %lf", totalTime);

    setRelay(dataBytes, SOLENOID_VALVE_2);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700)); //700

    setRelay(dataBytes, DOSER_DEVICE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 
    
    setRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    refTime = esp_timer_get_time();

    vTaskDelay(pdMS_TO_TICKS(700));

    setRelay(dataBytes, WHIPPER);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 


    //vTaskDelay(pdMS_TO_TICKS(22000));
    xQueueSend(xQueueInputPulse, (void *) &pulses, portMAX_DELAY);

    do{
        xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, portMAX_DELAY);

        if((ulNotifiedValue & 0x08) >> 3){
            onWait = false;

            ESP_LOGI(CONTROL_TASK_TAG, "TURNING OFF PUMP - %lf", (esp_timer_get_time() - refTime));
        }

    }while(onWait);


    resetRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700));

    resetRelay(dataBytes, DOSER_DEVICE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    ESP_LOGI(CONTROL_TASK_TAG, "EXTRA TIME - %lf", (esp_timer_get_time() - refTime));

    vTaskDelay(pdMS_TO_TICKS(700));

    resetRelay(dataBytes, SOLENOID_VALVE_2);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(2000));

    resetRelay(dataBytes, WHIPPER);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    ESP_LOGE(CONTROL_TASK_TAG, "Inject powder water: OFF");


}
*/


static bool grindAndDeliver(uint8_t *dataBytes, bool checkStop){

    bool runProcess = true;
    bool wait2Finish = true;
    bool noCoffee = false;

    double refTime = 0;
    double cTime = 0;

    InputSwStruct inputSwStruct;

    setRelay(dataBytes, COFFEE_GRINDER_MOTOR);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    refTime = esp_timer_get_time();

    do{

        if(xQueuePeek(xQueueInputsSw, (void *) &inputSwStruct, pdMS_TO_TICKS(30))){
            
            if(inputSwStruct.coffeeReleaseSw)
                wait2Finish = false;
        }

        if(checkStop && checkStopBtnFromUi()){
            wait2Finish = false;
            runProcess = false;
        }

        if(wait2Finish){
            cTime = esp_timer_get_time() - refTime;
            //ESP_LOGW(BOILER_TASK_TAG, "grinder time: %lf", cTime);
            
            if( cTime >= 16000000){      //16 seg
                noCoffee = true;
                wait2Finish = false;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200));

    }while(wait2Finish);

    resetRelay(dataBytes, COFFEE_GRINDER_MOTOR);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700));

    if(runProcess){
        if(noCoffee){
            ESP_LOGE(CONTROL_TASK_TAG, "ERROR: NO COFFEE!!!");
            runProcess = false;
        }
        else{
            setRelay(dataBytes, COFFEE_RELEASE_MAGNET);
            writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);    

            vTaskDelay(pdMS_TO_TICKS(1000));

            resetRelay(dataBytes, COFFEE_RELEASE_MAGNET);
            writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  

            ESP_LOGI(CONTROL_TASK_TAG, "Coffee released :)");
        }
    }

    return runProcess;

}

static void cleanMachine(uint8_t *dataBytes){

    bool onWait = true; 
    uint16_t pulses = 142;
    uint32_t ulNotifiedValue = 0;

    ESP_LOGE(CONTROL_TASK_TAG, "Clean machine: ON");

    setBrewer2InjectPosition(dataBytes);

    vTaskDelay(pdMS_TO_TICKS(1000));  

    injecBrewerWater(dataBytes, pulses);

    vTaskDelay(pdMS_TO_TICKS(2500));

    setBrewer2StartPosition(dataBytes);

    vTaskDelay(pdMS_TO_TICKS(5000));

    ////////

    pulses = 160;

    xTaskNotify(boilerTaskH, 0x20, eSetBits);

    setRelay(dataBytes, SOLENOID_VALVE_2);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700));

    setRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700));

    setRelay(dataBytes, WHIPPER);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);

    xQueueSend(xQueueInputPulse, (void *) &pulses, portMAX_DELAY);

    do{
        xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, portMAX_DELAY);

        if((ulNotifiedValue & 0x08) >> 3){
            onWait = false;

            ESP_LOGI(CONTROL_TASK_TAG, "TURNING OFF PUMP");
        }

    }while(onWait);

    resetRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700));

    resetRelay(dataBytes, SOLENOID_VALVE_2);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(2000));

    resetRelay(dataBytes, WHIPPER);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  

    ESP_LOGE(CONTROL_TASK_TAG, "Clean machine: OFF");

    xTaskNotify(boilerTaskH, 0x08, eSetBits);

}

void calculatePulseTime(uint8_t *dataBytes){

    bool onWait = true;
    uint16_t pulses = 20;
    uint32_t ulNotifiedValue = 0;
    double pulseTimeArray[] = {0, 0, 0};


    for(size_t i = 0; i < 4; i++){

        if(i != 0)
            xTaskNotify(intTaskH, 0x02, eSetBits);

        setRelay(dataBytes, SOLENOID_VALVE_2);
        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

        vTaskDelay(pdMS_TO_TICKS(700)); 
    
        setRelay(dataBytes, PUMP);
        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

        xQueueSend(xQueueInputPulse, (void *) &pulses, portMAX_DELAY);

        do{
            xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, portMAX_DELAY);

            if((ulNotifiedValue & 0x08) >> 3)
                onWait = false;

        }while(onWait);

        resetRelay(dataBytes, PUMP);
        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

        vTaskDelay(pdMS_TO_TICKS(1000));

        resetRelay(dataBytes, SOLENOID_VALVE_2);
        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

        onWait = true;

        if(i != 0){
            do{
                vTaskDelay(pdMS_TO_TICKS(200));

                if(xQueueReceive(xQueueInputTimePerPulse, (void *) &pulseTimeArray[i-1], pdMS_TO_TICKS(10)) == pdPASS){
                    onWait = false;
                    ESP_LOGI(CONTROL_TASK_TAG, "pulseTime -> %lf", pulseTimeArray[i-1]);
                }

            }while(onWait);
        }

        pulses = 100;

        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    double totalTime = pulseTimeArray[0];

    for(size_t i = 1; i < 3; i++)
        totalTime += pulseTimeArray[i];

    totalTime /= 3;

    ESP_LOGW(CONTROL_TASK_TAG, "times: %lf - %lf - %lf = %lf", 
                pulseTimeArray[0], pulseTimeArray[1], pulseTimeArray[2], totalTime);


    ESP_LOGE(CONTROL_TASK_TAG, "Pulse time test: OFF");


}

static void waitMachine2Start(uint8_t *dataBytes){
    checkAirBreak(dataBytes);

}

static void startBoilerTask(){
    xTaskNotify(boilerTaskH, 0x04, eSetBits);         
}

static void waitBoiler2Start(){
    uint32_t ulNotifiedValue = 0;

    bool waiting = true;

    do{
        xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, portMAX_DELAY);

        if((ulNotifiedValue & 0x04) >> 2)
            waiting = false;

    }while(waiting);
}

static void syncronizeAllTasks(){
    uint32_t ulNotifiedValue = 0;
    bool uiT_off = true;            //ui task
    bool inT_off = true;            //interrupts task
    bool boT_off = true;            //boiler task


    do{
        xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, portMAX_DELAY);

        if(ulNotifiedValue & 0x01)
            uiT_off = false;

        if((ulNotifiedValue & 0x02) >> 1)
            inT_off = false;

        if((ulNotifiedValue & 0x04) >> 2)
            boT_off = false;


    }while(uiT_off || inT_off || boT_off);


}

static bool checkWaterBtnFromUi(){
    bool temp = false;

    InputBtnStruct tempInputBtnStruct;

    if(xQueueReceive(xQueueInputBtns, &tempInputBtnStruct, pdMS_TO_TICKS(10)) == pdPASS){

        if(tempInputBtnStruct.btn7)
            temp = true;
    }

    return temp;
}

static bool checkStopBtnFromUi(){

    bool temp = false;

    InputBtnStruct tempInputBtnStruct;

    if(xQueueReceive(xQueueInputBtns, &tempInputBtnStruct, pdMS_TO_TICKS(10)) == pdPASS){

        if(tempInputBtnStruct.btn6)
            temp = true;
    }

    return temp;

}

static void checkQueuesFromUi(ControlData *controlData){

    InputBtnStruct tempInputBtnStruct;

    if(xQueueReceive(xQueueInputBtns, &tempInputBtnStruct, pdMS_TO_TICKS(10)) == pdPASS){

        if(controlData->controlState == IDLE_C){

            if(tempInputBtnStruct.btn0){
                controlData->controlState = DRINK_C;
                if(controlData->page == PAGE_1)
                    controlData->recipeIndex = 0;
                else
                    controlData->recipeIndex = 7;
            }
            else if(tempInputBtnStruct.btn1){
                controlData->controlState = DRINK_C;
                if(controlData->page == PAGE_1)
                    controlData->recipeIndex = 1;
                else
                    controlData->recipeIndex = 8;
            }
            else if(tempInputBtnStruct.btn2){
                controlData->controlState = DRINK_C;
                if(controlData->page == PAGE_1)
                    controlData->recipeIndex = 2;
                else
                    controlData->recipeIndex = 9;
            }
            else if(tempInputBtnStruct.btn3){
                controlData->controlState = DRINK_C;
                if(controlData->page == PAGE_1)
                    controlData->recipeIndex = 3;
                else
                    controlData->recipeIndex = 10;
            }
            else if(tempInputBtnStruct.btn4){
                controlData->controlState = DRINK_C;
                if(controlData->page == PAGE_1)
                    controlData->recipeIndex = 4;
                else
                    controlData->recipeIndex = 11;
            }
            else if(tempInputBtnStruct.btn5){
                controlData->controlState = DRINK_C;
                if(controlData->page == PAGE_1)
                    controlData->recipeIndex = 5;
                else
                    controlData->recipeIndex = 12;
            }
            else if(tempInputBtnStruct.btn6){
                if(controlData->page == PAGE_1){
                    controlData->recipeIndex = 6;
                    controlData->controlState = DRINK_C;
                }
                else
                    controlData->controlState = WATER_C;
                
            }
            else if(tempInputBtnStruct.btn7){
                controlData->controlState = SWAP_PAGE_C;
            }
            else if(tempInputBtnStruct.btnA)
                controlData->controlState = CLEAN_C;
            else if(tempInputBtnStruct.btnB)
                controlData->controlState = MAINTENANCE_C;
            
            /*
            ESP_LOGE(CONTROL_TASK_TAG, "-> %d %d %d %d %d %d %d %d %d %d",
                tempInputBtnStruct.btn0, tempInputBtnStruct.btn1, tempInputBtnStruct.btn2, 
                tempInputBtnStruct.btn3, tempInputBtnStruct.btn4, tempInputBtnStruct.btn5,
                tempInputBtnStruct.btn6, tempInputBtnStruct.btn7, tempInputBtnStruct.btnA, 
                tempInputBtnStruct.btnB);
                */
        }
        

    }
}

static void initMemData(Recipe *recipeData, SystemData *sysData, PowderGramsRefData *powderData){
    nvs_handle_t nvsHandle;
    esp_err_t err;

    uint8_t u8Data = 0;
    uint16_t u16Data = 0;
    uint32_t u32Data = 0;
 
    char *namespaceName = (char *)malloc(10);
    strcpy(namespaceName, "recipe0");

    char *enableKey = (char *)malloc(10);
    strcpy(enableKey, "enable0");

    char *moduleTypeKey = (char *)malloc(13);
    strcpy(moduleTypeKey, "moduleType0");

    char *pulsesKey = (char *)malloc(10);
    strcpy(pulsesKey, "pulses0");

    char *grKey = (char *)malloc(5);
    strcpy(grKey, "gr0");

    char *nameStr = (char *)malloc(17);
    strcpy(nameStr, "recipe_0");        //max 16 chars

    
    for(size_t i = 0; i < 13; i++){                     //13
        if(i < 9){
            namespaceName[6] = (char)(i + 49);
            nameStr[7] = (char)(i + 49);
        }
        else{
            namespaceName[6] = '1';
            namespaceName[7] = (char)(i + 39);
            namespaceName[8] = '\0';

            nameStr[7] = '1';
            nameStr[8] = (char)(i + 39);
            nameStr[9] = '\0';
        }

        err = nvs_open(namespaceName, NVS_READONLY, &nvsHandle);

        if(err == ESP_ERR_NVS_NOT_FOUND){
            err = nvs_open(namespaceName, NVS_READWRITE, &nvsHandle);

            if(err == ESP_OK){

                for(size_t j = 0; j < 5; j++){

                    char charNum = (char)(j + 48);

                    enableKey[6] = charNum;
                    moduleTypeKey[10] = charNum;
                    pulsesKey[6] = charNum;
                    grKey[2] = charNum;

                    nvs_set_u8(nvsHandle, enableKey, 0);
                    nvs_set_u8(nvsHandle, moduleTypeKey, 0);
                    nvs_set_u16(nvsHandle, pulsesKey, 0);
                    nvs_set_u8(nvsHandle, grKey, 0);

                    //ESP_LOGI(CONTROL_TASK_TAG, "%s KEYS: %s %s %s %s %s",
                      //          nameStr, enableKey, moduleTypeKey, preReadyKey, pulsesKey, grKey);

                }

                nvs_set_u16(nvsHandle, "counter", 0);

                if(nvs_commit(nvsHandle) != ESP_OK)
                    ESP_LOGE(CONTROL_TASK_TAG, "NVS Commit error on %s", namespaceName);

                err = nvs_set_str(nvsHandle, "recipeName", nameStr);

                if(err == ESP_OK){
                    err = nvs_commit(nvsHandle);

                    if(err == ESP_OK)
                        ESP_LOGI(CONTROL_TASK_TAG, "NVS %s successfully created!", namespaceName);
                }
                else
                    ESP_LOGE(CONTROL_TASK_TAG, "Error: can't set nvt str!");

                nvs_close(nvsHandle);

                err = nvs_open(namespaceName, NVS_READONLY, &nvsHandle);
            }
        }//END if(err == ESP_ERR_NVS_NOT_FOUND)


        if(err == ESP_OK){
            

            /*

            size_t strLen;

            nvs_get_str(nvsHandle, "recipeName", NULL, &strLen);

            char *tempStr = (char *)malloc(strLen);
            memset(tempStr, 0, strLen);

            err = nvs_get_str(nvsHandle, "recipeName", tempStr, &strLen);

            recipeData[i].recipeName = (char *)malloc(strLen);
            
            if(err == ESP_OK)
                strcpy(recipeData[i].recipeName, tempStr);
            else        
                ESP_LOGE(CONTROL_TASK_TAG, "Error %s: can't get nvt str! %s", namespaceName, esp_err_to_name(err));
            

            free(tempStr);

            nvs_get_u16(nvsHandle, "counter", &u16Data);

            recipeData[i].recipeCounter = u16Data;


            strLen = 0;
            for(size_t j = 0; j < 5; j++){
                char charNum = (char)(j + 48);

                enableKey[6] = charNum;

                nvs_get_u8(nvsHandle, enableKey, &u8Data);

                if(u8Data == 1)
                    strLen++;

            }

            recipeData[i].modulesArray = (RecipeModuleStruct *)malloc(sizeof(RecipeModuleStruct) * strLen);
            recipeData[i].moduleSize = strLen;

            size_t k = 0;
            for(size_t j = 0; j < 5; j++){
                char charNum = (char)(j + 48);

                enableKey[6] = charNum;
                moduleTypeKey[10] = charNum;
                preReadyKey[8] = charNum;
                pulsesKey[6] = charNum;
                grKey[2] = charNum;

                err = nvs_get_u8(nvsHandle, enableKey, &u8Data);

                if(u8Data == 1){

                    nvs_get_u8(nvsHandle, moduleTypeKey, &u8Data);
                    recipeData[i].modulesArray[k].moduleType = (RecipeModuleType)u8Data;

                    nvs_get_u8(nvsHandle, preReadyKey, &u8Data);
                    recipeData[i].modulesArray[k].preReady = (bool)u8Data;

                    nvs_get_u16(nvsHandle, pulsesKey, &u16Data);
                    recipeData[i].modulesArray[k].pulses = u16Data;

                    nvs_get_u8(nvsHandle, grKey, &u8Data);
                    recipeData[i].modulesArray[k++].gr = u8Data;

                }
            }
            */
            
            nvs_close(nvsHandle);
        }
    }

    strcpy(namespaceName, "sysData");

    err = nvs_open(namespaceName, NVS_READONLY, &nvsHandle);

    if(err == ESP_ERR_NVS_NOT_FOUND){
        err = nvs_open(namespaceName, NVS_READWRITE, &nvsHandle);

        if(err == ESP_OK){
            nvs_set_u8(nvsHandle, "boilerTemp", 91);
            nvs_set_u16(nvsHandle, "password", 1234);
            nvs_set_u32(nvsHandle, "mCounter", 0);


            if(nvs_commit(nvsHandle) == ESP_OK)
                ESP_LOGI(CONTROL_TASK_TAG, "NVS %s successfully created!", namespaceName);
            else
                ESP_LOGE(CONTROL_TASK_TAG, "NVS Commit error on %s", namespaceName);

            nvs_close(nvsHandle);

            err = nvs_open(namespaceName, NVS_READONLY, &nvsHandle);
        }
    }

    if(err == ESP_OK){
        nvs_get_u8(nvsHandle, "boilerTemp", &u8Data);
        sysData->boilerTemperature = u8Data;
        
        nvs_get_u16(nvsHandle, "password", &u16Data);
        sysData->password = u16Data;

        nvs_get_u32(nvsHandle, "mConuter", &u32Data);
        sysData->mainCounter = u32Data;

        nvs_close(nvsHandle);
    }

    
    strcpy(namespaceName, "powdData");
    err = nvs_open(namespaceName, NVS_READONLY, &nvsHandle);

    if(err == ESP_ERR_NVS_NOT_FOUND){
        err = nvs_open(namespaceName, NVS_READWRITE, &nvsHandle);

        if(err == ESP_OK){
            nvs_set_u8(nvsHandle, "powderA", 10);
            nvs_set_u8(nvsHandle, "powderB", 11);
            nvs_set_u8(nvsHandle, "powderC", 12);

            if(nvs_commit(nvsHandle) == ESP_OK)
                ESP_LOGI(CONTROL_TASK_TAG, "NVS %s successfully created!", namespaceName);
            else
                ESP_LOGE(CONTROL_TASK_TAG, "NVS Commit error on %s", namespaceName);

            nvs_close(nvsHandle);

            err = nvs_open(namespaceName, NVS_READONLY, &nvsHandle);
        }
    }

    if(err == ESP_OK){
        nvs_get_u8(nvsHandle, "powderA", &u8Data);
        powderData->refGrPowA = u8Data;

        nvs_get_u8(nvsHandle, "powderB", &u8Data);
        powderData->refGrPowB = u8Data;

        nvs_get_u8(nvsHandle, "powderC", &u8Data);
        powderData->refGrPowC = u8Data;

        nvs_close(nvsHandle);
    }

    free(namespaceName);
    free(enableKey);
    free(moduleTypeKey);
    free(pulsesKey);
    free(grKey);
    free(nameStr);
}


static void checkMemContent(const Recipe *recipeData, const SystemData *sysData, PowderGramsRefData *powderData){
    /*
    int aSize = sizeof(Recipe);

    for(size_t i = 0; i < 13; i++)
        ESP_LOGI(CONTROL_TASK_TAG, "-> %s ", recipeData[i].recipeName);

    ESP_LOGI(CONTROL_TASK_TAG, "--> %d - %d - %lu", sysData->boilerTemperature, sysData->password, sysData->mainCounter);
    */

   for(size_t i = 0; i < 13; i++){


        ESP_LOGI(CONTROL_TASK_TAG, "%s (%d):", recipeData[i].recipeName, recipeData[i].moduleSize);

        if(recipeData[i].moduleSize > 0){

            for(size_t j = 0; j < recipeData[i].moduleSize; j++){
                ESP_LOGW(CONTROL_TASK_TAG, "%d: %d - %d - %d", j, recipeData[i].modulesArray[j].moduleType, 
                                                        recipeData[i].modulesArray[j].pulses, recipeData[i].modulesArray[j].gr);
            }
        }

        ESP_LOGI(CONTROL_TASK_TAG, "------------------" );
        
   }


    /*
    ESP_LOGI(CONTROL_TASK_TAG, "0 = %s - %d - %d + %d", 
        recipeData[0].recipeName, recipeData[0].modulesArray[0].moduleType, recipeData[0].modulesArray[0].pulses,
        recipeData[0].moduleSize);

    ESP_LOGI(CONTROL_TASK_TAG, "1 = %s - %d - %d | %d - %d + %d", 
        recipeData[1].recipeName, recipeData[1].modulesArray[0].moduleType, recipeData[1].modulesArray[0].pulses,
        recipeData[1].modulesArray[1].moduleType, recipeData[1].modulesArray[1].pulses,
        recipeData[1].moduleSize);            

    ESP_LOGI(CONTROL_TASK_TAG, "2 = %s - %d - %d - %d + %d", 
        recipeData[2].recipeName, recipeData[2].modulesArray[0].moduleType, recipeData[2].modulesArray[0].pulses,
        recipeData[2].modulesArray[0].gr, recipeData[2].moduleSize);

        */

    ESP_LOGI(CONTROL_TASK_TAG, "pow = %d - %d - %d", powderData->refGrPowA, powderData->refGrPowB, powderData->refGrPowC);
}


static void loadFakeMemContent(Recipe *recipeData, SystemData *sysData, PowderGramsRefData *powderData){

    ESP_LOGI(CONTROL_TASK_TAG, "pow pre = %d - %d - %d", powderData->refGrPowA, powderData->refGrPowB, powderData->refGrPowC);

    recipeData[0].recipeName = (char *)malloc(17);
    memset(recipeData[0].recipeName, 0, 17);
    strcpy(recipeData[0].recipeName, "Dark Coffee");


    recipeData[0].modulesArray = (RecipeModuleStruct *)malloc(sizeof(RecipeModuleStruct) * 1);
    recipeData[0].modulesArray[0].moduleType = COFFEE_M;
    recipeData[0].modulesArray[0].pulses = 154;         //148

    recipeData[0].moduleSize = 1;


    recipeData[1].recipeName = (char *)malloc(17);
    memset(recipeData[1].recipeName, 0, 17);
    strcpy(recipeData[1].recipeName, "Medium Coffee");

    recipeData[1].modulesArray = (RecipeModuleStruct *)malloc(sizeof(RecipeModuleStruct) * 2);
    recipeData[1].modulesArray[0].moduleType = COFFEE_M;
    recipeData[1].modulesArray[0].pulses = 114;             
    recipeData[1].modulesArray[1].moduleType = WATER_C;
    recipeData[1].modulesArray[1].pulses = 10;

    recipeData[1].moduleSize = 2;


    recipeData[2].recipeName = (char *)malloc(17);
    memset(recipeData[2].recipeName, 0, 17);
    strcpy(recipeData[2].recipeName, "Light Coffee");

    recipeData[2].modulesArray = (RecipeModuleStruct *)malloc(sizeof(RecipeModuleStruct) * 2);
    recipeData[2].modulesArray[0].moduleType = COFFEE_M;
    recipeData[2].modulesArray[0].pulses = 78;

    recipeData[2].modulesArray[1].moduleType = WATER_C;
    recipeData[2].modulesArray[1].pulses = 29;

    recipeData[2].moduleSize = 2;


    recipeData[3].recipeName = (char *)malloc(17);
    memset(recipeData[3].recipeName, 0, 17);
    strcpy(recipeData[3].recipeName, "Americano");


    recipeData[3].modulesArray = (RecipeModuleStruct *)malloc(sizeof(RecipeModuleStruct) * 1);
    recipeData[3].modulesArray[0].moduleType = COFFEE_M;
    recipeData[3].modulesArray[0].pulses = 246;         //193

    recipeData[3].moduleSize = 1;


    recipeData[4].recipeName = (char *)malloc(17);
    memset(recipeData[4].recipeName, 0, 17);
    strcpy(recipeData[4].recipeName, "Cafe con Leche");

    recipeData[4].modulesArray = (RecipeModuleStruct *)malloc(sizeof(RecipeModuleStruct) * 2);
    recipeData[4].modulesArray[0].moduleType = COFFEE_M;
    recipeData[4].modulesArray[0].pulses = 122;

    recipeData[4].modulesArray[1].moduleType = POWDER_A_M;
    recipeData[4].modulesArray[1].pulses = 113;
    recipeData[4].modulesArray[1].gr = 28;

    recipeData[4].moduleSize = 2;


    recipeData[5].recipeName = (char *)malloc(17);
    memset(recipeData[5].recipeName, 0, 17);
    strcpy(recipeData[5].recipeName, "Pintadito");

    recipeData[5].modulesArray = (RecipeModuleStruct *)malloc(sizeof(RecipeModuleStruct) * 2);
    recipeData[5].modulesArray[0].moduleType = COFFEE_M;
    recipeData[5].modulesArray[0].pulses = 54;

    recipeData[5].modulesArray[1].moduleType = POWDER_A_M;
    recipeData[5].modulesArray[1].pulses = 182;
    recipeData[5].modulesArray[1].gr = 22;

    recipeData[5].moduleSize = 2;


    for(size_t i = 6; i < 13; i++){
        recipeData[i].recipeName = (char *)malloc(17);
        memset(recipeData[i].recipeName, 0, 17);

        recipeData[i].moduleSize = 0;
    }


    powderData->refGrPowA = 15;
    powderData->refGrPowB = 15;
    powderData->refGrPowC = 15;

}


static void btSubTask(bool *btEnabled){
  
    if(!(*btEnabled)){
        *btEnabled = true;

        initBLUE();
    }

    if(*btEnabled){
        bool runIt = true;

        

        while(runIt){
            checkQueuesFromUi(NULL);
            
        }

    }

}


void initI2C_MCP23017_Out(){

    uint8_t *dataBuff = (uint8_t *)malloc(2);

    memset(dataBuff, 0, 2);

    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x0, dataBuff, 2);                //Set output pins

    vTaskDelay(pdMS_TO_TICKS(20));
 
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBuff, 2);               //Clear all output pins.

    vTaskDelay(pdMS_TO_TICKS(20));

    free(dataBuff);

}

void initI2C_MCP23017_In(){
    uint8_t *dataBuff = malloc(1);

    memset(dataBuff, 0x3f, 1);

    writeBytesMCP2307(MCP23017_INPUT_ADDR, 0x05, dataBuff, 1);              //GPINTENB

    memset(dataBuff, 0x2, 1);

    writeBytesMCP2307(MCP23017_INPUT_ADDR, 0X0b, dataBuff, 1);

    free(dataBuff);

}

void initBLUE(){
    esp_err_t ret;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(CONTROL_TASK_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(CONTROL_TASK_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(CONTROL_TASK_TAG, "%s init bluetooth\n", __func__);
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(CONTROL_TASK_TAG, "%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(CONTROL_TASK_TAG, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_app_register(ESP_SPP_APP_ID);

}


void initControlTask(){
    BaseType_t xReturned = xTaskCreate(controlTask, "control_task", CONTROL_TASK_SIZE, (void *)NULL, CONTROL_TASK_PRIORITY, &controlTaskH);

    if(xReturned == pdFAIL){
        ESP_LOGE(CONTROL_TASK_TAG, "Failed to create task.");

        while(true){

        }
    }
}


static void controlTask(void *pvParameters){

    bool btEnabled = false;
    ControlData controlData = {IDLE_C, 0, PAGE_1};
    //ContsPowderData conPowderData = {1.8333f, 1.8333f, 1.8333f, 80737.37};
    PowderGramsRefData powderGramsRefData = {0, 0, 0};

    recipeList = (Recipe *)malloc(sizeof(Recipe) * 13);
    SystemData systemData;
    UiData uiDataControl = {PAGE_1, 0};

    uiDataControl.strData = (char *)malloc(17);
    memset(uiDataControl.strData, 0, 17);

    uint8_t *outputIO_Buff = (uint8_t *)malloc(2);

    memset(outputIO_Buff, 0, 2);

    recvBuff = malloc(64);
    memset(recvBuff, 0, 64);

    initMemData(recipeList, &systemData, &powderGramsRefData);

    systemData.boilerTemperature = 89;

    xQueueSend(xQueueData2Boiler, (void *) &systemData.boilerTemperature, (TickType_t) 10);

    loadFakeMemContent(recipeList, &systemData, &powderGramsRefData);

    checkMemContent(recipeList, &systemData, &powderGramsRefData);

    initBLUE();

    while(true){

       
       vTaskDelay(pdMS_TO_TICKS(500)); //100
    }



    checkAirBreak(outputIO_Buff);

    xTaskNotify(uiTaskH, 0x020, eSetBits);                  //Set ui task to booting state

    startBoilerTask();

    waitBoiler2Start();

    //clearCoffeeChamber(outputIO_Buff);

    resetBrewer(outputIO_Buff);

    //syncronizeAllTasks();

    xTaskNotify(uiTaskH, 0x10, eSetBits);                   //Set ui task to idle

    ESP_LOGI(CONTROL_TASK_TAG, "ONLINE");


    while(true){

        checkQueuesFromUi(&controlData);

        switch(controlData.controlState){
            case IDLE_C:

            break;
            case MAINTENANCE_C:
                btSubTask(&btEnabled);

                controlData.controlState = IDLE_C;
            break;
            case CLEAN_C:
                xTaskNotify(uiTaskH, 0x40, eSetBits);                   //Set ui task to cleaning state

                cleanMachine(outputIO_Buff);

                xTaskNotify(uiTaskH, 0x10, eSetBits);                   //Set ui task to idle

                controlData.controlState = IDLE_C;
            break;
            case DRINK_C:
                strcpy(uiDataControl.strData, recipeList[controlData.recipeIndex].recipeName);
                xQueueSend(xQueueUI, (void *) &uiDataControl, (TickType_t) 10);

                xTaskNotify(uiTaskH, 0x100, eSetBits);                   //Set ui task to preparing drink state

                runDrink(&recipeList[controlData.recipeIndex], outputIO_Buff, &powderGramsRefData);

                xTaskNotify(uiTaskH, 0x10, eSetBits);                   //Set ui task to idle
                
                controlData.controlState = IDLE_C;
            break;
            case WATER_C:
                strcpy(uiDataControl.strData, " Water");
                xQueueSend(xQueueUI, (void *) &uiDataControl, (TickType_t) 10);

                xTaskNotify(uiTaskH, 0x100, eSetBits);                   //Set ui task to preparing drink state

                injectOnlyWaterLineManual(outputIO_Buff);

                xTaskNotify(uiTaskH, 0x10, eSetBits);                   //Set ui task to idle

                controlData.controlState = IDLE_C;
            break;
            case SWAP_PAGE_C:
                if(controlData.page == PAGE_1)
                    controlData.page = PAGE_2;
                else
                    controlData.page = PAGE_1;

                uiDataControl.page = controlData.page;
                
                xQueueSend(xQueueUI, (void *) &uiDataControl, (TickType_t) 10);

                controlData.controlState = IDLE_C;
            break;
        }

        checkAirBreak(outputIO_Buff);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    
    

}
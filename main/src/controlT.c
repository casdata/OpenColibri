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

bool sendNextData;
SyncroState syncroState;
char *recvBuff;
char *commandBuff;
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

static void addRecipeIndex2CommandBuff(uint8_t recipeIndex){

    char *tempData = (char *)malloc(8);

    sprintf(tempData, "%d", recipeIndex);

    if(recipeIndex < 10)
        strcat(commandBuff,"0");

    strcat(commandBuff, tempData);


    free(tempData);

}

static void addU8Value2CommandBuff(uint8_t u8Value){

    char *tempData = (char *)malloc(6);

    memset(tempData, 0, 6);

    sprintf(tempData, "%d", u8Value);

    strcat(commandBuff, tempData);

    free(tempData);

}

static void addU16Value2CommandBuff(uint16_t u16Value){

    char *tempData = (char *)malloc(16);

    memset(tempData, 0, 16);

    sprintf(tempData, "%d", u16Value);

    strcat(commandBuff, tempData);

    free(tempData);

}

static void sendAllRecipes(Recipe *recipe){
    static uint8_t tempPC = 0;
    static uint8_t recipeIndex = 0;
    static bool createCommand = true;


    if(sendNextData){
        sendNextData = false;
    
        if(createCommand){
            createCommand = false;

            memset(commandBuff, 0, 64);

            strcat(commandBuff, "#");

            size_t modSize = recipe[recipeIndex].moduleSize;

            if(modSize > 0){
                strcat(commandBuff, "d");
                addRecipeIndex2CommandBuff(recipeIndex);

                strcat(commandBuff, recipe[recipeIndex].recipeName);

                strcat(commandBuff, "|o");

                addU16Value2CommandBuff(recipe[recipeIndex].recipeCounter);

                strcat(commandBuff, "|");

                for(size_t i = 0; i < modSize; i++){
                    switch(recipe[recipeIndex].modulesArray[i].moduleType){
                        case COFFEE_M:
                            strcat(commandBuff, "C");
                            addU16Value2CommandBuff(recipe[recipeIndex].modulesArray[i].pulses);
                        break;
                        case COFFEE_PRE_M:
                            strcat(commandBuff, "r");
                            addU16Value2CommandBuff(recipe[recipeIndex].modulesArray[i].pulses);
                        break;
                        case POWDER_A_M:
                            strcat(commandBuff, "a");
                            addU16Value2CommandBuff(recipe[recipeIndex].modulesArray[i].pulses);
                            strcat(commandBuff, ",");
                            addU8Value2CommandBuff(recipe[recipeIndex].modulesArray[i].gr);
                        break;
                        case POWDER_B_M:
                            strcat(commandBuff, "b");
                            addU16Value2CommandBuff(recipe[recipeIndex].modulesArray[i].pulses);
                            strcat(commandBuff, ",");
                            addU8Value2CommandBuff(recipe[recipeIndex].modulesArray[i].gr);
                        break;
                        case POWDER_C_M:
                            strcat(commandBuff, "c");
                            addU16Value2CommandBuff(recipe[recipeIndex].modulesArray[i].pulses);
                            strcat(commandBuff, ",");
                            addU8Value2CommandBuff(recipe[recipeIndex].modulesArray[i].gr);
                        break;
                        case WATER_M:
                            strcat(commandBuff, "w");
                            addU16Value2CommandBuff(recipe[recipeIndex].modulesArray[i].pulses);
                        break;
                    }
                    strcat(commandBuff, "|");
                }
            }
            else{
                strcat(commandBuff, "D");
                addRecipeIndex2CommandBuff(recipeIndex);

                strcat(commandBuff, "|");
            }

            strcat(commandBuff, "~");

            //ESP_LOGI(CONTROL_TASK_TAG, "command[%d]: %s", recipeIndex, commandBuff);

            recipeIndex++;

            tempPC = 0;

        }


        char *innerBuffer = (char *)malloc(24); 
        memset(innerBuffer, 0, 24);

        size_t countTo20 = 0;

        do{
            innerBuffer[countTo20++] = commandBuff[tempPC++];
        } while(countTo20 < 20 && (tempPC < strlen(commandBuff)));

        innerBuffer[countTo20] = '\0';

        esp_ble_gatts_set_attr_value(spp_handle_table[SPP_IDX_SPP_DATA_NTY_VAL], strlen(innerBuffer), (uint8_t *)innerBuffer);

        //ESP_LOGI(CONTROL_TASK_TAG, "sending recp[%d]: %s", tempPC, innerBuffer);

        if(tempPC >= strlen(commandBuff)){ 

            if(recipeIndex > 12){
                sendNextData = false;
                syncroState = POST_SEND_RECIPES_C;

            }

            createCommand = true;
        }        

        free(innerBuffer);
    

    }


}


static void processBlueData(uint8_t hIndex, uint8_t tIndex){

    //checkOnlyRecipeMemContent(recipeList);

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

    nvs_handle_t nvsHandle;
    esp_err_t err;


    uint8_t tempU8 = 0;
    uint16_t tempU16 = 0;

    char *tempStr = (char *)malloc(17);
    memset(tempStr, 0, 17);

    hIndex += 2;

    tempStr[0] = recvBuff[hIndex++];
    tempStr[1] = recvBuff[hIndex++];
    tempStr[2] = '\0';

    uint8_t rSlot = atoi(tempStr);

    memset(tempStr, 0, 17);

    if(rSlot < 9)
        namespaceName[6] = (char)(rSlot + 49);
    else{
        namespaceName[6] = '1';
        namespaceName[7] = (char)(rSlot + 39);
        namespaceName[8] = '\0';
    }


    err = nvs_open(namespaceName, NVS_READWRITE, &nvsHandle);
    bool nvsOk = false;

    if(err == ESP_OK)
        nvsOk = true;
    

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

    if(nvsOk){
        err = nvs_set_str(nvsHandle, "recipeName", tempStr);
        if(err == ESP_OK)
            nvs_commit(nvsHandle);
    }
  
    memset(tempStr, 0, 17);


    for(size_t i = hIndex; i < tIndex; i++){
        if(recvBuff[i] == '|')
            tempU8++;
    }

    if(recipeList[rSlot].moduleSize != 0)
        free(recipeList[rSlot].modulesArray);


    recipeList[rSlot].moduleSize = tempU8;

    //if(recipeList[rSlot].modulesArray != NULL)    
        //free(recipeList[rSlot].modulesArray);

    recipeList[rSlot].modulesArray = (RecipeModuleStruct *)malloc(sizeof(RecipeModuleStruct) * tempU8);
    recipeList[rSlot].recipeCounter = 0;


    size_t j = hIndex;

    size_t tempModuleNum = 0;
    size_t strPC = 0;

    size_t subModulePC = 0;
    char charNum;

    if(nvsOk){
        for(size_t k = 0; k < 5; k++){
            charNum = (char)(k + 48);

            enableKey[6] = charNum;
            moduleTypeKey[10] = charNum;
            pulsesKey[6] = charNum;
            grKey[2] = charNum;

            nvs_set_u8(nvsHandle, enableKey, 0);
            nvs_set_u8(nvsHandle, moduleTypeKey, 0);
            nvs_set_u16(nvsHandle, pulsesKey, 0);
            nvs_set_u8(nvsHandle, grKey, 0);
        }

        nvs_commit(nvsHandle);
    }

    do{

        switch(recvBuff[j++]){
            case 'C':                       //coffee
                charNum = (char)((subModulePC++) + 48);

                enableKey[6]= charNum;
                moduleTypeKey[10] = charNum;
                pulsesKey[6] = charNum;
                grKey[2] = charNum;

                strPC = 0;
                memset(tempStr, 0, 17);

                while(recvBuff[j] != '|')
                    tempStr[strPC++] = recvBuff[j++];

                j++;

                tempStr[strPC++] = '\0';

                tempU16 = atoi(tempStr);
            
                recipeList[rSlot].modulesArray[tempModuleNum].moduleType = COFFEE_M;
                recipeList[rSlot].modulesArray[tempModuleNum].pulses = tempU16;

                if(nvsOk){
                    nvs_set_u8(nvsHandle, enableKey, 1);
                    nvs_set_u8(nvsHandle, moduleTypeKey, 0);
                    nvs_set_u16(nvsHandle, pulsesKey, tempU16);
                }

                tempModuleNum++;
            break;
            case 'r':                       //pre coffee

                charNum = (char)((subModulePC++) + 48);

                enableKey[6]= charNum;
                moduleTypeKey[10] = charNum;
                pulsesKey[6] = charNum;
                grKey[2] = charNum;

                strPC = 0;
                memset(tempStr, 0, 17);

                while(recvBuff[j] != '|')
                    tempStr[strPC++] = recvBuff[j++];

                j++;

                tempStr[strPC++] = '\0';

                tempU16 = atoi(tempStr);
            
                recipeList[rSlot].modulesArray[tempModuleNum].moduleType = COFFEE_PRE_M;
                recipeList[rSlot].modulesArray[tempModuleNum].pulses = tempU16;

                if(nvsOk){
                    nvs_set_u8(nvsHandle, enableKey, 1);
                    nvs_set_u8(nvsHandle, moduleTypeKey, 1);
                    nvs_set_u16(nvsHandle, pulsesKey, tempU16);
                }

                tempModuleNum++;
            break;
            case 'a':                       //powder a
                charNum = (char)((subModulePC++) + 48);

                enableKey[6]= charNum;
                moduleTypeKey[10] = charNum;
                pulsesKey[6] = charNum;
                grKey[2] = charNum;

                strPC = 0;
                memset(tempStr, 0, 17);

                while(recvBuff[j] != ',')
                    tempStr[strPC++] = recvBuff[j++];

                j++;

                tempStr[strPC++] = '\0';

                tempU16 = atoi(tempStr);
        

                strPC = 0;
                memset(tempStr, 0, 17);

                while(recvBuff[j] != '|')
                    tempStr[strPC++] = recvBuff[j++];
                
                j++;

                tempStr[strPC++] = '\0';

                tempU8 = atoi(tempStr);

                recipeList[rSlot].modulesArray[tempModuleNum].moduleType = POWDER_A_M;
                recipeList[rSlot].modulesArray[tempModuleNum].pulses = tempU16;
                recipeList[rSlot].modulesArray[tempModuleNum].gr = tempU8;

                if(nvsOk){
                    nvs_set_u8(nvsHandle, enableKey, 1);
                    nvs_set_u8(nvsHandle, moduleTypeKey, 2);
                    nvs_set_u16(nvsHandle, pulsesKey, tempU16);
                    nvs_set_u8(nvsHandle, grKey, tempU8);
                }


                tempModuleNum++;
            break;
            case 'b':                       //powder b
                charNum = (char)((subModulePC++) + 48);

                enableKey[6]= charNum;
                moduleTypeKey[10] = charNum;
                pulsesKey[6] = charNum;
                grKey[2] = charNum;

                strPC = 0;
                memset(tempStr, 0, 17);

                while(recvBuff[j] != ',')
                    tempStr[strPC++] = recvBuff[j++];

                j++;

                tempStr[strPC++] = '\0';

                tempU16 = atoi(tempStr);
        

                strPC = 0;
                memset(tempStr, 0, 17);

                while(recvBuff[j] != '|')
                    tempStr[strPC++] = recvBuff[j++];
                
                j++;

                tempStr[strPC++] = '\0';

                tempU8 = atoi(tempStr);

                recipeList[rSlot].modulesArray[tempModuleNum].moduleType = POWDER_B_M;
                recipeList[rSlot].modulesArray[tempModuleNum].pulses = tempU16;
                recipeList[rSlot].modulesArray[tempModuleNum].gr = tempU8;

                if(nvsOk){
                    nvs_set_u8(nvsHandle, enableKey, 1);
                    nvs_set_u8(nvsHandle, moduleTypeKey, 3);
                    nvs_set_u16(nvsHandle, pulsesKey, tempU16);
                    nvs_set_u8(nvsHandle, grKey, tempU8);
                }

                tempModuleNum++;
            break;
            case 'c':                       //powder c
                charNum = (char)((subModulePC++) + 48);

                enableKey[6]= charNum;
                moduleTypeKey[10] = charNum;
                pulsesKey[6] = charNum;
                grKey[2] = charNum;

                strPC = 0;
                memset(tempStr, 0, 17);

                while(recvBuff[j] != ',')
                    tempStr[strPC++] = recvBuff[j++];

                j++;

                tempStr[strPC++] = '\0';

                tempU16 = atoi(tempStr);
        

                strPC = 0;
                memset(tempStr, 0, 17);

                while(recvBuff[j] != '|')
                    tempStr[strPC++] = recvBuff[j++];
                
                j++;

                tempStr[strPC++] = '\0';

                tempU8 = atoi(tempStr);

                recipeList[rSlot].modulesArray[tempModuleNum].moduleType = POWDER_C_M;
                recipeList[rSlot].modulesArray[tempModuleNum].pulses = tempU16;
                recipeList[rSlot].modulesArray[tempModuleNum].gr = tempU8;

                if(nvsOk){
                    nvs_set_u8(nvsHandle, enableKey, 1);
                    nvs_set_u8(nvsHandle, moduleTypeKey, 4);
                    nvs_set_u16(nvsHandle, pulsesKey, tempU16);
                    nvs_set_u8(nvsHandle, grKey, tempU8);
                }


                tempModuleNum++;
            break;
            case 'w':                       //water
                charNum = (char)((subModulePC++) + 48);

                enableKey[6]= charNum;
                moduleTypeKey[10] = charNum;
                pulsesKey[6] = charNum;
                grKey[2] = charNum;

                strPC = 0;
                memset(tempStr, 0, 17);

                while(recvBuff[j] != '|')
                    tempStr[strPC++] = recvBuff[j++];

                j++;

                tempStr[strPC++] = '\0';

                tempU16 = atoi(tempStr);
            
                recipeList[rSlot].modulesArray[tempModuleNum].moduleType = WATER_M;
                recipeList[rSlot].modulesArray[tempModuleNum].pulses = tempU16;

                if(nvsOk){
                    nvs_set_u8(nvsHandle, enableKey, 1);
                    nvs_set_u8(nvsHandle, moduleTypeKey, 5);
                    nvs_set_u16(nvsHandle, pulsesKey, tempU16);
                }

                tempModuleNum++;
            break;
        }

    }while(j < tIndex);

    if(nvsOk){
        nvs_commit(nvsHandle);

        nvs_close(nvsHandle);
    }

    //checkOnlyRecipeMemContent(recipeList);

    
    free(tempStr);
    free(namespaceName);
    free(enableKey);
    free(moduleTypeKey);
    free(pulsesKey);
    free(grKey);
}

static void processNullData(uint8_t hIndex, uint8_t tIndex){

    //checkOnlyRecipeMemContent(recipeList);

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

    nvs_handle_t nvsHandle;
    esp_err_t err;


    uint8_t tempU8 = 0;
    uint16_t tempU16 = 0;

    char *tempStr = (char *)malloc(17);
    memset(tempStr, 0, 17);

    hIndex += 2;

    tempStr[0] = recvBuff[hIndex++];
    tempStr[1] = recvBuff[hIndex++];
    tempStr[2] = '\0';

    uint8_t rSlot = atoi(tempStr);

    memset(tempStr, 0, 17);

    if(rSlot < 9)
        namespaceName[6] = (char)(rSlot + 49);
    else{
        namespaceName[6] = '1';
        namespaceName[7] = (char)(rSlot + 39);
        namespaceName[8] = '\0';
    }


    err = nvs_open(namespaceName, NVS_READWRITE, &nvsHandle);
    bool nvsOk = false;

    if(err == ESP_OK)
        nvsOk = true;


    strcpy(recipeList[rSlot].recipeName, "none");
    recipeList[rSlot].moduleSize = 0;
    recipeList[rSlot].recipeCounter = 0;

    //if(recipeList[rSlot].modulesArray != NULL)    
        //free(recipeList[rSlot].modulesArray);


    if(nvsOk){
        char charNum;

        for(size_t k = 0; k < 5; k++){
            charNum = (char)(k + 48);

            enableKey[6] = charNum;
            moduleTypeKey[10] = charNum;
            pulsesKey[6] = charNum;
            grKey[2] = charNum;

            nvs_set_u8(nvsHandle, enableKey, 0);
            nvs_set_u8(nvsHandle, moduleTypeKey, 0);
            nvs_set_u16(nvsHandle, pulsesKey, 0);
            nvs_set_u8(nvsHandle, grKey, 0);
            nvs_set_u16(nvsHandle, "counter", 0);
        }

        nvs_commit(nvsHandle);

        err = nvs_set_str(nvsHandle, "recipeName", "none");
        if(err == ESP_OK)
            nvs_commit(nvsHandle);
    }

    
    free(tempStr);
    free(namespaceName);
    free(enableKey);
    free(moduleTypeKey);
    free(pulsesKey);
    free(grKey);

    //checkOnlyRecipeMemContent(recipeList);

}

static void processTempData(uint8_t hIndex, uint8_t tIndex){

    //sysData
    //nvs_set_u8(nvsHandle, "boilerTemp", 89);
    //xQueueSend(xQueueData2Boiler, (void *) &systemData.boilerTemperature, (TickType_t) 10);

    char *namespaceName = (char *)malloc(10);
    strcpy(namespaceName, "sysData");

    nvs_handle_t nvsHandle;
    esp_err_t err;
    bool nvsOk = false;

    err = nvs_open(namespaceName, NVS_READWRITE, &nvsHandle);
    if(err == ESP_OK)
        nvsOk = true;


    char *tempStr = (char *)malloc(10);

    memset(tempStr, 0, 10);

    size_t strPC = 0;
    hIndex += 2;

    while(recvBuff[hIndex] != '|')
        tempStr[strPC++] = recvBuff[hIndex++];


    tempStr[strPC++] = '\0';

    uint8_t tempU8 = atoi(tempStr);

    //ESP_LOGW(CONTROL_TASK_TAG, "NEW temp: %d", tempU8);

    xQueueSend(xQueueData2Boiler, (void *) &tempU8, (TickType_t) 10);

    if(nvsOk){
        nvs_set_u8(nvsHandle, "boilerTemp", tempU8);
        nvs_commit(nvsHandle);
        nvs_close(nvsHandle);
    }


    free(tempStr);
    free(namespaceName);

}


static bool append2Command(uint8_t *dataFromBlue, uint16_t dataSize){
    bool commandFound = false;
    static int appendPC = 0;


    for(size_t i = 0; i < dataSize; i++)
        *(recvBuff + (appendPC++)) = *(dataFromBlue + i);
        

    int8_t headIndex = -1;
    int8_t tailIndex = -1;

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

        //ESP_LOGW(CONTROL_TASK_TAG, "commandFound %s", recvBuff);

        switch(recvBuff[headIndex + 1]){
            case 'n':                                               //get next data
                //ESP_LOGW(CONTROL_TASK_TAG, "n command");
                switch(syncroState){
                    case SEND_RECIPES_C:
                        sendNextData = true;
                    break;
                    case POST_SEND_RECIPES_C:
                        syncroState = SEND_TEMPERATURE_C;
                    break;
                    case POST_TEMPERATURE_C:
                        syncroState = END_SYNC_C;
                    break;
                    default:
                }

            break;
            case 'z':                                               //get all data
                //ESP_LOGW(CONTROL_TASK_TAG, "z command");
                syncroState = SEND_RECIPES_C;
                sendNextData = true;
            break;
            case 'd':                                               //data recipe
                //ESP_LOGW(CONTROL_TASK_TAG, "d command");
                processBlueData(headIndex, tailIndex);
                xTaskNotify(uiTaskH, 0x800, eSetBits);
            break;
            case 'D':                                               //data null recipe
                //ESP_LOGW(CONTROL_TASK_TAG, "D command");
                processNullData(headIndex, tailIndex);
                xTaskNotify(uiTaskH, 0x800, eSetBits);
            break;
            case 't':
                //ESP_LOGW(CONTROL_TASK_TAG, "t command");
                processTempData(headIndex, tailIndex);
            break;
        }

        appendPC = 0;

        memset(recvBuff, 0, 64);
        //*(recvBuff + (tailIndex + 1)) = '\0';
    }
    


    return commandFound;
}

static void sendTempData(uint8_t bTemperature){
    
    memset(commandBuff, 0, 64);

    strcat(commandBuff, "#t");

    addU8Value2CommandBuff(bTemperature);

    strcat(commandBuff, "|~");


    esp_ble_gatts_set_attr_value(spp_handle_table[SPP_IDX_SPP_DATA_NTY_VAL], strlen(commandBuff), (uint8_t *)commandBuff);

    syncroState = POST_TEMPERATURE_C;
}

static void closeSyncroData(){
    memset(commandBuff, 0, 64);
    strcpy(commandBuff, "#f~");

    esp_ble_gatts_set_attr_value(spp_handle_table[SPP_IDX_SPP_DATA_NTY_VAL], strlen(commandBuff), (uint8_t *)commandBuff);

    syncroState = IDLE_SYNC_C;
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

                }
                else if(res == SPP_IDX_SPP_DATA_RECV_VAL){

                    char *tempBuffer = (char *)malloc(18);

                    memset(tempBuffer, 0, 18);
                    

                    if(!append2Command(p_data->write.value, p_data->write.len)){
                        strcpy(tempBuffer, "#n~");
                        esp_ble_gatts_set_attr_value(spp_handle_table[SPP_IDX_SPP_DATA_NTY_VAL], strlen(tempBuffer), (uint8_t *)tempBuffer);
                    }                

                    free(tempBuffer);
                    

                }else{
                    //TODO:
                }
            }else if((p_data->write.is_prep == true)&&(res == SPP_IDX_SPP_DATA_RECV_VAL)){

                
            }
      	 	break;
    	}
    	case ESP_GATTS_EXEC_WRITE_EVT:{
    	    if(p_data->exec_write.exec_write_flag){
                            
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


bool runDrink(const Recipe *myRecipe, uint8_t *dataBytes, PowderGramsRefData *powderData, uint8_t *boilerPulse){
    
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
                        runIt = grindAndDeliver(dataBytes, false);

                        vTaskDelay(pdMS_TO_TICKS(1000));

                        if(!runIt)
                            break;

                        setBrewer2InjectPosition(dataBytes);

                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }

                    if(*boilerPulse == 1){
                        *boilerPulse = 0;
                        doBoilerPulse(dataBytes);
                    }

                    injecBrewerWater(dataBytes, myRecipe->modulesArray[i].pulses);

                    vTaskDelay(pdMS_TO_TICKS(2500));

                    setBrewer2StartPosition(dataBytes);
                break;
                case COFFEE_PRE_M:
                    vTaskDelay(pdMS_TO_TICKS(2000));

                    if(*boilerPulse == 1){
                        *boilerPulse = 0;
                        doBoilerPulse(dataBytes);
                    }

                    injecBrewerWater(dataBytes, myRecipe->modulesArray[i].pulses);

                    vTaskDelay(pdMS_TO_TICKS(2500));

                    setBrewer2StartPosition(dataBytes);
                break;
                case POWDER_A_M:
                    injectPowderPlusWater(dataBytes, myRecipe->modulesArray[i].pulses, myRecipe->modulesArray[i].gr, powderData->refGrPowA, DOSER_DEVICE_1, false, boilerPulse);
                break;
                case POWDER_B_M:
                    injectPowderPlusWater(dataBytes, myRecipe->modulesArray[i].pulses, myRecipe->modulesArray[i].gr, powderData->refGrPowB, DOSER_DEVICE_2, false, boilerPulse);
                break;
                case POWDER_C_M:
                    if(!NEXTION_LCD)
                        injectPowderPlusWater(dataBytes, myRecipe->modulesArray[i].pulses, myRecipe->modulesArray[i].gr, powderData->refGrPowB, DOSER_DEVICE_2, true, boilerPulse);
                break;
                case WATER_M:
                    injectOnlyWaterLine(dataBytes, myRecipe->modulesArray[i].pulses, boilerPulse);
                break;
            }

            if(!runIt)
                break;

            vTaskDelay(pdMS_TO_TICKS(1000));   

        }
    }

    return runIt;

}


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
            //ESP_LOGW(CONTROL_TASK_TAG, "OPEN WATER_INLET_VALVE");

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

                    //ESP_LOGW(CONTROL_TASK_TAG, "CLOSE WATER_INLET_VALVE");

                    onWait = false;
                }

            }while(onWait);

            //ESP_LOGI(CONTROL_TASK_TAG, "END AIRBREAK CHECK");
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

    //ESP_LOGI(CONTROL_TASK_TAG, "Coffee released :)");

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

    //ESP_LOGI(CONTROL_TASK_TAG, "Inject brewer water: ON");

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
            //ESP_LOGI(CONTROL_TASK_TAG, "TURNING OFF PUMP %lf", endTime);
        }

    }while(onWait);


    resetRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(1000));

    resetRelay(dataBytes, THREE_WAY_VALVE);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    //ESP_LOGE(CONTROL_TASK_TAG, "Inject brewer water: OFF");

    xTaskNotify(boilerTaskH, 0x08, eSetBits);
}

static void injectPowderPlusWater(uint8_t *dataBytes, const uint16_t pulses, uint8_t gr, uint8_t powderRefGram, const OutputRelays outputRelay, bool thirdPowder, uint8_t *boilerPulse){

    const static double secLimit = 6000000;
    uint32_t ulNotifiedValue = 0;
    bool onWait = true; 
    bool injectPowder = true;


    double rTime = 0;
    double cTime = 0;

    double off_rTime = 0;
    double off_cTime = 0;

    //16000000 16 seg * 1000000

    //10 seg = 15 gr
    //12 seg = 18 gr
    double powderTime = ((double)((float)gr * 10.0f)/((float)powderRefGram)) * 1000000;  //error


    //hot water
    //100 pulses = 8.13 sec
    // n pulses  = x
    double pulseTime = (double)((float)pulses * 8.13f)/((float)100) * 1000000;
    bool noExtraTime = true;

    bool divideProcess = false;

    uint16_t itInt = 0;
    double offTimeValue = 0;

    uint16_t normalPulses = 0;
    uint16_t lastPulses = 0;

    
    if(powderTime > pulseTime){
        double extraTime = powderTime - pulseTime;
        noExtraTime = false;

        if(extraTime > secLimit){                 //more than 6 sec
            divideProcess = true;

            float it = (float)(powderTime / secLimit);

            /*
            ESP_LOGI(CONTROL_TASK_TAG, "before: %f | %lf - %lf |", it, powderTime, 
                                            secLimit);
                                            */

            itInt = (int)it;

            /*
            float pointItValue = it - (float)itInt;

            if(pointItValue > 0.5f){
                itInt++;

            }
            else{

            }
            */
            if(it > (float)itInt)
                itInt++;

            offTimeValue = secLimit - (pulseTime / itInt);      //2297500   | 3702500

            normalPulses = (uint16_t)(pulses / itInt); 

            uint16_t tempPulses = normalPulses * itInt;
            lastPulses = (uint16_t)(pulses - tempPulses);
            lastPulses += normalPulses;

            ESP_LOGI(CONTROL_TASK_TAG, "divide: %f - %d | %lf | %d - %d", it, itInt, 
                                            offTimeValue, normalPulses, lastPulses);

        }
        else{

            if(thirdPowder)
                gpio_set_level(POWDER_C_PIN, 1);
            else{
                setRelay(dataBytes, outputRelay);
                writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 
            }

            vTaskDelay(pdMS_TO_TICKS(400)); 

            rTime = esp_timer_get_time();


            do{
                vTaskDelay(pdMS_TO_TICKS(100));

                cTime = esp_timer_get_time() - rTime;

                if(cTime >= extraTime)
                    onWait = false;
            
            }while(onWait);


            if(thirdPowder)
                gpio_set_level(POWDER_C_PIN, 0);
            else{
                resetRelay(dataBytes, outputRelay);
                writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 
            }

            vTaskDelay(pdMS_TO_TICKS(400));
        }
    }

    if(*boilerPulse == 1){
        *boilerPulse = 0;
        doBoilerPulse(dataBytes);
    }

    if(divideProcess){
        
        setRelay(dataBytes, SOLENOID_VALVE_2);
        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

        vTaskDelay(pdMS_TO_TICKS(200));

        setRelay(dataBytes, WHIPPER);
        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);

        vTaskDelay(pdMS_TO_TICKS(200));        

        if(thirdPowder)
            gpio_set_level(POWDER_C_PIN, 1);
        else{
            setRelay(dataBytes, outputRelay);
            writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 
        }

        vTaskDelay(pdMS_TO_TICKS(200));

        rTime = esp_timer_get_time();
        off_rTime = rTime;

        size_t counts = 0;

        do{
            vTaskDelay(pdMS_TO_TICKS(100));

            off_cTime = esp_timer_get_time() - off_rTime;

            if(off_cTime >= offTimeValue){

                if(counts < (itInt - 1))
                    xQueueSend(xQueueInputPulse, (void *) &normalPulses, portMAX_DELAY);
                else
                    xQueueSend(xQueueInputPulse, (void *) &lastPulses, portMAX_DELAY);

                xTaskNotify(boilerTaskH, 0x20, eSetBits);               //max hot

                setRelay(dataBytes, PUMP);
                writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

                vTaskDelay(pdMS_TO_TICKS(400));

                onWait = true;

                ESP_LOGE(CONTROL_TASK_TAG, "nLoop %d", counts);
                
                do{
                    xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, pdMS_TO_TICKS(200));

                    if((ulNotifiedValue & 0x08) >> 3){
                        onWait = false;

                        //ESP_LOGI(CONTROL_TASK_TAG, "TURNING OFF PUMP");
                    }

                    if(injectPowder && onWait){
                        cTime = esp_timer_get_time() - rTime;

                        if(cTime >= powderTime){
                            injectPowder = false;
        
                        //ESP_LOGI(CONTROL_TASK_TAG, "TURNING DOSER OFF");

                            if(thirdPowder)
                                gpio_set_level(POWDER_C_PIN, 0);
                            else{
                                resetRelay(dataBytes, outputRelay);
                                writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 
                                vTaskDelay(pdMS_TO_TICKS(200));
                            }
                        }
                    }
                    

                }while(onWait);

                ESP_LOGE(CONTROL_TASK_TAG, "end %d", counts);

                if(counts < (itInt - 1))
                    xTaskNotify(boilerTaskH, 0x10, eSetBits);                   //hot

                resetRelay(dataBytes, PUMP);
                writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

                vTaskDelay(pdMS_TO_TICKS(200));

                counts++;
                off_rTime = esp_timer_get_time();
            }
        
            
        }while(counts < itInt);

        if(injectPowder){
        
            if(thirdPowder)
                gpio_set_level(POWDER_C_PIN, 0);
            else{
                resetRelay(dataBytes, outputRelay);
                writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 
            }
        
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        

    }
    else{
        onWait = true;
        cTime = 0;
        rTime = 0;

        xTaskNotify(boilerTaskH, 0x20, eSetBits);

        setRelay(dataBytes, SOLENOID_VALVE_2);
        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

        vTaskDelay(pdMS_TO_TICKS(700));

        setRelay(dataBytes, PUMP);
        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

        vTaskDelay(pdMS_TO_TICKS(500));

        setRelay(dataBytes, WHIPPER);
        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);


        xQueueSend(xQueueInputPulse, (void *) &pulses, portMAX_DELAY);

        vTaskDelay(pdMS_TO_TICKS(400));

        
        if(thirdPowder)
            gpio_set_level(POWDER_C_PIN, 1);
        else{
            setRelay(dataBytes, outputRelay);
            writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 
        }


        vTaskDelay(pdMS_TO_TICKS(400)); 

        rTime = esp_timer_get_time();
    
        do{
            xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, pdMS_TO_TICKS(200));

            if((ulNotifiedValue & 0x08) >> 3){
                onWait = false;

                //ESP_LOGI(CONTROL_TASK_TAG, "TURNING OFF PUMP");
            }

            if(noExtraTime && injectPowder && onWait){
                cTime = esp_timer_get_time() - rTime;

                if(cTime >= powderTime){
                    injectPowder = false;
        
                    //ESP_LOGI(CONTROL_TASK_TAG, "TURNING DOSER OFF");

                    if(thirdPowder)
                        gpio_set_level(POWDER_C_PIN, 0);
                    else{
                        resetRelay(dataBytes, outputRelay);
                        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 
                    }
                
                }
            }

        }while(onWait);
    

        cTime = esp_timer_get_time() - rTime;
    
        if(injectPowder){
        
            if(thirdPowder)
                gpio_set_level(POWDER_C_PIN, 0);
            else{
                resetRelay(dataBytes, outputRelay);
                writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 
            }
        

            vTaskDelay(pdMS_TO_TICKS(400));
        }
        else
            vTaskDelay(pdMS_TO_TICKS(200));

    
        resetRelay(dataBytes, PUMP);
        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

        vTaskDelay(pdMS_TO_TICKS(700));

    }

    
    resetRelay(dataBytes, SOLENOID_VALVE_2);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(4000));

    resetRelay(dataBytes, WHIPPER);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  

    //ESP_LOGE(CONTROL_TASK_TAG, "Inject powder water: OFF - %lf - %lf| puls: %d | gr: %d", powderTime, pulseTime, pulses, gr);

    xTaskNotify(boilerTaskH, 0x08, eSetBits);
    
    
}


static void injectPowderPlusWaterBackUp(uint8_t *dataBytes, const uint16_t pulses, uint8_t gr, uint8_t powderRefGram, const OutputRelays outputRelay, bool thirdPowder, uint8_t *boilerPulse){

    uint32_t ulNotifiedValue = 0;
    bool onWait = true; 
    bool injectPowder = true;


    double rTime = 0;
    double cTime = 0;

    //16000000 16 seg * 1000000

    //10 seg = 15 gr
    //12 seg = 18 gr
    double powderTime = ((double)((float)gr * 10.0f)/((float)powderRefGram)) * 1000000;  //error


    //hot water
    //100 pulses = 8.13 sec
    // n pulses  = x
    double pulseTime = (double)((float)pulses * 8.13f)/((float)100) * 1000000;

    bool noExtraTime = true;

    if(powderTime > pulseTime){

        noExtraTime = false;

        double extraTime = powderTime - pulseTime;
        
        if(thirdPowder)
            gpio_set_level(POWDER_C_PIN, 1);
        else{
            setRelay(dataBytes, outputRelay);
            writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 
        }

        vTaskDelay(pdMS_TO_TICKS(400)); 


        rTime = esp_timer_get_time();


        do{
            vTaskDelay(pdMS_TO_TICKS(100));

            cTime = esp_timer_get_time() - rTime;

            if(cTime >= extraTime)
                onWait = false;
            
        }while(onWait);


        if(thirdPowder)
            gpio_set_level(POWDER_C_PIN, 0);
        else{
            resetRelay(dataBytes, outputRelay);
            writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 
        }

        vTaskDelay(pdMS_TO_TICKS(400));

    }

    onWait = true;
    cTime = 0;
    rTime = 0;

    if(*boilerPulse == 1){
        *boilerPulse = 0;
        doBoilerPulse(dataBytes);
    }

    xTaskNotify(boilerTaskH, 0x20, eSetBits);

    setRelay(dataBytes, SOLENOID_VALVE_2);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700));

    setRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(500));

    setRelay(dataBytes, WHIPPER);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);


    xQueueSend(xQueueInputPulse, (void *) &pulses, portMAX_DELAY);

    vTaskDelay(pdMS_TO_TICKS(400));

        
    if(thirdPowder)
        gpio_set_level(POWDER_C_PIN, 1);
    else{
        setRelay(dataBytes, outputRelay);
        writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 
    }


    vTaskDelay(pdMS_TO_TICKS(400)); 


    rTime = esp_timer_get_time();

    
    do{
        xTaskNotifyWait(0xFFFF, 0xFFFF, &ulNotifiedValue, pdMS_TO_TICKS(200));

        if((ulNotifiedValue & 0x08) >> 3){
            onWait = false;

            //ESP_LOGI(CONTROL_TASK_TAG, "TURNING OFF PUMP");
        }


        if(noExtraTime && injectPowder && onWait){
            cTime = esp_timer_get_time() - rTime;

            if(cTime >= powderTime){
                injectPowder = false;
        
                //ESP_LOGI(CONTROL_TASK_TAG, "TURNING DOSER OFF");

                if(thirdPowder)
                    gpio_set_level(POWDER_C_PIN, 0);
                else{
                    resetRelay(dataBytes, outputRelay);
                    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 
                }
                
            }
        }

    }while(onWait);
    

    cTime = esp_timer_get_time() - rTime;


    
    if(injectPowder){
        
        if(thirdPowder)
            gpio_set_level(POWDER_C_PIN, 0);
        else{
            resetRelay(dataBytes, outputRelay);
            writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 
        }
        

        vTaskDelay(pdMS_TO_TICKS(400));
    }
    else
        vTaskDelay(pdMS_TO_TICKS(200));


    
    resetRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(700));

    resetRelay(dataBytes, SOLENOID_VALVE_2);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(4000));

    resetRelay(dataBytes, WHIPPER);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  



    //ESP_LOGE(CONTROL_TASK_TAG, "Inject powder water: OFF - %lf - %lf| puls: %d | gr: %d", powderTime, pulseTime, pulses, gr);

    xTaskNotify(boilerTaskH, 0x08, eSetBits);

    
}



static void injectOnlyWaterLine(uint8_t *dataBytes, const uint16_t pulses, uint8_t *boilerPulse){

    uint32_t ulNotifiedValue = 0;
    bool onWait = true;

    //ESP_LOGI(CONTROL_TASK_TAG, "Inject only water line: ON");
    if(*boilerPulse == 1){
        *boilerPulse = 0;
        doBoilerPulse(dataBytes);
    }

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
            //ESP_LOGI(CONTROL_TASK_TAG, "TURNING OFF PUMP");
        }

    }while(onWait);


    resetRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(1000));

    resetRelay(dataBytes, SOLENOID_VALVE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    //ESP_LOGE(CONTROL_TASK_TAG, "Inject only water line: OFF");

    xTaskNotify(boilerTaskH, 0x08, eSetBits);

}

static void injectOnlyWaterLineManual(uint8_t *dataBytes){
    static const uint16_t maxPulse = 200;  

    uint32_t ulNotifiedValue = 0;
    bool onWait = true;

    //ESP_LOGI(CONTROL_TASK_TAG, "Inject only water line: ON");
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
            //ESP_LOGI(CONTROL_TASK_TAG, "TURNING OFF PUMP");
        }
        
        if(checkWaterBtnFromUi()){
            onWait = false;
            //ESP_LOGI(CONTROL_TASK_TAG, "TURNING OFF PUMP");
        }
        

    }while(onWait);


    resetRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(1000));

    resetRelay(dataBytes, SOLENOID_VALVE_1);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    //ESP_LOGE(CONTROL_TASK_TAG, "Inject only water line: OFF");

    xTaskNotify(boilerTaskH, 0x08, eSetBits);
    xTaskNotify(intTaskH, 0x04, eSetBits); 
}

static void doBoilerPulse(uint8_t *dataBytes){
    setRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(1200));        //1200

    resetRelay(dataBytes, PUMP);
    writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2); 

    vTaskDelay(pdMS_TO_TICKS(1200));
}


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
            //ESP_LOGE(CONTROL_TASK_TAG, "ERROR: NO COFFEE!!!");
            xTaskNotify(uiTaskH, 0x004, eSetBits);
            runProcess = false;
        }
        else{
            setRelay(dataBytes, COFFEE_RELEASE_MAGNET);
            writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);    

            vTaskDelay(pdMS_TO_TICKS(1000));

            resetRelay(dataBytes, COFFEE_RELEASE_MAGNET);
            writeBytesMCP2307(MCP23017_OUTPUT_ADDR, 0x14, dataBytes, 2);  

            //ESP_LOGI(CONTROL_TASK_TAG, "Coffee released :)");
        }
    }

    return runProcess;

}

static void add2Counter(const uint8_t repIndex){

    if(recipeList[repIndex].moduleSize > 0){

        char *namespaceName = (char *)malloc(10);
        strcpy(namespaceName, "recipe0");

        if(repIndex < 9)
            namespaceName[6] = (char)(repIndex + 49);
        else{
            namespaceName[6] = '1';
            namespaceName[7] = (char)(repIndex + 39);
            namespaceName[8] = '\0';
        }

        nvs_handle_t nvsHandle;

        recipeList[repIndex].recipeCounter++;


        if(nvs_open(namespaceName, NVS_READWRITE, &nvsHandle) == ESP_OK){
            if(nvs_set_u16(nvsHandle, "counter", recipeList[repIndex].recipeCounter) == ESP_OK){
                nvs_commit(nvsHandle);
                ESP_LOGI(CONTROL_TASK_TAG, "Count updated %s - %d", recipeList[repIndex].recipeName, 
                                            recipeList[repIndex].recipeCounter);
            }

            nvs_close(nvsHandle);
        }



        free(namespaceName);
    }

}

static void cleanMachine(uint8_t *dataBytes){

    bool onWait = true; 
    uint16_t pulses = 142;
    uint32_t ulNotifiedValue = 0;

    //ESP_LOGE(CONTROL_TASK_TAG, "Clean machine: ON");

    setBrewer2InjectPosition(dataBytes);

    vTaskDelay(pdMS_TO_TICKS(1000));  

    injecBrewerWater(dataBytes, pulses);

    vTaskDelay(pdMS_TO_TICKS(2500));

    setBrewer2StartPosition(dataBytes);

    xTaskNotify(boilerTaskH, 0x20, eSetBits);

    vTaskDelay(pdMS_TO_TICKS(2000));

    ////////
    pulses = 100;

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

            //ESP_LOGI(CONTROL_TASK_TAG, "TURNING OFF PUMP");
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

    //ESP_LOGE(CONTROL_TASK_TAG, "Clean machine: OFF");

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
                    //ESP_LOGI(CONTROL_TASK_TAG, "pulseTime -> %lf", pulseTimeArray[i-1]);
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

    //ESP_LOGW(CONTROL_TASK_TAG, "times: %lf - %lf - %lf = %lf", 
      //          pulseTimeArray[0], pulseTimeArray[1], pulseTimeArray[2], totalTime);


    //ESP_LOGE(CONTROL_TASK_TAG, "Pulse time test: OFF");


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

        if(tempInputBtnStruct.btn6)
            temp = true;
    }

    return temp;
}

static bool checkStopBtnFromUi(){

    bool temp = false;

    InputBtnStruct tempInputBtnStruct;

    if(xQueueReceive(xQueueInputBtns, &tempInputBtnStruct, pdMS_TO_TICKS(10)) == pdPASS){

        if(tempInputBtnStruct.btn7)
            temp = true;
    }

    return temp;

}


static void clearInputBtnQueue(){
    InputBtnStruct tempInputBtnStruct;

    if(xQueueReceive(xQueueInputBtns, &tempInputBtnStruct, pdMS_TO_TICKS(10)) == pdPASS){

    }

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
            else if(tempInputBtnStruct.btnA){
                controlData->controlState = MENU_C;
                xTaskNotify(uiTaskH, 0x200, eSetBits);                 
            }
                /*
            else if(tempInputBtnStruct.btnB)
                controlData->controlState = MAINTENANCE_C;
                */
                
            
            /*
            ESP_LOGE(CONTROL_TASK_TAG, "-> %d %d %d %d %d %d %d %d %d %d",
                tempInputBtnStruct.btn0, tempInputBtnStruct.btn1, tempInputBtnStruct.btn2, 
                tempInputBtnStruct.btn3, tempInputBtnStruct.btn4, tempInputBtnStruct.btn5,
                tempInputBtnStruct.btn6, tempInputBtnStruct.btn7, tempInputBtnStruct.btnA, 
                tempInputBtnStruct.btnB);
                */
        }
        else if(controlData->controlState == MENU_C){
            if(tempInputBtnStruct.btn0){
                controlData->controlState = ENABLE_BLUE_C;
                xTaskNotify(uiTaskH, 0x040, eSetBits);
            }
            else if(tempInputBtnStruct.btn4){
                controlData->controlState = CLEAN_C;
                xTaskNotify(uiTaskH, 0x080, eSetBits);
            }
            else if(tempInputBtnStruct.btn7){
                controlData->controlState = IDLE_C;
                xTaskNotify(uiTaskH, 0x010, eSetBits);            
            }
            

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
                pulsesKey[6] = charNum;
                grKey[2] = charNum;

                err = nvs_get_u8(nvsHandle, enableKey, &u8Data);

                if(u8Data == 1){

                    nvs_get_u8(nvsHandle, moduleTypeKey, &u8Data);
                    recipeData[i].modulesArray[k].moduleType = (RecipeModuleType)u8Data;

                    nvs_get_u16(nvsHandle, pulsesKey, &u16Data);
                    recipeData[i].modulesArray[k].pulses = u16Data;

                    nvs_get_u8(nvsHandle, grKey, &u8Data);
                    recipeData[i].modulesArray[k++].gr = u8Data;

                }
            }
            
            nvs_close(nvsHandle);
        }
    }

    strcpy(namespaceName, "sysData");

    err = nvs_open(namespaceName, NVS_READONLY, &nvsHandle);

    if(err == ESP_ERR_NVS_NOT_FOUND){
        err = nvs_open(namespaceName, NVS_READWRITE, &nvsHandle);

        if(err == ESP_OK){
            nvs_set_u8(nvsHandle, "boilerTemp", 89);
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
            nvs_set_u8(nvsHandle, "powderA", 15);
            nvs_set_u8(nvsHandle, "powderB", 15);
            nvs_set_u8(nvsHandle, "powderC", 15);

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


static void checkOnlyRecipeMemContent(const Recipe *recipeData){
    for(size_t i = 0; i < 13; i++){


        ESP_LOGI(CONTROL_TASK_TAG, "%s[%d] (%d):", recipeData[i].recipeName, i, recipeData[i].moduleSize);

        if(recipeData[i].moduleSize > 0){

            for(size_t j = 0; j < recipeData[i].moduleSize; j++){
                ESP_LOGW(CONTROL_TASK_TAG, "%d: %d - %d - %d", j, recipeData[i].modulesArray[j].moduleType, 
                                                        recipeData[i].modulesArray[j].pulses, recipeData[i].modulesArray[j].gr);
            }
        }

        ESP_LOGI(CONTROL_TASK_TAG, "------------------" );
        
   }
}


static void checkMemContent(const Recipe *recipeData, const SystemData *sysData, PowderGramsRefData *powderData){

   for(size_t i = 0; i < 13; i++){


        ESP_LOGI(CONTROL_TASK_TAG, "%s[%d] (%d):", recipeData[i].recipeName, i, recipeData[i].moduleSize);

        if(recipeData[i].moduleSize > 0){

            for(size_t j = 0; j < recipeData[i].moduleSize; j++){
                ESP_LOGW(CONTROL_TASK_TAG, "%d: %d - %d - %d", j, recipeData[i].modulesArray[j].moduleType, 
                                                        recipeData[i].modulesArray[j].pulses, recipeData[i].modulesArray[j].gr);
            }
        }

        ESP_LOGI(CONTROL_TASK_TAG, "------------------" );
        
   }


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
    recipeData[0].recipeCounter = 0;


    recipeData[1].recipeName = (char *)malloc(17);
    memset(recipeData[1].recipeName, 0, 17);
    strcpy(recipeData[1].recipeName, "Medium Coffee");

    recipeData[1].modulesArray = (RecipeModuleStruct *)malloc(sizeof(RecipeModuleStruct) * 2);
    recipeData[1].modulesArray[0].moduleType = COFFEE_M;
    recipeData[1].modulesArray[0].pulses = 114;             
    recipeData[1].modulesArray[1].moduleType = WATER_M;
    recipeData[1].modulesArray[1].pulses = 10;

    recipeData[1].moduleSize = 2;
    recipeData[1].recipeCounter = 0;


    recipeData[2].recipeName = (char *)malloc(17);
    memset(recipeData[2].recipeName, 0, 17);
    strcpy(recipeData[2].recipeName, "Light Coffee");

    recipeData[2].modulesArray = (RecipeModuleStruct *)malloc(sizeof(RecipeModuleStruct) * 2);
    recipeData[2].modulesArray[0].moduleType = COFFEE_M;
    recipeData[2].modulesArray[0].pulses = 78;

    recipeData[2].modulesArray[1].moduleType = WATER_M;
    recipeData[2].modulesArray[1].pulses = 29;

    recipeData[2].moduleSize = 2;
    recipeData[2].recipeCounter = 0;


    recipeData[3].recipeName = (char *)malloc(17);
    memset(recipeData[3].recipeName, 0, 17);
    strcpy(recipeData[3].recipeName, "Americano");


    recipeData[3].modulesArray = (RecipeModuleStruct *)malloc(sizeof(RecipeModuleStruct) * 1);
    recipeData[3].modulesArray[0].moduleType = COFFEE_M;
    recipeData[3].modulesArray[0].pulses = 246;         //193

    recipeData[3].moduleSize = 1;
    recipeData[3].recipeCounter = 0;


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
    recipeData[4].recipeCounter = 0;


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
    recipeData[5].recipeCounter = 0;


    for(size_t i = 6; i < 13; i++){
        recipeData[i].recipeName = (char *)malloc(17);
        memset(recipeData[i].recipeName, 0, 17);

        recipeData[i].moduleSize = 0;
    }


    powderData->refGrPowA = 15;
    powderData->refGrPowB = 15;
    powderData->refGrPowC = 15;

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

    syncroState = IDLE_SYNC_C;    
    bool btEnabled = false;
    bool onStartPulse = true;

    float rtTemperature = 0;

    double refTimeIdle = 0;
    double cTimeIdle = 0;
    uint8_t idleBoilerPulse = 0;

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

    recvBuff = (char *)malloc(64);
    memset(recvBuff, 0, 64);

    commandBuff = (char *)malloc(64);
    memset(commandBuff, 0, 64);

    initMemData(recipeList, &systemData, &powderGramsRefData);

    powderGramsRefData.refGrPowA = 15;
    powderGramsRefData.refGrPowB = 15;
    powderGramsRefData.refGrPowC = 15;


    xQueueSend(xQueueData2Boiler, (void *) &systemData.boilerTemperature, (TickType_t) 10);

    checkMemContent(recipeList, &systemData, &powderGramsRefData);

    checkAirBreak(outputIO_Buff);

    xTaskNotify(uiTaskH, 0x020, eSetBits);                  //Set ui task to booting state

    startBoilerTask();

    waitBoiler2Start();

    //clearCoffeeChamber(outputIO_Buff);

    resetBrewer(outputIO_Buff);

    //syncronizeAllTasks();

    xTaskNotify(uiTaskH, 0x10, eSetBits);                   //Set ui task to idle

    ESP_LOGI(CONTROL_TASK_TAG, "ONLINE");      

    refTimeIdle = esp_timer_get_time();

    while(true){

        checkQueuesFromUi(&controlData);

        switch(controlData.controlState){
            case IDLE_C:
            case MENU_C:

            break;
            case ENABLE_BLUE_C:
                if(!btEnabled){
                    btEnabled = true;
                    //ESP_LOGI(CONTROL_TASK_TAG, "Blue enabled!!!");

                    initBLUE();
                }

                controlData.controlState = IDLE_C;

                vTaskDelay(pdMS_TO_TICKS(1000));

            break;
            case CLEAN_C:
                cleanMachine(outputIO_Buff);

                xTaskNotify(uiTaskH, 0x10, eSetBits);                   //Set ui task to idle

                controlData.controlState = IDLE_C;
            break;
            case DRINK_C:

                if(recipeList[controlData.recipeIndex].moduleSize > 0){

                    xQueuePeek(xQueueBoilerTemp, &rtTemperature, pdMS_TO_TICKS(60));

                    if(rtTemperature > (systemData.boilerTemperature - 3)){

                        /*
                        cTimeIdle = esp_timer_get_time() - refTimeIdle;

                        if(cTimeIdle >= 600000000 || onStartPulse){
                            onStartPulse = false;
                            idleBoilerPulse = 1;                        
                            ESP_LOGW(CONTROL_TASK_TAG, "boiler pulse water");
                        }
                        */


                        strcpy(uiDataControl.strData, recipeList[controlData.recipeIndex].recipeName);
                        xQueueSend(xQueueUI, (void *) &uiDataControl, (TickType_t) 10);

                        xTaskNotify(uiTaskH, 0x100, eSetBits);                   //Set ui task to preparing drink state

                        if(runDrink(&recipeList[controlData.recipeIndex], outputIO_Buff, &powderGramsRefData, &idleBoilerPulse))
                            add2Counter(controlData.recipeIndex);

                        xTaskNotify(uiTaskH, 0x10, eSetBits);                   //Set ui task to idle

                        //refTimeIdle = esp_timer_get_time();
                
                    }
                    else
                        xTaskNotify(uiTaskH, 0x400, eSetBits);

                }
                
                controlData.controlState = IDLE_C;
            break;
            case WATER_C:

                xQueuePeek(xQueueBoilerTemp, &rtTemperature, pdMS_TO_TICKS(60));

                if(rtTemperature > (systemData.boilerTemperature - 3)){

                    strcpy(uiDataControl.strData, " Water");
                    xQueueSend(xQueueUI, (void *) &uiDataControl, (TickType_t) 10);

                    xTaskNotify(uiTaskH, 0x100, eSetBits);                   //Set ui task to preparing drink state

                    injectOnlyWaterLineManual(outputIO_Buff);

                    xTaskNotify(uiTaskH, 0x10, eSetBits);                   //Set ui task to idle

                    vTaskDelay(pdMS_TO_TICKS(3000));

                    clearInputBtnQueue();

                    //refTimeIdle = esp_timer_get_time();
                }
                else
                    xTaskNotify(uiTaskH, 0x400, eSetBits);

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

        if(syncroState != IDLE_SYNC_C){
            switch(syncroState){
                case SEND_RECIPES_C:
                    sendAllRecipes(recipeList);
                break;
                case SEND_TEMPERATURE_C:
                    sendTempData(systemData.boilerTemperature);
                break;
                case END_SYNC_C:
                    closeSyncroData();
                break;
                default:
            }
        }

        checkAirBreak(outputIO_Buff);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    
    

}
#include "cJSON.h"
#include "parseJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int parseMSGJSON(const char* msg,char** resp){
    printLog("src string:%s\n",msg);
    const cJSON *jsonObject= NULL;
    const cJSON *doType = NULL;
    char *respmsg= NULL;
    jsonObject=cJSON_Parse(msg);
    if(jsonObject==NULL){
        printLog("JSON Invalid!");
        return PARSE_MSG_JSON_INVALID;
    }
    doType = cJSON_GetObjectItemCaseSensitive(jsonObject, "type");
    if (cJSON_IsString(doType) && (doType->valuestring != NULL))
    {
        printLog("Msg Type: %s\n", doType->valuestring);
    }else{
        printLog("JSON not Type!\n");
        cJSON_Delete((cJSON *)jsonObject);
        return PARSE_JSON_INVALID_TYPE;
    }
    if(strcmp(doType->valuestring,"write_serial")==0){
        const cJSON *payload= NULL;
        payload = cJSON_GetObjectItemCaseSensitive(jsonObject, "payload");
        if (cJSON_IsArray(payload)) {
            int array_size = cJSON_GetArraySize(payload);
            unsigned char *int_array = (unsigned char *)malloc(array_size * sizeof(unsigned char));
            memset(int_array,0,array_size);
            for (int i = 0; i < array_size; ++i) {
                cJSON *item = cJSON_GetArrayItem(payload, i);
                if (cJSON_IsNumber(item)) {
                    int_array[i]=(unsigned char)item->valueint;
                }else{
                    continue;
                }
            }
            printLog("Write Serial Payload %.*s \n", array_size,int_array);
            //TODO: write_serial
        }else{
            printLog("Payload Invalid!!!\n");
            cJSON_Delete((cJSON *)jsonObject);
            return PARSE_JSON_INVALID_PAYLOAD;
        }
    }
    if(strcmp(doType->valuestring,"get_usbswitch")==0){
        int ch442_en=0;
        int ch442_in=0;
        int ch442_status=0;
        
        if(ch442_en==0 && ch442_in==0){
            ch442_status=1;
        }else if(ch442_en==0 && ch442_in==1){
            ch442_status=0;
        }else{
            ch442_status=2;
        }
        cJSON *resp= cJSON_CreateObject();
        cJSON *status= cJSON_CreateString("usbswitch_status");
        cJSON *payload= cJSON_CreateNumber(ch442_status);
        cJSON_AddItemToObject(resp, "type", status);
        cJSON_AddItemToObject(resp, "payload", payload);
        respmsg=cJSON_PrintUnformatted(resp);
        cJSON_Delete(resp);
    }
    if(strcmp(doType->valuestring,"set_usbswitch")==0){
        const cJSON *payload= NULL;
        int ch442_en=0;
        int ch442_in=0;
        payload = cJSON_GetObjectItemCaseSensitive(jsonObject, "payload");
        if(cJSON_IsNumber(payload)){
            if(payload->valueint==1){
                ch442_en=0;
                ch442_in=0;
            }else if(payload->valueint==0){
                ch442_en=0;
                ch442_in=1;
            }else{
                ch442_en=1;
                ch442_in=1;
            }
            // TODO : set gpio
            printLog("Set ch442 IN:%d,EN:%d\n",ch442_in,ch442_en);
            cJSON *resp= cJSON_CreateObject();
            cJSON *msgType= cJSON_CreateString("info");
            cJSON *info= cJSON_CreateString("set usb switch success.");
            cJSON_AddItemToObject(resp, "type", msgType);
            cJSON_AddItemToObject(resp, "info", info);
            respmsg=cJSON_PrintUnformatted(resp);
            cJSON_Delete(resp);
        }else{
            printLog("Payload Invalid!!!\n");
            cJSON_Delete((cJSON *)jsonObject);
            return PARSE_JSON_INVALID_PAYLOAD;
        }
    }
    if(strcmp(doType->valuestring,"relay_action")==0){
        const cJSON *payload= NULL;
        const cJSON *relayID= NULL;
        const cJSON *relayAction= NULL;
        char *string= NULL;
        payload = cJSON_GetObjectItemCaseSensitive(jsonObject, "payload");
        if(cJSON_IsObject(payload)){
            relayID=cJSON_GetObjectItemCaseSensitive(payload, "relayid");
            relayAction=cJSON_GetObjectItemCaseSensitive(payload, "action");
            if(cJSON_IsNumber(relayID) && cJSON_IsNumber(relayAction)){
                printLog("Set relay%d action %d\n",relayID->valueint,relayAction->valueint);
                //TODO SET RELAY
            }else{
                printLog("Payload Invalid!!!\n");
                cJSON_Delete((cJSON *)jsonObject);
                return 0;
            }
        }else{
            printLog("Payload Invalid!!!\n");
            cJSON_Delete((cJSON *)jsonObject);
            return PARSE_JSON_INVALID_PAYLOAD;
        }
        char infoString[30]="";
        sprintf(infoString,"set relay%d success.",relayID->valueint);
        cJSON *resp= cJSON_CreateObject();
        cJSON *msgType= cJSON_CreateString("info");
        cJSON *info= cJSON_CreateString(infoString);
        cJSON_AddItemToObject(resp, "type", msgType);
        cJSON_AddItemToObject(resp, "info", info);
        respmsg=cJSON_PrintUnformatted(resp);
        cJSON_Delete(resp);
    }
    if(strcmp(doType->valuestring,"wake_on_lan")==0){
        const cJSON *payload= NULL;
        payload = cJSON_GetObjectItemCaseSensitive(jsonObject, "payload");
        if (cJSON_IsString(payload) && (payload->valuestring != NULL)){
            printLog("Wake on LAN:%s\n",payload->valuestring);
            //TODO : wake on lan
        }else{
            printLog("Payload Invalid!!!\n");
            cJSON_Delete((cJSON *)jsonObject);
            return PARSE_JSON_INVALID_PAYLOAD;
        }
    }
    printLog("Done\n");
    *resp=respmsg;
    cJSON_Delete((cJSON *)jsonObject);
    return PARSE_MSG_JSON_OK;
}
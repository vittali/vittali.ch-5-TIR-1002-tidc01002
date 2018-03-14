/*
 * Copyright (c) 2016, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* Standard includes                                                          */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <mqueue.h>
#include <time.h>

/* Hardware includes                                                          */
#include <ti/devices/cc32xx/inc/hw_types.h>
#include <ti/devices/cc32xx/inc/hw_ints.h>
#include <ti/devices/cc32xx/inc/hw_memmap.h>

/* Driverlib includes                                                         */
#include <ti/devices/cc32xx/driverlib/rom.h>
#include <ti/devices/cc32xx/driverlib/rom_map.h>
#include <ti/devices/cc32xx/driverlib/timer.h>

/* TI-Driver includes                                                         */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/sysbios/posix/_time.h>


/* Common interface includes                                                  */
#include <Utils/uart_term.h>
#include <Common/commonDefs.h>

/* Application includes                                                       */
#include <Board.h>
#include <CloudService/cloudJson.h>
#include "cloudServiceAWS.h"
#include "certs.h"

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_config.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_shadow_interface.h"
/*
 * The following macro is disabled by default. This is done to prevent the
 * certificate files from being written to flash every time the program
 * is run.  If an update to the cert files are needed, just update the
 * corresponding arrays, and rebuild with this macro defined. Note
 * you must remember to disable it otherwise the files will keep being
 * overwritten each time.
 */
#ifdef OVERWRITE_CERTS
static bool overwriteCerts = true;
#else
static bool overwriteCerts = false;
#endif


//*****************************************************************************
//                          LOCAL DEFINES
//*****************************************************************************



#define RXTASKSIZE              4096
#define MAX_DESCONNECTED_MSGS   20
#define MAX_NWK_THING_NAME      34

char HostAddress[255] = AWS_IOT_MQTT_HOST;
uint32_t port = AWS_IOT_MQTT_PORT;
bool messageArrivedOnDelta = false;

int devCount = 0;
CloudAWS_DevSubs_t devSubList[AWS_MAX_DEVICES];

char nwkThingName[MAX_NWK_THING_NAME];

const char *incomingDevCmds[] =
{
 "updateFanSpeed",
 "sendToggle",
 "updateDoorLock"
};
const CmdTypes incomingDevCmdTypes[] =
{
 CmdType_FAN_DATA,
 CmdType_LED_DATA,
 CmdType_DOORLOCK_DATA
};

//*****************************************************************************
//                      LOCAL FUNCTION PROTOTYPES
//*****************************************************************************

static void cloudAWS_stop();
int32_t cloudAWS_startMqttCli();
static void cloudAWS_deltaCb(const char *pJsonValueBuffer, uint32_t valueLength,
                             jsonStruct_t *pJsonStruct_t);


void UpdateStatusCallback(const char *pThingName, ShadowActions_t action,
        Shadow_Ack_Status_t status, const char *pReceivedJsonDocument,
        void *pContextData);

bool buildJSONForReported(char *pJsonDocument, size_t maxSizeOfJsonDocument,
        const char *pReceivedDeltaData, uint32_t lengthDelta);

void cloudAWS_registerDevShadowDelta(char *devThingName);

//*****************************************************************************
//                 GLOBAL VARIABLES
//*****************************************************************************


/* Receive task handle                                                        */
pthread_t g_rx_task_hndl = (pthread_t) NULL;


/* Message Queue                                                              */
mqd_t cloudSrvrMq;
mqd_t disconnectedMq;
mqd_t *gatewayMq;
AWS_IoT_Client mqttClient;
bool wlanConnected = false;
bool awsCloudConnected = false;
char stringToEchoDelta[SHADOW_MAX_SIZE_OF_RX_BUFFER];

int32_t NetWiFi_isConnected(void)
{
    if(wlanConnected)
    {
        return SUCCESS;
    }
    return FAILURE;
}

/*
 *  ======== flashCerts ========
 *  Utility function to flash the contents of a buffer (PEM format) into the
 *  filename/path specified by certName (DER format)
 */
void flashCerts(uint8_t *certName, uint8_t *buffer, uint32_t bufflen)
{
    int status = 0;
    int16_t slStatus = 0;
    SlFsFileInfo_t fsFileInfo;

    /* Check if the cert file already exists */
    slStatus = sl_FsGetInfo(certName, 0, &fsFileInfo);

    /* If the cert doesn't exist, write it (or overwrite if specified to) */
    if (slStatus == SL_ERROR_FS_FILE_NOT_EXISTS || overwriteCerts == true)
    {

        UART_PRINT("\n\r[CS_AWS] Flashing certificate file ...\n\r");

        /* Convert the cert to DER format and write to flash */
        status = TLS_writeDerFile(buffer, bufflen, TLS_CERT_FORMAT_PEM,
                (const char *)certName);

        if (status != 0)
        {
            UART_PRINT("[CS_AWS] Error: Could not write file %s to flash (%d)\n\r",
                    certName, status);
            //while(1);
        }
        else
        {
            UART_PRINT("[CS_AWS] successfully wrote file %s to flash\n\r", certName);
            //break;
        }
    }
}


void *cloudAWS_MqttClientThread(void * pvParameters)
{
    msgQueue_t queueElement;
    IoT_Error_t rc = SUCCESS;
    ShadowInitParameters_t sp = ShadowInitParametersDefault;

    sp.pHost = HostAddress;
    sp.port = port;
    sp.pClientCRT = AWS_IOT_CERTIFICATE_FILENAME;
    sp.pClientKey = AWS_IOT_PRIVATE_KEY_FILENAME;
    sp.pRootCA = AWS_IOT_ROOT_CA_FILENAME;
    sp.enableAutoReconnect = true;
    sp.disconnectHandler = NULL;

    IOT_INFO("Shadow Init");
    rc = aws_iot_shadow_init(&mqttClient, &sp);
    if (SUCCESS != rc)
    {
        IOT_ERROR("Shadow Initialization Error (%d)", rc);
        return NULL;
    }

    ShadowConnectParameters_t scp = ShadowConnectParametersDefault;
    scp.pMyThingName = nwkThingName;
    scp.pMqttClientId = AWS_IOT_MQTT_CLIENT_ID;
    scp.mqttClientIdLen = (uint16_t) strlen(AWS_IOT_MQTT_CLIENT_ID);

    IOT_INFO("Shadow Connect");
    rc = aws_iot_shadow_connect(&mqttClient, &scp);
    if (SUCCESS != rc)
    {
        IOT_ERROR("Shadow Connection Error (%d)", rc);
        return NULL;
    }

    /*
     *  Enable Auto Reconnect functionality. Minimum and Maximum time of
     *  exponential backoff are set in aws_iot_config.h:
     *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
     *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
     */
    rc = aws_iot_shadow_set_autoreconnect_status(&mqttClient, true);
    if (SUCCESS != rc)
    {
        IOT_ERROR("Unable to set Auto Reconnect to true - %d", rc);
    }

    jsonStruct_t deltaObject;
    deltaObject.pData = stringToEchoDelta;
    deltaObject.pKey = "state";
    deltaObject.type = SHADOW_JSON_OBJECT;
    deltaObject.cb = cloudAWS_deltaCb;

    /*
     * Register the jsonStruct object
     */
    rc = aws_iot_shadow_register_delta(&mqttClient, &deltaObject);

    aws_iot_shadow_disable_discard_old_delta_msgs();
    while (1)
    {
        /*
         * Check for the incoming messages for 500 ms.
         */
        rc = aws_iot_shadow_yield(&mqttClient, 500);
        CloudAWS_handleCloudConnect();
        if (SUCCESS != rc)
        {
            IOT_INFO("MQTT RX LOOP RC: %d", rc);
        }
        if(NETWORK_RECONNECT_TIMED_OUT_ERROR == rc )
        {
            break;
        }
        /* sleep for some time in seconds */
        sleep(1);

    }
    if (SUCCESS != rc)
    {
        IOT_ERROR("An error occurred in the loop %d", rc);
    }

    queueElement.event = CommonEvent_CLOUD_SERV_DISCONNECTED;
    queueElement.msgPtr = NULL;
    UART_PRINT("[Cloud Service] MQTT CLIENT disconnected, exit pthread\n\r");
    /* write message indicating disconnect Broker message.                    */
    mq_send(cloudSrvrMq, (char*) &queueElement, sizeof(msgQueue_t), 0);

    pthread_exit(0);

    return NULL;
}

/*!
 Register the message queue

 Public function defined in cloudService.h
 */
void cloudAWS_registerCliMq(mqd_t *pGatewayMq)
{
    gatewayMq = pGatewayMq;
}

/*!
 Initialize the AWS client

 Public function defined in cloudService.h
 */
void cloudAWS_init(const char *cloudSrvrMqName)
{
    mq_attr attr;
    unsigned mode = 0;

    attr.mq_maxmsg = MAX_DESCONNECTED_MSGS;
    attr.mq_msgsize = sizeof(msgQueue_t);
    disconnectedMq = mq_open("disconnectedMQ", O_CREAT | O_NONBLOCK, mode, &attr);
    cloudSrvrMq = mq_open(cloudSrvrMqName, O_WRONLY );//| O_NONBLOCK);

    if (cloudSrvrMq == NULL)
    {
        UART_PRINT("[Cloud Service] MQTT Message Queue create fail\n\r");
    }

    snprintf(nwkThingName, MAX_NWK_THING_NAME,"ti_iot_%s_network", AWS_IOT_MY_THING_NAME);

}

/*!
 Start the cloud client

 Public function defined in cloudService.h
 */
void cloudAWS_start()
{
    /* Flash Certificate Files */
   flashCerts((uint8_t *)AWS_IOT_ROOT_CA_FILENAME, (uint8_t *)root_ca_pem,
           strlen(root_ca_pem));

   flashCerts((uint8_t *)AWS_IOT_CERTIFICATE_FILENAME,
           (uint8_t *)client_cert_pem, strlen(client_cert_pem));

   flashCerts((uint8_t *)AWS_IOT_PRIVATE_KEY_FILENAME,
           (uint8_t *)client_private_key_pem, strlen(client_private_key_pem));
    if(wlanConnected && !awsCloudConnected)// && cloudConnectionInfo)
    {
        awsCloudConnected = (cloudAWS_startMqttCli() == 0);
        if(awsCloudConnected)
        {
            msgQueue_t mqEvt = {CommonEvent_CLOUD_SERV_CONNECTED, NULL, 0};
            mq_send(cloudSrvrMq, (char*)&mqEvt, sizeof(msgQueue_t), MQ_LOW_PRIOR);
        }
    }
}


/*!
 Start the AWS mqtt client

 Public function defined in cloudService.h
 */
int32_t cloudAWS_startMqttCli()
{
    int32_t threadArg = 100;
    int32_t lRetVal = -1;
    pthread_attr_t pAttrs;
    struct sched_param priParam;
    
    /* Open Client Receive Thread start the receive task. Set priority and    */
    /* stack size attributes                                                  */
    pthread_attr_init(&pAttrs);
    priParam.sched_priority = CLOUDRX_TASK_PRI;
    lRetVal = pthread_attr_setschedparam(&pAttrs, &priParam);
    lRetVal |= pthread_attr_setstacksize(&pAttrs, RXTASKSIZE);
    lRetVal |= pthread_attr_setdetachstate(&pAttrs, PTHREAD_CREATE_DETACHED);
    lRetVal |= pthread_create(&g_rx_task_hndl, &pAttrs, cloudAWS_MqttClientThread, (void *) &threadArg);

    if (lRetVal != 0)
    {
        UART_PRINT("[Cloud Service] Client Thread Create Failed failed\n\r");
        return -1;
    }
    UART_PRINT("[Cloud Service] RX MQTT THREAD STARTED\n\r");
    return 0;
}

/*!
 Stop the AWS Client

 Public function defined in cloudService.h
 */
void cloudAWS_stop()
{
    IoT_Error_t rc = SUCCESS;
    if(awsCloudConnected)
    {
        IOT_INFO("Disconnecting");
        rc = aws_iot_shadow_disconnect(&mqttClient);

        if (SUCCESS != rc)
        {
            IOT_ERROR("Disconnect error %d", rc);
        }
        awsCloudConnected = false;
    }
}


void CloudAWS_handleWlanDisconnect()
{
    wlanConnected = false;
}
void CloudAWS_handleWlanConnect()
{
    wlanConnected = true;
    //cloudAWS_start();
}
void CloudAWS_handleCloudDisconnect()
{
    cloudAWS_stop();
    cloudAWS_start();
}
void CloudAWS_handleCloudConnect(void)
{
    uint8_t timeout = MAX_DESCONNECTED_MSGS;
    msgQueue_t queueElemRecv;
    // flush all queued msgs accumulated while cloud was disconnected
    while(mq_receive(disconnectedMq, (char*) &queueElemRecv, sizeof(msgQueue_t), NULL) > 0 && timeout--)
    {
        char *tmpBuff;
        char *extractedJson;
        char thingname[MAX_DEV_NAME];
        int32_t rc = 0;
        char stringToEchoDelta[SHADOW_MAX_SIZE_OF_RX_BUFFER];
        tmpBuff = (char*) queueElemRecv.msgPtr;

        switch(queueElemRecv.event)
        {
        case CloudServiceEvt_STATE_CNF_EVT:
        case CloudServiceEvt_NWK_UPDATE:
            buildJSONForReported(stringToEchoDelta, SHADOW_MAX_SIZE_OF_RX_BUFFER,
                                 tmpBuff, queueElemRecv.msgPtrLen);
            rc = aws_iot_shadow_update(&mqttClient, nwkThingName,
                                        stringToEchoDelta, UpdateStatusCallback, NULL, 2, false);
            break;

        case CloudServiceEvt_DEV_UPDATE:
            extractedJson = jsonParseIn(tmpBuff, "ext_addr");
            if(extractedJson)
            {
                IOT_DEBUG("\n\rExtracted: %s", extractedJson);
                snprintf(thingname, MAX_DEV_NAME, "ti_iot_%s_%s", AWS_IOT_MY_THING_NAME, extractedJson);
                thingname[MAX_DEV_NAME - 1] = '\0';
                free(extractedJson);

                snprintf(stringToEchoDelta, SHADOW_MAX_SIZE_OF_RX_BUFFER,
                        "{\"state\":{\"desired\": null, \"reported\":%.*s}}",
                        queueElemRecv.msgPtrLen, tmpBuff);
                rc = aws_iot_shadow_update(&mqttClient, thingname,
                                           stringToEchoDelta, UpdateStatusCallback, NULL, 2, false);
                cloudAWS_registerDevShadowDelta(thingname);


                IOT_DEBUG("\n\rTHING NAME: %s", thingname);
            }
            else
            {
                IOT_WARN("NO ext_addr found in message from gateway: ---> %s", tmpBuff);
                stringToEchoDelta[0] = '\0';
            }

            break;
        }
        IOT_DEBUG("\n\rMsg to AWS: %s (%d)\n\rReported Len: %d", stringToEchoDelta, rc, strlen(stringToEchoDelta));

        if(queueElemRecv.msgPtr)
        {
            free(queueElemRecv.msgPtr);
            queueElemRecv.msgPtr = NULL;
        }
    }
}

bool buildJSONForReported(char *pJsonDocument, size_t maxSizeOfJsonDocument,
        const char *pReceivedDeltaData, uint32_t lengthDelta)
{
    int32_t ret;

    if (pJsonDocument == NULL)
    {
        return false;
    }

    ret = snprintf(pJsonDocument, maxSizeOfJsonDocument,
                   "{\"state\":{\"reported\":%.*s}}",
            lengthDelta, pReceivedDeltaData);

    if (ret >= maxSizeOfJsonDocument || ret < 0)
    {
        return false;
    }

    return true;
}
void CloudAWS_handleGatewayEvt(msgQueue_t *inEvtMsg)
{
    if(!awsCloudConnected && wlanConnected)
    {
        cloudAWS_start();
    }

    char *buf = (char*)malloc(inEvtMsg->msgPtrLen);
    memcpy(buf, inEvtMsg->msgPtr, inEvtMsg->msgPtrLen);
    msgQueue_t mqEvt = {inEvtMsg->event, buf, inEvtMsg->msgPtrLen};
    if(mq_send(disconnectedMq, (char*) &mqEvt, sizeof(msgQueue_t), 0) != 0)
    {
        free(buf);
    }
}


/*!
 Parse the received message from the cloud

 Public function defined in cloudService.h
 */
void CloudAWS_handleCloudEvt(msgQueue_t *inEvtMsg)
{
    char *extractedJson;
    char *tmpBuff;
    deviceCmd_t *inCommand;
    msgQueue_t queueElementSend;
    inCommand = malloc(sizeof(deviceCmd_t));
    inCommand->cmdType =   0xFFFF;
    inCommand->shortAddr = 0x0000;
    tmpBuff = (char *)inEvtMsg->msgPtr;
    UART_PRINT(" [CS_AWS] InCmd: %s\n\r", tmpBuff);

    extractedJson = jsonParseIn(tmpBuff, "state");
    if(extractedJson)
    {
        UART_PRINT(" [CS_AWS] ExtractedJson: %s\n\r", extractedJson);
        queueElementSend.event = GatewayEvent_PERMIT_JOIN;
        inCommand->cmdType = CmdType_PERMIT_JOIN;
        if(strncmp(extractedJson, "open", 5) == 0)
        {
            inCommand->data = 1;
        }
        else if(strncmp(extractedJson, "close", 5) == 0)
        {
            inCommand->data = 0;
        }
        free(extractedJson);
    }

    extractedJson = jsonParseIn(tmpBuff, "dstAddr");
    if(!extractedJson)
    {
        extractedJson = jsonParseIn(tmpBuff, "ext_addr");
        inCommand->shortAddr = 0xFFFF;
    }


    if(extractedJson)
    {
        UART_PRINT(" [CS_AWS] ExtractedJson: %s\n\r", extractedJson);
        int shortAddr;
        inCommand->cmdType = CmdType_DEVICE_DATA;
        queueElementSend.event = GatewayEvent_DEVICE_CMD;
        if(inCommand->shortAddr == 0x0000)
        {
            sscanf(extractedJson, "%x", &shortAddr);
            inCommand->shortAddr = shortAddr;
        }
        else if(inCommand->shortAddr == 0xFFFF)
        {
            *(uint64_t*)inCommand->extAddr = strtoull (extractedJson, NULL, 16);
        }
        free(extractedJson);

        extractedJson = jsonParseIn(tmpBuff, "devActionType");
        if(extractedJson)
        {
            for(uint8_t cmds = 0; cmds < sizeof(incomingDevCmds); cmds++)
            {
                if(strncmp(extractedJson, incomingDevCmds[cmds], strlen(incomingDevCmds[cmds])) == 0)
                {
                    inCommand->cmdType = incomingDevCmdTypes[cmds];
                    break;
                }
            }
            free(extractedJson);
        }



        extractedJson = jsonParseIn(tmpBuff, "value");
        if(extractedJson)
        {
            inCommand->data = atoi(extractedJson);
            free(extractedJson);
        }
        extractedJson = jsonParseIn(tmpBuff, "toggleLED");
        if(extractedJson)
        {
            inCommand->cmdType = CmdType_LED_DATA;
            inCommand->data = 0;
            if(strncmp(extractedJson, "true", strlen("true")) == 0)
            {
                inCommand->data = 1;
            }
            free(extractedJson);
        }
    }

    queueElementSend.msgPtr = inCommand;

    /* send message to gateway task for processing */
    if(mq_send(*gatewayMq, (char*) &queueElementSend, sizeof(msgQueue_t), 0) != 0)
    {
        free(inCommand);
    }
}



//****************************************************************************
//
//! Callback in case of various event (for clients connection with remote
//! broker)
//!
//! \param[in]  app_hndl    - is the ctx handl for the connection
//! \param[in]  evt         - is a event occured
//! \param[in]  buff        - is the pointer to the buffer for data
//!                           (for this event)
//! \param[in]  len         - is the length of the buffer data
//!
//! return none
//
//****************************************************************************
void cloudAWS_deltaCb(const char *pJsonValueBuffer, uint32_t valueLength,
                           jsonStruct_t *pJsonStruct_t)
{
    char *deltaBuf = malloc(valueLength + 1);
    memcpy(deltaBuf, pJsonValueBuffer, valueLength);
    deltaBuf[valueLength] = '\0';
    IOT_DEBUG("Received Delta message %.*s", valueLength, pJsonValueBuffer);
    

    {
        msgQueue_t mqEvt = {CloudServiceEvt_CLOUD_IN_MQTT, 
                            deltaBuf, 
                            valueLength};
        mq_send(cloudSrvrMq, (char*)&mqEvt, sizeof(msgQueue_t), MQ_LOW_PRIOR);

    }
}


void UpdateStatusCallback(const char *pThingName, ShadowActions_t action,
        Shadow_Ack_Status_t status, const char *pReceivedJsonDocument,
        void *pContextData)
{
    if (status == SHADOW_ACK_TIMEOUT)
    {
        IOT_INFO("Update Timeout--");
    }
    else if (status == SHADOW_ACK_REJECTED)
    {
        IOT_INFO("Update RejectedXX");
    }
    else if (status == SHADOW_ACK_ACCEPTED)
    {
        IOT_INFO("Update Accepted !!");
    }
}

#define THING_NAME_START_IDX 12
static void cloudAWS_devShadowDeltaCb(AWS_IoT_Client *pClient, char *topicName,
                                  uint16_t topicNameLen, IoT_Publish_Message_Params *params, void *pData)
{
    char thingName[MAX_SIZE_OF_THING_NAME];
    char *deltaBuf;
    char *jsonPayload;
    uint8_t thingNameLen = 0;
    uint8_t deltaBufLen;

    UART_PRINT("[Cloud Service] RX Dev shadow delta:\n\r");
    UART_PRINT("[Cloud Service] %s\n\n\r", topicName);
    // start at Idx 12 since thats where the thing name starts "$aws/things/"
    for(uint8_t charIdx = THING_NAME_START_IDX; charIdx < topicNameLen; charIdx++)
    {
        if(topicName[charIdx] =='/')
        {
            thingNameLen = charIdx - THING_NAME_START_IDX;
            memcpy(thingName, &topicName[THING_NAME_START_IDX], thingNameLen);
            thingName[thingNameLen] = '\0';
            break;
        }
    }
    UART_PRINT("[Cloud Service] Thing Name: %s\n\r", thingName);
    for(uint8_t charIdx = thingNameLen-1; charIdx ; charIdx--)
    {
        if(thingName[charIdx] =='_')
        {
            thingNameLen = thingNameLen - (charIdx + 1);
            memcpy(thingName, &thingName[charIdx + 1], thingNameLen);
            thingName[thingNameLen] = '\0';
            break;
        }
    }
    UART_PRINT("[Cloud Service] Extracted ExtAddr: %s\n\r", thingName);

    deltaBufLen = 30 + thingNameLen + params->payloadLen - 1;
    jsonPayload = malloc(params->payloadLen - 1);
    deltaBuf = malloc(deltaBufLen);

    memcpy(jsonPayload, ((uint8_t*)params->payload + 1), params->payloadLen - 2);
    jsonPayload[params->payloadLen - 2] = '\0';

    snprintf(deltaBuf, 250, "{\"ext_addr\":\"%s\",%s}", thingName, jsonPayload);
    free(jsonPayload);

    IOT_DEBUG("Received Delta message %.*s", deltaBufLen, deltaBuf);


    {
        msgQueue_t mqEvt = {CloudServiceEvt_CLOUD_IN_MQTT,
                            deltaBuf,
                            deltaBufLen};
        mq_send(cloudSrvrMq, (char*)&mqEvt, sizeof(msgQueue_t), MQ_LOW_PRIOR);

    }
}

static int cloudAWS_addDevThing(char *devThingName)
{
    int devIdx;
    for(devIdx = 0; devIdx < devCount; devIdx++)
    {
        if(strncmp(devSubList[devIdx].devThingName,
                   devThingName, MAX_DEV_NAME) == 0)
        {
            return devIdx;
        }
    }

    if(devCount < AWS_MAX_DEVICES)
    {
        snprintf(devSubList[devIdx].topic, MAX_DEV_TOPIC_LEN,
                 "$aws/things/%s/shadow/update/delta", devThingName);
        devSubList[devIdx].topic[MAX_DEV_TOPIC_LEN - 1] = '\0';

        strncpy(devSubList[devIdx].devThingName, devThingName, MAX_DEV_NAME);
        devSubList[devIdx].devThingName[MAX_DEV_NAME - 1] = '\0';
        devSubList[devIdx].registered = false;

        devCount++;
        return devIdx;
    }

    return -1;
}

void cloudAWS_registerDevShadowDelta(char *devThingName)
{
    int devIdx = cloudAWS_addDevThing(devThingName);
    char *devTopic = devSubList[devIdx].topic;
    IoT_Error_t rc;

    if(!devSubList[devIdx].registered)
    {
        IOT_DEBUG("\n\n\rDEVICE TOPIC--------->> %.*s, len: %d",
                  MAX_DEV_TOPIC_LEN, devTopic, strlen(devTopic));
        rc = aws_iot_mqtt_subscribe(&mqttClient, devTopic,
                                    (uint16_t) strlen(devTopic), QOS0,
                                    cloudAWS_devShadowDeltaCb, NULL);
        devSubList[devIdx].registered = (rc == SUCCESS);
    }
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************

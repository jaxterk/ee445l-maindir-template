//*****************************************************************************
//
// cc3100_http_server.c - WebServer Application on CC3100 BoosterPack.
//
// Copyright (c) 2016 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 2.1.3.156 of the EK-TM4C1294XL Firmware Package.
//
//*****************************************************************************

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"
#include "drivers/buttons.h"
#include "drivers/pinout.h"
#include "io.h"
#include "cc3100_fsdata.h"

#include "simplelink.h"
#include "protocol.h"
#include "sl_common.h"

//*****************************************************************************
//
//! \addtogroup example_list
//! <h1>CC3100 HTTP Server Example (cc3100_http_server)</h1>
//!
//! This application demonstrates web-based I/O control using CC3100
//! Boosterpack (on Boosterpack 2 header) and EK-TM4C1294XL, which allows the
//! end-users to communicate and control with EK-TM4C1294XL using standard
//! web-browsers.
//!
//! The CC3100 acts as an Access Point and uses the SSID <app_mode_SSID> that
//! can be modified statically by updating the value of the define SSID_AP_MODE
//! in the file sl_common.h.
//!
//! The information and debug messages are transmitted over UART0, which can be
//! accessed with a terminal window at 115200 baud with the settings 8-N-1.
//!
//! This application is built with Webpages, that can be uploaded to the serial
//! flash on CC3100 Boosterpack by pressing and holding USR_SW1 during reset.
//! These webpages take preference over the default ones that come with the
//! CC3100 BoosterPack.
//!
//! Once a WIFI station connects to CC3100, domain name and authorization
//! parameters are displayed on the terminal window.  Use the domain name on a
//! standard browser to access the webserver and enter authorization details
//! when prompted.
//!
//! The I/O control can be achieved via the webpage ``IO Control Demo'', which
//! uses JavaScript running in the web browser to send HTTP requests to control
//! the Animation LED (LED D1) and the Toggle LED (LED D2).  Responses
//! generated by the board are returned to the browser and inserted into the
//! page's HTML dynamically by more JavaScript code.
//!
//! Source files for the internal file system image can be found in the ``fs''
//! directory.  If any of these files are changed, the file system image
//! (cc3100_fsdata.h) should be rebuilt by running the following command from
//! the project root directory:
//!
//! ../../../../tools/bin/makefsfile -i fs -o cc3100_fsdata.h -r -h -q
//
//*****************************************************************************
//*****************************************************************************
//
// Timeout to shutdown CC3100. 
//
//*****************************************************************************
#define SL_STOP_TIMEOUT        0xFF

//*****************************************************************************
//
// Maximum length a file name can be. 
//
//*****************************************************************************
#define MAX_FILE_NAME_SIZE      50

//*****************************************************************************
//
// Prefix added to files before uploading to CC3100 Boosterpack's serial flash.
//
//*****************************************************************************
#define FILE_NAME_PREFIX        "www"

//*****************************************************************************
//
// The system clock frequency.  This variable is declared i the board.c file.
//
//*****************************************************************************
extern uint32_t g_SysClock;

//*****************************************************************************
//
// Flags to keep track of the Status of the Toggle LED and blink rate of the
// Animation LED.
//
//*****************************************************************************
volatile uint32_t gui32LEDStatus;
extern volatile uint32_t g_ui32AnimSpeed;

//*****************************************************************************
//
// A set of flags used by Simplelink Event Handlers
//
//*****************************************************************************
uint32_t  g_ui32Status = 0;

//*****************************************************************************
//
// Various Buffers to hold Authentication parameters and Domain name and URN.
//
//*****************************************************************************
char g_pcAuthName[MAX_AUTH_NAME_LEN+1];
char g_pcAuthPass[MAX_AUTH_PASSWORD_LEN+1];
char g_pcAuthRealm[MAX_AUTH_REALM_LEN+1];
char g_pcDomainName[MAX_DOMAIN_NAME_LEN];
char g_pcDeviceURN[MAX_DEVICE_URN_LEN];

//*****************************************************************************
//
// Post and Get tokens used to communicate with CC3100.
//
// __SL_P_U00 and __SL_G_U00 is used to Post and Get the Toggle LED state.
// __SL_P_U01 and __SL_G_U01 is used to Post and Get the blink rate of
//  Animation LED.
//
//*****************************************************************************
char g_pcPOSTToken0[] = "__SL_P_U00";
char g_pcPOSTToken1[] = "__SL_P_U01";

char g_pcGETToken0[]  = "__SL_G_U00";
char g_pcGETToken1[]  = "__SL_G_U01";

//*****************************************************************************
//
// Application specific status/error codes.
//
//*****************************************************************************
typedef enum
{
    //
    // Choosing the following number to avoid overlap with host-driver's error
    // codes.
    //
    DEVICE_NOT_IN_STATION_MODE = -0x7D0,
    STATUS_CODE_MAX = -0xBB8
}e_AppStatusCodes;

//*****************************************************************************
//
// Set the speed of the animation shown on the display.  In this version, the
// speed is described as a decimal number encoded as an ASCII string.
//
//*****************************************************************************
uint32_t
String2Decimal(unsigned char *pucBuf)
{
    uint32_t ui32Speed;

    //
    // Parse the passed parameter as a decimal number.
    //
    ui32Speed = 0;
    while((*pucBuf >= '0') && (*pucBuf <= '9'))
    {
        ui32Speed *= 10;
        ui32Speed += (*pucBuf - '0');
        pucBuf++;
    }

    return (ui32Speed);
}

//*****************************************************************************
//
// This function handles WLAN events.
//
// Parameter psWlanEvent is the event passed to the handler
//
//*****************************************************************************
void
SimpleLinkWlanEventHandler(SlWlanEvent_t * psWlanEvent)
{
    slWlanConnectAsyncResponse_t * psEventData = NULL;

    if(psWlanEvent == NULL)
    {
        UARTprintf(" [WLAN EVENT] NULL Pointer Error\n\r");
        return;
    }

    switch(psWlanEvent->Event)
    {
        case SL_WLAN_CONNECT_EVENT:
        {
            SET_STATUS_BIT(g_ui32Status, STATUS_BIT_CONNECTION);

            //
            // Information about the connected AP (like name, MAC etc) will be
            // available in 'slWlanConnectAsyncResponse_t' - Applications
            // can use it if required
            //
            // slWlanConnectAsyncResponse_t * psEventData = NULL;
            // psEventData = &psWlanEvent->EventData.STAandP2PModeWlanConnected;
            //
        }
        break;

        case SL_WLAN_DISCONNECT_EVENT:
        {
            CLR_STATUS_BIT(g_ui32Status, STATUS_BIT_CONNECTION);
            CLR_STATUS_BIT(g_ui32Status, STATUS_BIT_IP_ACQUIRED);

            psEventData = &psWlanEvent->EventData.STAandP2PModeDisconnected;

            //
            // If the user has initiated 'Disconnect' request, 'reason_code' is
            // SL_USER_INITIATED_DISCONNECTION.
            //
            if(psEventData->reason_code == SL_USER_INITIATED_DISCONNECTION)
            {
                UARTprintf(" Device disconnected from the AP on application's "
                           "request \n\r");
            }
            else
            {
                UARTprintf(" Device disconnected from the AP on an ERROR..!! "
                           "\n\r");
            }
        }
        break;

        case SL_WLAN_STA_CONNECTED_EVENT:
        {
            SET_STATUS_BIT(g_ui32Status, STATUS_BIT_STA_CONNECTED);
        }
        break;

        case SL_WLAN_STA_DISCONNECTED_EVENT:
        {
            CLR_STATUS_BIT(g_ui32Status, STATUS_BIT_STA_CONNECTED);
            CLR_STATUS_BIT(g_ui32Status, STATUS_BIT_IP_LEASED);
        }
        break;

        default:
        {
            UARTprintf(" [WLAN EVENT] Unexpected event \n\r");
        }
        break;
    }
}

//*****************************************************************************
//
// This function handles callback for the HTTP server events.
//
// Parameter psEvent contains the relevant event information
//
// Parameter psResponse should be filled by the user with the relevant response
// information
//
//*****************************************************************************
void
SimpleLinkHttpServerCallback(SlHttpServerEvent_t * psEvent,
                             SlHttpServerResponse_t * psResponse)
{
    uint32_t ui32Speed;
    uint8_t * pui8Data = 0;
    char pcBuf[5];

    if(psEvent == NULL || psResponse == NULL)
    {
        UARTprintf(" [HTTP EVENT] NULL Pointer Error \n\r");
        return;
    }

    switch (psEvent->Event)
    {
        //
        // Handle HTTP GET requests from Client.
        //
        case SL_NETAPP_HTTPGETTOKENVALUE_EVENT:
        {
            //
            // Get the address of the buffer, that handles the response to the
            // GET request.
            //
            pui8Data = psResponse->ResponseData.token_value.data;
            psResponse->ResponseData.token_value.len = 0;

            //
            // Check if Toggle LED status is requested (Get command).
            //
            if(pal_Memcmp(psEvent->EventData.httpTokenName.data, g_pcGETToken0,
                                               pal_Strlen(g_pcGETToken0)) == 0)
            {                
                //
                // Pass a value to the buffer (that handles GET response) based
                // on the global flag that tracks the Toggle LED status.
                //
                if(gui32LEDStatus == 1)
                {
                    pal_Memcpy(pui8Data, "ON", pal_Strlen("ON"));
                    pui8Data += 2;
                    psResponse->ResponseData.token_value.len += 2;
                }
                else
                {
                    pal_Memcpy(pui8Data, "OFF", pal_Strlen("OFF"));
                    pui8Data += 3;
                    psResponse->ResponseData.token_value.len += 3;
                }
                *pui8Data = '\0';
            }

            //
            // Check if Animation LED toggle rate is requested (Get command).
            //
            if(pal_Memcmp(psEvent->EventData.httpTokenName.data, g_pcGETToken1,
                                           pal_Strlen(g_pcGETToken1)) == 0)
            {
                //
                // Check if the global resource that tracks the Animation LED
                // toggle rate is within bounds.
                //
                if(g_ui32AnimSpeed > 100)
                {
                    UARTprintf("  Web token Error: Wrong Toggle rate value "
                               "tracked - using default value.\r\n");
                    ui32Speed = 10;
                }
                
                //
                // Convert toggle rate of Animation LED from decimal to string.
                //
                usnprintf(pcBuf, sizeof(pcBuf), "%d", g_ui32AnimSpeed);

                //
                // Pass this string to the buffer that handles GET response.
                //
                pal_Memcpy(pui8Data, (uint8_t *)(pcBuf), pal_Strlen(pcBuf));
                pui8Data += pal_Strlen(pcBuf);
                psResponse->ResponseData.token_value.len += pal_Strlen(pcBuf);
                *pui8Data = '\0';
            }
        }
        break;

        //
        // Handle HTTP POST requests from Client.
        //
        case SL_NETAPP_HTTPPOSTTOKENVALUE_EVENT:
        {
            //
            // Get the Post token name to process it.
            //
            pui8Data = psEvent->EventData.httpPostData.token_name.data;

            //
            // Check if Toggle LED status change is reported (Post).
            //
            if(pal_Memcmp(pui8Data, g_pcPOSTToken0,
                          pal_Strlen(g_pcPOSTToken0)) == 0)
            {
                //
                // Toggle global flag that tracks the Toggle LED status and
                // pass it to the function that updates the LED status.
                //
                gui32LEDStatus ^= 1;
                io_set_led((gui32LEDStatus == 1) ? true : false);
            }

            //
            // Check if Animation LED toggle rate change is reported (Post).
            //
            if(pal_Memcmp(pui8Data, g_pcPOSTToken1,
                          pal_Strlen(g_pcPOSTToken1)) == 0)
            {
                //
                // Get the toggle rate passed by the user through the web
                // browser and convert the received string to decimal number.
                // Check if the value sent is within bounds.
                //
                pui8Data = psEvent->EventData.httpPostData.token_value.data;
                ui32Speed = String2Decimal(pui8Data);
                if(ui32Speed > 100)
                {
                    UARTprintf("  Web token Error: Wrong Toggle rate value "
                               "received - using default value.\r\n");
                    ui32Speed = 10;
                }

                //
                // Pass this value to the function that manages toggling of the
                // LED continuously at the requested rate.  Also update the
                // global variable that tracks this rate.
                //
                g_ui32AnimSpeed = ui32Speed;
                io_set_timer(ui32Speed);
            }
        }
        break;

        default:
        break;
    }
}

//*****************************************************************************
//
// This function handles events for IP address acquisition via DHCP indication.
//
// Parameter pNetAppEvent is the event passed to the handler
//
//*****************************************************************************
void
SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{
    if(pNetAppEvent == NULL)
    {
        UARTprintf(" [NETAPP EVENT] NULL Pointer Error \n\r");
        return;
    }

    switch(pNetAppEvent->Event)
    {
        case SL_NETAPP_IPV4_IPACQUIRED_EVENT:
        {
            SET_STATUS_BIT(g_ui32Status, STATUS_BIT_IP_ACQUIRED);

            //
            // Information about the connected AP's IP, gateway, DNS etc
            // will be available in 'SlIpV4AcquiredAsync_t' - Applications
            // can use it if required
            //
            // SlIpV4AcquiredAsync_t * psEventData = NULL;
            // psEventData = &pNetAppEvent->EventData.ipAcquiredV4;
            // <gateway_ip> = psEventData->gateway;
            //
        }
        break;

        case SL_NETAPP_IP_LEASED_EVENT:
        {
            SET_STATUS_BIT(g_ui32Status, STATUS_BIT_IP_LEASED);
        }
        break;

        default:
        {
            UARTprintf(" [NETAPP EVENT] Unexpected event \n\r");
        }
        break;
    }
}

//*****************************************************************************
//
// This function handles general error events indication.
//
//  Parameter pDevEvent is the event passed to the handler
//
//*****************************************************************************
void
SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
    //
    // Most of the general errors are not FATAL are are to be handled
    // appropriately by the application
    //
    UARTprintf(" [GENERAL EVENT] \n\r");
}

//*****************************************************************************
//
// This function handles socket events indication.
//
// Parameter pSock is the event passed to the handler
//
//*****************************************************************************
void
SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
    //
    // This application doesn't work with socket - Hence these
    // events are not handled here
    //
    UARTprintf(" [SOCK EVENT] Unexpected event \n\r");
}

//*****************************************************************************
//
// Set the HTTP port.
//
// This function can be used to change the default port (80) for HTTP request.
//
// Parameter "ui16PortNum" contains the port number to be set.
//
// Note: This function is not used in this application.
//
//*****************************************************************************
int32_t
SetPortNumber(uint16_t ui16PortNum)
{
    _NetAppHttpServerGetSet_port_num_t sPortNum;
    int32_t i32Status = -1;

    sPortNum.port_number = ui16PortNum;

    //
    // Need to restart the server in order for the new port number
    // configuration to take place
    //
    i32Status = sl_NetAppStop(SL_NET_APP_HTTP_SERVER_ID);
    ASSERT_ON_ERROR(i32Status);

    i32Status  = sl_NetAppSet (SL_NET_APP_HTTP_SERVER_ID,
                               NETAPP_SET_GET_HTTP_OPT_PORT_NUMBER,
                               sizeof(_NetAppHttpServerGetSet_port_num_t),
                               (uint8_t *)&sPortNum);
    ASSERT_ON_ERROR(i32Status);

    i32Status = sl_NetAppStart(SL_NET_APP_HTTP_SERVER_ID);
    ASSERT_ON_ERROR(i32Status);

    return SUCCESS;
}

//*****************************************************************************
//
// Enable/Disable the authentication check for http server.  By default
// authentication is disabled.
//
// Parameter ui8Enable - set to false to disable authentication and
//                              true to enable authentication
//
//*****************************************************************************
static int32_t
SetAuthenticationCheck (uint8_t ui8Enable)
{
    _NetAppHttpServerGetSet_auth_enable_t sAuthEnable;
    int32_t i32Status = -1;

    sAuthEnable.auth_enable = ui8Enable;
    i32Status = sl_NetAppSet(SL_NET_APP_HTTP_SERVER_ID,
                             NETAPP_SET_GET_HTTP_OPT_AUTH_CHECK,
                             sizeof(_NetAppHttpServerGetSet_auth_enable_t),
                             (uint8_t *)&sAuthEnable);
    ASSERT_ON_ERROR(i32Status);

    return SUCCESS;
}

//*****************************************************************************
//
// Get the authentication user name.
//
// Parameter      pui8AuthName - Pointer to the string to store authentication
//                               name
//
//*****************************************************************************
static int32_t
GetAuthName (uint8_t * pui8AuthName)
{
    uint8_t ui8Len = MAX_AUTH_NAME_LEN;
    int32_t i32Status = -1;

    i32Status = sl_NetAppGet(SL_NET_APP_HTTP_SERVER_ID,
                             NETAPP_SET_GET_HTTP_OPT_AUTH_NAME, &ui8Len,
                             (uint8_t *) pui8AuthName);
    ASSERT_ON_ERROR(i32Status);

    pui8AuthName[ui8Len] = '\0';

    return SUCCESS;
}

//*****************************************************************************
//
// Get the authentication password.
//
// Parameter pui8AuthPassword - Pointer to the string to store authentication
//                              password
//
//*****************************************************************************
static int32_t
GetAuthPassword (uint8_t * pui8AuthPassword)
{
    uint8_t ui8Len = MAX_AUTH_PASSWORD_LEN;
    int32_t i32Status = -1;

    i32Status = sl_NetAppGet(SL_NET_APP_HTTP_SERVER_ID,
                             NETAPP_SET_GET_HTTP_OPT_AUTH_PASSWORD, &ui8Len,
                             (uint8_t *) pui8AuthPassword);
    ASSERT_ON_ERROR(i32Status);

    pui8AuthPassword[ui8Len] = '\0';

    return SUCCESS;
}

//*****************************************************************************
//
// Get the authentication realm.
//
// Parameter pui8AuthRealm - Pointer to the string to store authentication
//                           realm
//
//*****************************************************************************
static int32_t
GetAuthRealm (uint8_t * pui8AuthRealm)
{
    uint8_t ui8Len = MAX_AUTH_REALM_LEN;
    int32_t i32Status = -1;

    i32Status = sl_NetAppGet(SL_NET_APP_HTTP_SERVER_ID,
                             NETAPP_SET_GET_HTTP_OPT_AUTH_REALM, &ui8Len,
                             (uint8_t *) pui8AuthRealm);
    ASSERT_ON_ERROR(i32Status);

    pui8AuthRealm[ui8Len] = '\0';

    return SUCCESS;
}

//*****************************************************************************
//
// Get the device URN.
//
// Parameter pui8DeviceUrn - Pointer to the string to store device urn
//
//*****************************************************************************
static int32_t
GetDeviceUrn (uint8_t * pui8DeviceUrn)
{
    uint8_t ui8Len = MAX_DEVICE_URN_LEN;
    int32_t i32Status = -1;

    i32Status = sl_NetAppGet(SL_NET_APP_DEVICE_CONFIG_ID,
                             NETAPP_SET_GET_DEV_CONF_OPT_DEVICE_URN, &ui8Len,
                             (uint8_t *) pui8DeviceUrn);
    ASSERT_ON_ERROR(i32Status);

    pui8DeviceUrn[ui8Len] = '\0';

    return SUCCESS;
}

//*****************************************************************************
//
// Get the domain Name.
//
// Parameter pui8DeviceName - Pointer to the string to store domain name
//
//*****************************************************************************
static int32_t
GetDomainName (uint8_t * pui8DeviceName)
{
    uint8_t ui8Len = MAX_DOMAIN_NAME_LEN;
    int32_t i32Status = -1;

    i32Status = sl_NetAppGet(SL_NET_APP_DEVICE_CONFIG_ID,
                             NETAPP_SET_GET_DEV_CONF_OPT_DOMAIN_NAME, &ui8Len,
                             (uint8_t *)pui8DeviceName);
    ASSERT_ON_ERROR(i32Status);

    pui8DeviceName[ui8Len] = '\0';

    return SUCCESS;
}

//*****************************************************************************
//
// This function initializes the global application variables.
//
//*****************************************************************************
void
InitializeAppVariables(void)
{
    g_ui32Status = 0;
    pal_Memset(g_pcAuthName, 0, sizeof(g_pcAuthName));
    pal_Memset(g_pcAuthPass, 0, sizeof(g_pcAuthPass));
    pal_Memset(g_pcAuthRealm, 0, sizeof(g_pcAuthRealm));
    pal_Memset(g_pcDomainName, 0, sizeof(g_pcDomainName));
    pal_Memset(g_pcDeviceURN, 0, sizeof(g_pcDeviceURN));
}

//*****************************************************************************
//
// This function configure the SimpleLink device in its default state. It:
//           - Sets the mode to STATION
//           - Configures connection policy to Auto and AutoSmartConfig
//           - Deletes all the stored profiles
//           - Enables DHCP
//           - Disables Scan policy
//           - Sets Tx power to maximum
//           - Sets power policy to normal
//           - Unregisters mDNS services
//           - Remove all filters
//
// On success, zero is returned. On error, negative value is returned.
//
//*****************************************************************************
static int32_t
ConfigureSimpleLinkToDefaultState(void)
{
    uint8_t ui8Val = 1;
    uint8_t ui8ConfigOpt = 0;
    uint8_t ui8ConfigLen = 0;
    uint8_t ui8Power = 0;
    int32_t i32RetVal = -1;
    int32_t i32Mode = -1;
    SlVersionFull sVer = {0};
    _WlanRxFilterOperationCommandBuff_t sRxFilterIdMask = {0};

    i32Mode = sl_Start(0, 0, 0);
    ASSERT_ON_ERROR(i32Mode);

    //
    // If the device is not in station-mode, try configuring it in station-mode
    //
    if(i32Mode != ROLE_STA)
    {
        if(i32Mode == ROLE_AP)
        {
            //
            // If the device is in AP mode, we need to wait for this event
            // before doing anything.
            //
            while(!IS_IP_ACQUIRED(g_ui32Status))
            {
                _SlNonOsMainLoopTask();
            }
        }

        //
        // Switch to STA role and restart.
        //
        i32RetVal = sl_WlanSetMode(ROLE_STA);
        ASSERT_ON_ERROR(i32RetVal);

        i32RetVal = sl_Stop(SL_STOP_TIMEOUT);
        ASSERT_ON_ERROR(i32RetVal);

        i32RetVal = sl_Start(0, 0, 0);
        ASSERT_ON_ERROR(i32RetVal);

        //
        // Check if the device is in station again.
        //
        if(i32RetVal != ROLE_STA)
        {
            //
            // We don't want to proceed if the device is not coming up in
            // station-mode.
            //
            ASSERT_ON_ERROR(DEVICE_NOT_IN_STATION_MODE);
        }
    }

    //
    // Get the device's version-information.
    //
    ui8ConfigOpt = SL_DEVICE_GENERAL_VERSION;
    ui8ConfigLen = sizeof(sVer);
    i32RetVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &ui8ConfigOpt,
                          &ui8ConfigLen, (uint8_t *)(&sVer));
    ASSERT_ON_ERROR(i32RetVal);

    //
    // Set connection policy to Auto + SmartConfig (Device's default connection
    // policy).
    //
    i32RetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION,
                                 SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);
    ASSERT_ON_ERROR(i32RetVal);

    //
    // Remove all profiles.
    //
    i32RetVal = sl_WlanProfileDel(0xFF);
    ASSERT_ON_ERROR(i32RetVal);

    //
    // Device in station-mode.  Disconnect previous connection if any.  The
    // function returns 0 if 'Disconnected done' and negative number if already
    // disconnected. Wait for 'disconnection' event if 0 is returned and ignore
    // other return-codes.
    //
    i32RetVal = sl_WlanDisconnect();
    if(i32RetVal == 0)
    {
        //
        // Wait till a connection is established.
        //
        while(IS_CONNECTED(g_ui32Status))
        {
            _SlNonOsMainLoopTask();
        }
    }

    //
    // Enable DHCP client.
    //
    i32RetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE, 1, 1, &ui8Val);
    ASSERT_ON_ERROR(i32RetVal);

    //
    // Disable scan.
    //
    ui8ConfigOpt = SL_SCAN_POLICY(0);
    i32RetVal = sl_WlanPolicySet(SL_POLICY_SCAN , ui8ConfigOpt, NULL, 0);
    ASSERT_ON_ERROR(i32RetVal);

    //
    // Set Tx power level for station mode.
    // Number between 0-15, as dB offset from max power - 0 will set maximum
    // power.
    //
    ui8Power = 0;
    i32RetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
                           WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1,
                           (uint8_t *)&ui8Power);
    ASSERT_ON_ERROR(i32RetVal);

    //
    // Set PM policy to normal.
    //
    i32RetVal = sl_WlanPolicySet(SL_POLICY_PM , SL_NORMAL_POLICY, NULL, 0);
    ASSERT_ON_ERROR(i32RetVal);

    //
    // Unregister mDNS services.
    //
    i32RetVal = sl_NetAppMDNSUnRegisterService(0, 0);
    ASSERT_ON_ERROR(i32RetVal);

    //
    // Remove  all 64 filters (8*8).
    //
    pal_Memset(sRxFilterIdMask.FilterIdMask, 0xFF, 8);
    i32RetVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER,
                                   (uint8_t *)&sRxFilterIdMask,
                                   sizeof(_WlanRxFilterOperationCommandBuff_t));
    ASSERT_ON_ERROR(i32RetVal);

    i32RetVal = sl_Stop(SL_STOP_TIMEOUT);
    ASSERT_ON_ERROR(i32RetVal);

    InitializeAppVariables();

    //
    //Success.
    //
    return i32RetVal;
}

//*****************************************************************************
//
// This function uploads web pages to the Serial Flash on CC3100 BoosterPack.
//
//*****************************************************************************
void
UploadWebPages(void)
{
    uint32_t ui32Token = 0;
    int32_t i32FileHandle = -1;
    int32_t i32RetVal = -1;
    char pcFileName[MAX_FILE_NAME_SIZE];
    struct fsdata_file * psCurrentFile;
    uint32_t ui32PrefixLen = (sizeof(FILE_NAME_PREFIX) - 1);

    
    UARTprintf("\r\n    Uploading webpages to Serial Flash on CC3100 BP.\n\r");

    //
    // Pre-populate the filename with the pre-defined prefix.
    //
    strncpy(pcFileName, FILE_NAME_PREFIX, ui32PrefixLen);

    //
    // Get the root of the file system.
    //
    psCurrentFile = (struct fsdata_file *)(FS_ROOT);

    //
    // Upload all files to the serial flash.
    //
    while(psCurrentFile != NULL)
    {
        //
        // Get the name of the file to write.
        //
        if(ustrncmp((const char *)(psCurrentFile->name), "/index.htm", 10) ==
           0)
        {
            //
            // CC3100 will look for "index.html" or "main.html" file in the
            // serial flash.  If it does not find these files, it will use the
            // default file.
            //
            ustrncpy((pcFileName + ui32PrefixLen), "/index.html",
                     (MAX_FILE_NAME_SIZE - ui32PrefixLen));
        }
        else
        {
            ustrncpy((pcFileName + ui32PrefixLen),
                     (const char *)(psCurrentFile->name),
                     (MAX_FILE_NAME_SIZE - ui32PrefixLen));
        }


        //
        // Open one file at a time on the Serial Flash to write.
        //
        UARTprintf("\tOpening file %s\n\r", pcFileName);
        i32RetVal = sl_FsOpen((uint8_t *)pcFileName, FS_MODE_OPEN_WRITE,
                              &ui32Token, &i32FileHandle);
        if(i32RetVal < 0)
        {
            //
            // File Doesn't exit.  Create a new file.
            //
            i32RetVal = sl_FsOpen((uint8_t *)pcFileName,
                                  FS_MODE_OPEN_CREATE(psCurrentFile->len,
                                                      _FS_FILE_OPEN_FLAG_COMMIT |
                                                      _FS_FILE_PUBLIC_WRITE),
                                  &ui32Token, &i32FileHandle);
            if(i32RetVal < 0)
            {
                UARTprintf(" Error in creating the file \n\r");
                while(1)
                {
                }
            }
        }

        //
        // Write the data in the webpage buffer to the file on serial flash.
        //
        UARTprintf("\t\tWriting %d bytes\n\r", psCurrentFile->len);

        i32RetVal = sl_FsWrite(i32FileHandle, 0, (uint8_t *)psCurrentFile->data,
                               psCurrentFile->len);
        if (i32RetVal < 0)
        {
            UARTprintf(" Error in writing the file \n\r");

            i32RetVal = sl_FsClose(i32FileHandle, 0, 0, 0);
            i32RetVal = sl_FsDel((uint8_t *)pcFileName, ui32Token);

            while(1)
            {
            }
        }

        //
        // Close the user file
        //
        i32RetVal = sl_FsClose(i32FileHandle, 0, 0, 0);
        if (i32RetVal < 0)
        {
            UARTprintf(" Error in closing the file \n\r");
            i32RetVal = sl_FsDel((uint8_t *)pcFileName, ui32Token);

            while(1)
            {
            }
        }

        //
        // Get the address to the next file buffer.
        //
        psCurrentFile = (struct fsdata_file *)(psCurrentFile->next);
    }

    UARTprintf("\r\n    Device successfully wrote all the webpages to "
               "serial-flash\n\n\r");
}

//*****************************************************************************
//
// This example converts the CC3100 BoosterPack as a Webserver.
//
//*****************************************************************************
int
main(void)
{
    uint8_t ui8SecType = 0;
    int32_t i32RetVal = -1;
    int32_t i32Mode = ROLE_STA;
    uint32_t ui32Upload = 0;

    //
    // Initialize the system clock.
    //
    initClk();

    //
    // Configure the MCU pins.
    //
    PinoutSet(true, false);

    //
    // Configure debug port.
    //
    UARTStdioConfig(0, 115200, g_SysClock);

    //
    // Clear the terminal and print a banner.
    //
    UARTprintf("\033[2J\033[H");
    UARTprintf("HTTP Server Application on CC3100\n\n\r");
    UARTprintf("Press and hold USR_SW1 during reset to upload webpages to "
               "CC3100\n\rBoosterpack's Serial Flash.\n\r");

    //
    // Initialize the user switches.
    //
    ButtonsInit();

    //
    // Check if USR_SW1 is pressed.
    //
    if((ButtonsPoll(NULL, NULL) & USR_SW1) == USR_SW1)
    {
        //
        // Yes - User wants to upload webpages.  Set the necessary flag.
        //
        ui32Upload = 1;
        UARTprintf("Webpages will be uploaded to CC3100 Boosterpack's Serial "
                   "Flash.\n\n\r");
    }
    else
    {
        //
        // Yes - User wants to use the existing webpages.
        //
        ui32Upload = 0;
        UARTprintf("Existing webpages on CC3100 Boosterpack's Serial Flash "
                   "will be used.\n\n\r");
    }

    //
    // Initialize the application variables.
    //
    InitializeAppVariables();

    //
    // Initialize the GPIOs that control the Toggle LED and Animation LED.
    //
    io_init();

    //
    // Following function configures the device to default state by cleaning
    // the persistent settings stored in NVMEM (viz. connection profiles &
    // policies, power policy etc).
    //
    // Applications may choose to skip this step if the developer is sure that
    // the device is in its default state at start of application.
    //
    // Note that all profiles and persistent settings that were done on the
    // device will be lost.
    //
    i32RetVal = ConfigureSimpleLinkToDefaultState();
    if(i32RetVal < 0)
    {
        if (DEVICE_NOT_IN_STATION_MODE == i32RetVal)
        {
            UARTprintf("Failed to configure CC3100 in its default state.\n\r");
        }
        while(1)
        {
        }
    }
    UARTprintf("CC3100 is configured in default state.\n\n\r");

    //
    // Assumption is that the device is configured in station mode already
    // and it is in its default state.
    //
    i32Mode = sl_Start(0, 0, 0);
    if(i32Mode < 0)
    {
        while(1)
        {
        }
    }
    else
    {
        if (ROLE_AP == i32Mode)
        {
            //
            // If the device is in AP mode, we need to wait for this event
            // before doing anything.
            //
            while(!IS_IP_ACQUIRED(g_ui32Status))
            {
                _SlNonOsMainLoopTask();
            }
        }
        else
        {
            //
            // Configure CC3100 to start in AP mode.
            //
            i32RetVal = sl_WlanSetMode(ROLE_AP);
            if(i32RetVal < 0)
            {
                while(1)
                {
                }
            }
        }
    }

    //
    // Configure AP mode without security.
    //
    i32RetVal = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_SSID,
               pal_Strlen(SSID_AP_MODE), (uint8_t *)SSID_AP_MODE);
    if(i32RetVal < 0)
    {
        while(1)
        {
        }
    }

    //
    // Configure the Security parameter in the AP mode.
    //
    ui8SecType = SEC_TYPE_AP_MODE;
    i32RetVal = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_SECURITY_TYPE, 1,
                           (uint8_t *)&ui8SecType);
    if(i32RetVal < 0)
    {
        while(1)
        {
        }
    }

    //
    // Configure the password for the AP mode.
    //
    i32RetVal = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_PASSWORD,
                           pal_Strlen(PASSWORD_AP_MODE),
                           (uint8_t *)PASSWORD_AP_MODE);
    if(i32RetVal < 0)
    {
        while(1)
        {
        }
    }

    //
    // Restart the CC3100.
    //
    i32RetVal = sl_Stop(SL_STOP_TIMEOUT);
    if(i32RetVal < 0)
    {
        while(1)
        {
        }
    }

    g_ui32Status = 0;

    //
    // Start CC3100 and check if it is configured in AP mode.  Inform the user
    // if CC3100 has entered AP mode.
    //
    i32Mode = sl_Start(0, 0, 0);
    if (i32Mode == ROLE_AP)
    {
        //
        // If the device is in AP mode, we need to wait for this event before
        // doing anything.
        //
        while(!IS_IP_ACQUIRED(g_ui32Status))
        {
            _SlNonOsMainLoopTask();
        }
    }
    else
    {
        //
        // Error.  Inform user and loop forever for ever.
        //
        UARTprintf("CC3100 couldn't be configured in AP mode \n\r");
        while(1)
        {
        }
    }
    UARTprintf("CC3100 is configured in AP mode\n\r");

    //
    // Check if user wants to write webpages to CC3100 Serial Flash.
    //
    if(ui32Upload == 1)
    {
        //
        // Yes - Upload webpages to the serial Flash.
        //
        UploadWebPages();
    }

    //
    // Wait for the client to connect and display the status once connected.
    //
    UARTprintf("Waiting for client to connect\n\r");
    while((!IS_IP_LEASED(g_ui32Status)) || (!IS_STA_CONNECTED(g_ui32Status)))
    {
        _SlNonOsMainLoopTask();
    }
    UARTprintf("Client connected\n\r");

    //
    // Enable the HTTP Authentication.  If an error is returned then loop
    // forever.
    //
    i32RetVal = SetAuthenticationCheck(TRUE);
    if(i32RetVal < 0)
    {
        while(1)
        {
        }
    }

    //
    // Get authentication parameters and display them.  If an error is returned
    // then loop forever.
    //
    i32RetVal = GetAuthName((_u8*)g_pcAuthName);
    if(i32RetVal < 0)
    {
        while(1)
        {
        }
    }
    i32RetVal = GetAuthPassword((_u8*)g_pcAuthPass);
    if(i32RetVal < 0)
    {
        while(1)
        {
        }
    }
    i32RetVal = GetAuthRealm((_u8*)g_pcAuthRealm);
    if(i32RetVal < 0)
    {
        while(1)
        {
        }
    }
    UARTprintf("\n\rAuthentication parameters:\n\r");
    UARTprintf("    Name = %s\n\r", g_pcAuthName);
    UARTprintf("    Password = %s\n\r", g_pcAuthPass);
    UARTprintf("    Realm = %s\n\n\r", g_pcAuthRealm);

    //
    // Get the domain name and display it. If an error is returned then loop
    // forever.
    //
    i32RetVal = GetDomainName((_u8*)g_pcDomainName);
    if(i32RetVal < 0)
    {
        while(1)
        {
        }
    }
    UARTprintf("Domain name = %s\n\r", g_pcDomainName);

    //
    // Get URN and display it. If an error is returned then loop forever.
    //
    i32RetVal = GetDeviceUrn((_u8*)g_pcDeviceURN);
    if(i32RetVal < 0)
    {
        while(1)
        {
        }
    }
    UARTprintf("Device URN = %s\n\n\r", g_pcDeviceURN);

    //
    // Process the async events from the NWP
    //
    while(1)
    {
        _SlNonOsMainLoopTask();
    }
}
#ifndef MCP_NETWORK_H
#define MCP_NETWORK_H

#include "freertos/FreeRTOS.h"

#include "mcp_wifi.h"
#include "mcp_ethernet.h"

class Network
{ 
    private:
        char* wifi_ssid;
        char* wifi_pass;
        bool use_wifi;
        bool connected;
    public:
        Network();

        uint8_t setup(char* wifi_ssid, char* wifi_pass);
        uint8_t setup();
        uint8_t init();
        uint8_t start();
        uint8_t restart();
        uint8_t stop();
        uint8_t status();
        bool isConnected();
};

#endif

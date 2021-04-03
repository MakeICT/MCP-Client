#include "mcp_network.h"

Network::Network()
{

}

uint8_t Network::setup(char* ssid, char* pass)
{
    this->wifi_ssid = ssid;
    this->wifi_pass = pass;
    this->use_wifi = true;

    return 0;
}

uint8_t Network::setup()
{
    this->use_wifi = false;

    return 0;
}

uint8_t Network::init()
{
    uint8_t ret = 0;

    if(use_wifi){
        ret = wifiInit(wifi_ssid, wifi_pass);
    }
    else {
        ret = ethernet_init();
    }

    return ret;
}

uint8_t Network::start()
{
    uint8_t ret = 0;

    if(use_wifi){
        ret = wifiStart();
    }
    else {
        ret = ethernet_start();
    }
    this->connected = true;

    return ret;
}

uint8_t Network::restart()
{
    uint8_t ret = 0;

    if(use_wifi){
        ret = wifiRestart();
    }
    else {
        ret = ethernet_reset();
    }

    return ret;
}

uint8_t Network::stop()
{
    uint8_t ret = 0;

    if(use_wifi){
        ret = wifiStop();
    }
    else {
        ret = ethernet_stop();
    }

    this->connected = false;

    return ret;
}

uint8_t Network::status()
{
    uint8_t ret = 0;

    return ret;
}

bool Network::isConnected()
{
    if(use_wifi){
        return wifiIsConnected();
    }
    else {
        return ethernetIsConnected();
    }
}
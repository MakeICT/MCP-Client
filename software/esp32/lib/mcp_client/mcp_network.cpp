#include "mcp_network.h"

Network::Network()
{

}

uint8_t Network::setup(const char* ssid, const char* pass)
{
    this->wifi_ssid = (char*) ssid;
    this->wifi_pass = (char*) pass;
    this->use_wifi = true;

    return 0;
}

uint8_t Network::setup()
{
    this->use_wifi = false;

    return 0;
}

uint8_t Network::setHostname(char* hostname)
{
    if(this->use_wifi) {
        ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, hostname));
    }
    else {
        ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_ETH, hostname));
    }

    return 0;
}

uint8_t Network::init()
{
    uint8_t ret = 0;

    if(use_wifi){
        ret = wifiInit(wifi_ssid, wifi_pass);
        #ifdef CONFIG_STATIC_IP
            wifi_set_static_ip(CONFIG_CLIENT_IP, CONFIG_GATEWAY_IP, CONFIG_NETMASK);
        #endif

        // disable wifi power saving to prevent GPIO 36 and 39 from constantly creating interrupts
        // https://github.com/espressif/esp-idf/issues/1096
        // https://github.com/espressif/esp-idf/issues/4585
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    }
    else {
        ret = ethernet_init();
        #ifdef CONFIG_STATIC_IP
            ethernet_set_static_ip(CONFIG_CLIENT_IP, CONFIG_GATEWAY_IP, CONFIG_NETMASK);
        #endif
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
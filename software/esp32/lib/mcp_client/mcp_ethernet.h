/* Ethernet Basic Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef MCP_ETHERNET_H
#define MCP_ETHERNET_H

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "tcpip_adapter.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

static const char *ETH_TAG = "MCP_ETHERNET";

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int ETH_CONNECTED_BIT = BIT0;
const int ETH_ADDRESSED_BIT = BIT1;

// /** Event handler for Ethernet events */
// static void eth_event_handler(void *arg, esp_event_base_t event_base,
//                               int32_t event_id, void *event_data);
// /** Event handler for IP_EVENT_ETH_GOT_IP */
// static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
//                                  int32_t event_id, void *event_data);
int ethernet_init();
int ethernet_reset();
int ethernet_start();
int ethernet_stop();
bool ethernetIsConnected();

#endif 
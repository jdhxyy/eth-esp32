// Copyright 2023-2023 The jdh99 Authors. All rights reserved.
// esp32的以太网驱动
// Authors: jdh99 <jdh821@163.com>

#include "eth.h"
#include "lagan.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#define TAG "eth"

#define CONFIG_EXAMPLE_ETH_PHY_ADDR 0
#define CONFIG_EXAMPLE_ETH_PHY_RST_GPIO 5
#define CONFIG_EXAMPLE_ETH_MDC_GPIO 23
#define CONFIG_EXAMPLE_ETH_MDIO_GPIO 18

static bool gIsConnect = false;
static EthConnectInfo gConnectInfo;

static void eventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void gotIPEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

// EthLoad 模块载入
// 载入之前需初始化nvs_flash_init,esp_netif_init,esp_event_loop_create_default
// 如果dhcp使能,则后续的本机ip,掩码,网关都无效可填0
bool EthLoad(bool enableDhcp, uint32_t ip, uint32_t mask, uint32_t gateway) {
    // Create new default instance of esp-netif for Ethernet
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);

    // Init MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    phy_config.phy_addr = CONFIG_EXAMPLE_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_EXAMPLE_ETH_PHY_RST_GPIO;
    mac_config.smi_mdc_gpio_num = CONFIG_EXAMPLE_ETH_MDC_GPIO;
    mac_config.smi_mdio_gpio_num = CONFIG_EXAMPLE_ETH_MDIO_GPIO;
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    if (esp_eth_driver_install(&config, &eth_handle) != ESP_OK) {
        return false;
    }

    // attach Ethernet driver to TCP/IP stack
    if (esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)) != ESP_OK) {
        return false;
    }
    //CONFIG_EXAMPLE_USE_INTERNAL_ETHERNET

    // Register user defined event handers
    if (esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eventHandler, NULL) != ESP_OK) {
        return false;
    }
    if (esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &gotIPEventHandler, NULL) != ESP_OK) {
        return false;
    }

    // 获取MAC
    memset(gConnectInfo.Mac, 0, sizeof(gConnectInfo.Mac));
    esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, gConnectInfo.Mac);
    LI(TAG, "Ethernet MAC Address: %02x:%02x:%02x:%02x:%02x:%02x", gConnectInfo.Mac[0], gConnectInfo.Mac[1], 
        gConnectInfo.Mac[2], gConnectInfo.Mac[3], gConnectInfo.Mac[4], gConnectInfo.Mac[5]);

    if (enableDhcp == false) {
        LI(TAG, "dhcpc_stop");
        if (esp_netif_dhcpc_stop(eth_netif) != ESP_OK) {
            return false;
        }

        esp_netif_ip_info_t info_t;
        memset(&info_t, 0, sizeof(esp_netif_ip_info_t));
        info_t.ip.addr = htonl(ip);
        info_t.netmask.addr = htonl(mask);
        info_t.gw.addr = htonl(gateway);
        LI(TAG, "static ip");
        if (esp_netif_set_ip_info(eth_netif, &info_t) != ESP_OK) {
            return false;
        }
    }
   
    // start Ethernet driver state machine
    if (esp_eth_start(eth_handle) != ESP_OK) {
        return false;
    }
    LI(TAG, "load success");
    return true;
}

static void eventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    uint8_t mac_addr[6] = {0};
    // we can get the ethernet driver handle from event data
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        LI(TAG, "Ethernet Link Up");
        LI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        LI(TAG, "Ethernet Link Down");
        gIsConnect = false;
        break;
    case ETHERNET_EVENT_START:
        LI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        LI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

static void gotIPEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    LI(TAG, "Ethernet Got IP Address");
    LI(TAG, "~~~~~~~~~~~");
    LI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    LI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    LI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    LI(TAG, "~~~~~~~~~~~");

    gConnectInfo.IP = ntohl(ip_info->ip.addr);
    gConnectInfo.Gateway = ntohl(ip_info->gw.addr);
    gIsConnect = true;
}

// EthIsConnect 是否已连接
bool EthIsConnect(void) {
    return gIsConnect;
}

// EthConnectInfo 读取当前已连接的信息
// 如果未连接则返回NULL
EthConnectInfo* EthGetConnectInfo(void) {
    if (gIsConnect == false) {
        return NULL;
    }
    return &gConnectInfo;
}



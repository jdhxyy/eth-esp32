// Copyright 2023-2023 The jdh99 Authors. All rights reserved.
// esp32的以太网驱动
// Authors: jdh99 <jdh821@163.com>

#ifndef ETH_H
#define ETH_H

#include <stdbool.h>
#include <stdint.h>

// EthConnectInfo eth连接信息
typedef struct {
    uint32_t IP;
    uint32_t Gateway;
    uint8_t Mac[6];
} EthConnectInfo;

// EthLoad 模块载入
// 载入之前需初始化nvs_flash_init,esp_netif_init,esp_event_loop_create_default
// 如果dhcp使能,则后续的本机ip,掩码,网关都无效可填0
bool EthLoad(bool enableDhcp, uint32_t ip, uint32_t mask, uint32_t gateway);

// EthIsConnect 是否已连接
bool EthIsConnect(void);

// EthConnectInfo 读取当前已连接的信息
// 如果未连接则返回NULL
EthConnectInfo* EthGetConnectInfo(void);

#endif

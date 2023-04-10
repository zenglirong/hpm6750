/*
 * Copyright (c) 2021 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/*  HPM example includes. */
#include <stdio.h>
#include "common.h"
#include "lwip.h"
#include "lwip/init.h"
#include "lwip/tcpip.h"
#include "netconf.h"
#include "tcp_echo.h"
#include "snmp.h"

#include "hpm_gpio_drv.h"

#include "mdns_app.h"
#include "uart_app.h"
#include "netconn_app.h"

#include "lwipopts.h"
/*--------------- Tasks Priority -------------*/
#define LED_TASK_PRIO    (tskIDLE_PRIORITY + 1)
#define MAIN_TASK_PRIO   (tskIDLE_PRIORITY + 2)
#define DHCP_TASK_PRIO   (tskIDLE_PRIORITY + 4)

ATTR_PLACE_AT_NONCACHEABLE_WITH_ALIGNMENT(ENET_SOC_DESC_ADDR_ALIGNMENT)
__RW enet_rx_desc_t dma_rx_desc_tab[ENET_RX_BUFF_COUNT]; /* Ethernet Rx DMA Descriptor */

ATTR_PLACE_AT_NONCACHEABLE_WITH_ALIGNMENT(ENET_SOC_DESC_ADDR_ALIGNMENT)
__RW enet_tx_desc_t dma_tx_desc_tab[ENET_TX_BUFF_COUNT]; /* Ethernet Tx DMA Descriptor */

ATTR_PLACE_AT_NONCACHEABLE_WITH_ALIGNMENT(ENET_SOC_BUFF_ADDR_ALIGNMENT)
__RW uint8_t rx_buff[ENET_RX_BUFF_COUNT][ENET_RX_BUFF_SIZE]; /* Ethernet Receive Buffer */

ATTR_PLACE_AT_NONCACHEABLE_WITH_ALIGNMENT(ENET_SOC_BUFF_ADDR_ALIGNMENT)
__RW uint8_t tx_buff[ENET_TX_BUFF_COUNT][ENET_TX_BUFF_SIZE]; /* Ethernet Transmit Buffer */

enet_desc_t desc;
uint8_t mac[ENET_MAC];

void Main_task(void *pvParameters);
void Led_task(void *pvParameters);

void bsp_init(void)
{
    /* Initialize BSP */
    board_init();

    /* Initialize GPIOs */
    board_init_enet_pins(ENET);

    /* Reset an enet PHY */
    board_reset_enet_phy(ENET);

    printf("This is an ethernet demo: TCP Echo on FreeRTOS\n");
    printf("LwIP Version: %s\n", LWIP_VERSION_STRING);

    #if RGMII
    /* Set RGMII clock delay */
    board_init_enet_rgmii_clock_delay(ENET);
    #else
    /* Set RMII reference clock */
    board_init_enet_rmii_reference_clock(ENET, BOARD_ENET_RMII_INT_REF_CLK);
    printf("Reference Clock: %s\n", BOARD_ENET_RMII_INT_REF_CLK ? "Internal Clock" : "External Clock");
    #endif
}

hpm_stat_t enet_init(ENET_Type *ptr)
{
    enet_int_config_t int_config = {.int_enable = 0, .int_mask = 0};
    enet_mac_config_t enet_config;

    #if RGMII
        #if __USE_DP83867
        dp83867_config_t phy_config;
        #else
        rtl8211_config_t phy_config;
        #endif
    #else
        #if __USE_DP83848
        dp83848_config_t phy_config;
        #else
        rtl8201_config_t phy_config;
        #endif
    #endif

    /* Initialize td, rd and the corresponding buffers */
    memset((uint8_t *)dma_tx_desc_tab, 0x00, sizeof(dma_tx_desc_tab));
    memset((uint8_t *)dma_rx_desc_tab, 0x00, sizeof(dma_rx_desc_tab));
    memset((uint8_t *)rx_buff, 0x00, sizeof(rx_buff));
    memset((uint8_t *)tx_buff, 0x00, sizeof(tx_buff));

    desc.tx_desc_list_head = (enet_tx_desc_t *)core_local_mem_to_sys_address(BOARD_RUNNING_CORE, (uint32_t)dma_tx_desc_tab);
    desc.rx_desc_list_head = (enet_rx_desc_t *)core_local_mem_to_sys_address(BOARD_RUNNING_CORE, (uint32_t)dma_rx_desc_tab);

    desc.tx_buff_cfg.buffer = core_local_mem_to_sys_address(BOARD_RUNNING_CORE, (uint32_t)tx_buff);
    desc.tx_buff_cfg.count = ENET_TX_BUFF_COUNT;
    desc.tx_buff_cfg.size = ENET_TX_BUFF_SIZE;

    desc.rx_buff_cfg.buffer = core_local_mem_to_sys_address(BOARD_RUNNING_CORE, (uint32_t)rx_buff);
    desc.rx_buff_cfg.count = ENET_RX_BUFF_COUNT;
    desc.rx_buff_cfg.size = ENET_RX_BUFF_SIZE;

    /* Get MAC address */
    enet_get_mac_address(mac);

    /* Set mac0 address */
    enet_config.mac_addr_high[0] = mac[5] << 8 | mac[4];
    enet_config.mac_addr_low[0]  = mac[3] << 24 | mac[2] << 16 | mac[1] << 8 | mac[0];
    enet_config.valid_max_count  = 1;

    /* Set DMA PBL */
    enet_config.dma_pbl = board_enet_get_dma_pbl(ENET);

    /* Enable Enet IRQ */
    board_enet_enable_irq(ENET);

    /* Set the interrupt enable mask */
    int_config.int_enable = enet_normal_int_sum_en    /* Enable normal interrupt summary */
                          | enet_receive_int_en;      /* Enable receive interrupt */

    int_config.int_mask = enet_rgsmii_int_mask | ENET_INTR_MASK_LPIIM_MASK; /* Disable RGSMII interrupt */

    /* Initialize enet controller */
    enet_controller_init(ptr, ENET_INF_TYPE, &desc, &enet_config, &int_config);

    /* Disable LPI interrupt */
    enet_disable_lpi_interrupt(ENET);

    /* Initialize phy */
    #if RGMII
        #if __USE_DP83867
        dp83867_reset(ptr);
        #ifdef __DISABLE_AUTO_NEGO
        dp83867_set_mdi_crossover_mode(ENET, enet_phy_mdi_crossover_manual_mdix);
        #endif
        dp83867_basic_mode_default_config(ptr, &phy_config);
        if (dp83867_basic_mode_init(ptr, &phy_config) == true) {
        #else
        rtl8211_reset(ptr);
        rtl8211_basic_mode_default_config(ptr, &phy_config);
        if (rtl8211_basic_mode_init(ptr, &phy_config) == true) {
        #endif
    #else
        #if __USE_DP83848
        dp83848_reset(ptr);
        dp83848_basic_mode_default_config(ptr, &phy_config);
        if (dp83848_basic_mode_init(ptr, &phy_config) == true) {
        #else
        rtl8201_reset(ptr);
        rtl8201_basic_mode_default_config(ptr, &phy_config);
        if (rtl8201_basic_mode_init(ptr, &phy_config) == true) { 
        #endif
    #endif
            printf("Enet phy init passes !\n");
            return status_success;
        } else {
            printf("Enet phy init fails !\n");
            return status_fail;
        }

        printf("164 intr_mask: %08x\n", ptr->INTR_MASK);
}

int main(void)
{
    xTaskCreate(Main_task, "Main", configMINIMAL_STACK_SIZE * 2, NULL, MAIN_TASK_PRIO, NULL);

    xTaskCreate(Led_task, "Led_task", configMINIMAL_STACK_SIZE, NULL, LED_TASK_PRIO, NULL);

    /* Start scheduler */
    vTaskStartScheduler();

    /* We should never get here as control is now taken by the scheduler */
    for ( ;; ) {
        
    }
}

void uart_recv_data(uint8_t *data, int len)
{
    netconn_tcp_send_data(data, len);
    uart_send_bytes(data, len);
}

void netconn_recv_data(uint8_t *data, int len)
{
    netconn_tcp_send_data(data, len);
    uart_send_bytes(data, len);
}

void Main_task(void *pvParameters)
{
    /* Initialize bsp */
    bsp_init();

    /* Initialize MAC and DMA */
    enet_init(ENET);

    /* Initialize LwIP stack */
    LwIP_Init();

#ifdef USE_DHCP
    /* Start DHCP Client */
    xTaskCreate(LwIP_DHCP_task, "DHCP", configMINIMAL_STACK_SIZE * 2, NULL, DHCP_TASK_PRIO, NULL);
#endif
    
    vTaskDelay(3000);
    // uart 
    uart_func_init(uart_recv_data);

    // netconn tcp
    netconn_init(netconn_recv_data);
    
    mdns_init();

    for ( ;; ) {
        vTaskDelete(NULL);
    }
}

void Led_task(void *pvParameters)
{
#ifdef BOARD_LED_GPIO_CTRL
    gpio_set_pin_output(BOARD_LED_GPIO_CTRL, BOARD_LED_GPIO_INDEX, BOARD_LED_GPIO_PIN);
    gpio_write_pin(BOARD_LED_GPIO_CTRL, BOARD_LED_GPIO_INDEX, BOARD_LED_GPIO_PIN, BOARD_LED_ON_LEVEL);
#endif

    for (;;) {
#ifdef BOARD_LED_GPIO_CTRL
        gpio_toggle_pin(BOARD_LED_GPIO_CTRL, BOARD_LED_GPIO_INDEX, BOARD_LED_GPIO_PIN);
#else
        printf("Toggle the flag.\n");
#endif
        vTaskDelay(500);
    }
}

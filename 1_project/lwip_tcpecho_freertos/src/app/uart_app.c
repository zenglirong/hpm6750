/*
 * Copyright (c) 2021 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "board.h"
#include "hpm_clock_drv.h"
#include "hpm_uart_drv.h"
#include "uart_app.h"

#include "lwip/sys.h"       // 调用线程需要用到
#include "sys_arch.h"

#include "FreeRTOS.h"
#include "task.h"

#ifndef CONFIG_UART_FIFO_MODE
#define CONFIG_UART_FIFO_MODE 1
#endif

#ifndef TEST_UART
#define TEST_UART BOARD_APP_UART_BASE
#define TEST_UART_IRQ BOARD_APP_UART_IRQ
#ifdef BOARD_APP_UART_SRC_FREQ
#define TEST_UART_SRC_FREQ BOARD_APP_UART_SRC_FREQ
#elif defined(BOARD_APP_UART_CLK_NAME)
#define TEST_UART_CLK_NAME BOARD_APP_UART_CLK_NAME
#else
#error unknow UART source frequency
#endif
#endif

uint8_t buff_index;
int data_count;
uint8_t data_buff[UART_SOC_FIFO_SIZE];

uart_recv_fn g_uart_recv = NULL;

char uart_data_update = 0;
char uart_delay_update = 0;

void uart_isr(void)
{
    uint8_t c;
    if (uart_get_irq_id(TEST_UART) & uart_intr_id_rx_data_avail) {
        if (status_success != uart_receive_byte(TEST_UART, &c)) {
            //while (1) { }
        }
        data_buff[buff_index] = c;
        buff_index++;
        uart_delay_update = 20;
        if (buff_index >= data_count) {
            //uart_disable_irq(TEST_UART, uart_intr_rx_data_avail_or_timeout);
            //uart_enable_irq(TEST_UART, uart_intr_tx_slot_avail);
            data_buff[buff_index] = 0;
            uart_data_update = 1;
            buff_index = 127;
        }
    }
    if (uart_get_irq_id(TEST_UART) & uart_intr_id_tx_slot_avail) {
        for (uint8_t i = 0; i < data_count; i++) {
            if (status_success != uart_send_byte(TEST_UART, data_buff[i])) {
                //while (1) { }
            }
        }

        buff_index = 0;
        uart_flush(TEST_UART);
        uart_disable_irq(TEST_UART, uart_intr_tx_slot_avail);
        uart_enable_irq(TEST_UART, uart_intr_rx_data_avail_or_timeout);
    }
}

void uart_send_string(char *str)
{
    while (*(str) != 0) {
        if (status_success != uart_send_byte(TEST_UART, *str)) {
            //while (1) {}
        }
        str++;
    }
}

void uart_send_bytes(uint8_t * str, int len)
{
    for (int i=0; i<len; i++) {
        if (status_success != uart_send_byte(TEST_UART, str[i])) {
            //while (1) {}
        }
    }
}

SDK_DECLARE_EXT_ISR_M(TEST_UART_IRQ, uart_isr)

static void  uart_thread(void *arg)
{
    hpm_stat_t stat;
    uint8_t fifo_size;
    buff_index = 0;

    board_init();
    board_init_uart(TEST_UART);

    uart_config_t config = {0};
    uart_default_config(TEST_UART, &config);
#if CONFIG_UART_FIFO_MODE
    config.fifo_enable = true;
#else
    config.fifo_enable = false;
#endif
#ifdef TEST_UART_SRC_FREQ
    config.src_freq_in_hz = TEST_UART_SRC_FREQ;
#else
    config.src_freq_in_hz = clock_get_frequency(TEST_UART_CLK_NAME);
#endif

    printf("uart driver example\n");
    if (!config.fifo_enable) {
        data_count = 1;
        printf("non-fifo mode\n");
        printf("uart will send back received characters, echo one by one\n");
    } else {
        fifo_size = uart_get_fifo_size(TEST_UART);
        data_count = fifo_size;
        printf("fifo mode\n");
        printf("uart will send back received characters, echo every %d bytes\n", data_count);
    }

    stat = uart_init(TEST_UART, &config);
    if (stat != status_success) {
        /* uart failed to be initialized */
        printf("failed to initialize uart\n");
        while(1);
    }

    uart_enable_irq(TEST_UART, uart_intr_rx_data_avail_or_timeout);
    intc_m_enable_irq_with_priority(TEST_UART_IRQ, 1);

    while(1)
    {
        //__asm("wfi;\n");
        //vTaskDelete(NULL); 
        vTaskDelay(1);

        if (uart_data_update)
        {
            uart_data_update = 0;
            g_uart_recv(data_buff, buff_index);
            buff_index = 0;
        }

        if (uart_delay_update)
        {
            if (!(--uart_delay_update))
            {
                uart_data_update = 1;
            }
        }
    }
}


void uart_func_init(uart_recv_fn recv)
{
    g_uart_recv = recv;
    sys_thread_new("uart_thread", uart_thread, NULL, 1024*8, 1);
}
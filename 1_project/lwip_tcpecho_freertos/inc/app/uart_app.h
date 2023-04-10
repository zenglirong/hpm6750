/*
 * Copyright (c) 2021 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#ifndef UART_APP_H
#define UART_APP_H

#include "stdint.h"

typedef void (*uart_recv_fn)(uint8_t *data, int len);

void uart_func_init(uart_recv_fn recv);
void uart_send_string(char *str);
void uart_send_bytes(uint8_t * str, int len);

#endif /* TCP_ECHO_H */
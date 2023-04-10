
#ifndef NETCONN_APP_H
#define NETCONN_APP_H

#include <stdint.h>

typedef void (*netconn_recv_fn)(uint8_t *data, int len);
void netconn_init(netconn_recv_fn recv);
void netconn_tcp_send_data(uint8_t *data, int len);

#endif
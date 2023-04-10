#include "netconn_app.h"

#include "lwip/api.h"       // netconn api

#include "lwip/sys.h"       // 调用线程需要用到
#include "sys_arch.h"

#include "FreeRTOS.h"       // 线程删除和延时用
#include "task.h"

netconn_recv_fn g_netconn_recv;

struct netconn *g_newconn = NULL;

#include "stdio.h"
static void netconn_tcp_server(void *arg)
{
  struct netconn *conn, *newconn;
  err_t err;
  LWIP_UNUSED_ARG(arg);
  struct netbuf *buf;
  /* Bind to SNMP port with default IP address */
#if LWIP_IPV6
  conn = netconn_new(NETCONN_UDP_IPV6);
  netconn_bind(conn, IP6_ADDR_ANY, LWIP_IANA_PORT_SNMP);
#else /* LWIP_IPV6 */
  conn = netconn_new(NETCONN_TCP);
  netconn_bind(conn, IP_ADDR_ANY, 5001);//LWIP_IANA_PORT_SNMP  c901a8c0

#endif /* LWIP_IPV6 */
  LWIP_ERROR("snmp_netconn: invalid conn", (conn != NULL), return;);

  netconn_listen(conn);

  while(1)
  {
    err = netconn_accept(conn, &newconn);
    if (err == ERR_OK)
    {
        g_newconn = newconn;
        while ((err = netconn_recv(newconn, &buf)) == ERR_OK)
        {
            g_newconn = newconn;
            do{
                //netconn_write(newconn,buf->p->payload,buf->p->len, NETCONN_NOCOPY);
                if (g_netconn_recv != NULL) {
                    ((char *)buf->p->payload)[buf->p->len] = 0;
                    g_netconn_recv(buf->p->payload,buf->p->len);
                }
            }while(netbuf_next(buf) >= 0);
            netbuf_free(buf);
            netbuf_delete(buf);
        }
        g_newconn = NULL;
        netconn_close(newconn);
        netconn_delete(newconn);
    }
  }
}

void netconn_tcp_send_data(uint8_t *data, int len)
{
    if (g_newconn != NULL)
    netconn_write(g_newconn,data ,len, NETCONN_NOFLAG);
}


void netconn_init(netconn_recv_fn recv)
{
    g_netconn_recv = recv;
    LWIP_ASSERT_CORE_LOCKED();
    sys_thread_new("netconn_tcp_server", netconn_tcp_server, NULL, 1024*16, 4);
}
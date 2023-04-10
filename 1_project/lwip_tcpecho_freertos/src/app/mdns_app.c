
#include "lwip/sys.h"       // 调用线程需要用到
#include "sys_arch.h"

#include "FreeRTOS.h"       // 线程延时和删除用到
#include "task.h"

#include "netconf.h"        // gnetif 用到

#include "mdns_app.h"



#if LWIP_MDNS_RESPONDER

#include "mdns.h"

#define MDNS_HOSTNAME "Uart-TCP"
static char s_mdns_hostname[65] = "";


/*!
 * @brief Callback function to generate TXT mDNS record for HTTP service.
 */
static void http_srv_txt(struct mdns_service *service, void *txt_userdata)
{
    mdns_resp_add_service_txtitem(service, "path=/", 6);
}

void http_server_enable_mdns(struct netif *netif, const char *mdns_hostname)
{
    mdns_resp_init();
    mdns_resp_add_netif(netif, mdns_hostname, 3600);
    mdns_resp_add_service(netif, mdns_hostname, "_udpserver", DNSSD_PROTO_UDP, 5001, 3600, http_srv_txt, NULL);

    //(void)strncpy(s_mdns_hostname, mdns_hostname, sizeof(s_mdns_hostname) - 1);
    //s_mdns_hostname[sizeof(s_mdns_hostname) - 1] = '\0'; // Make sure string will be always terminated.
}

static void mdns_thread(void *arg)
{
    vTaskDelay(10000);

    http_server_enable_mdns(&gnetif, MDNS_HOSTNAME);

    vTaskDelete(NULL);
}
#endif

void mdns_init(void)
{
#if LWIP_MDNS_RESPONDER
    sys_thread_new("mdns_thread", mdns_thread, NULL, 1024*8, 4);
#endif
}

/*
 *copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <aos/aos.h>
#include <aos/yloop.h>
#include "netmgr.h"
#include "iot_export.h"
#include "iot_import.h"
#include "air_fresh.h"
#include "vendor.h"
#include "msg_process_center.h"
#include "property_report.h"

#define MB_RGBSTATUS_COUNT 10

static aos_queue_t *g_cmd_msg_queue_id = NULL;
static char *g_cmd_msg_queue_buff = NULL;

aos_queue_t *g_property_report_queue_id = NULL;
char *g_property_report_queue_buff = NULL;

void init_msg_queue(void)
{
    if (g_cmd_msg_queue_buff == NULL) {
        g_cmd_msg_queue_id = (aos_queue_t *) aos_malloc(sizeof(aos_queue_t));
        g_cmd_msg_queue_buff = aos_malloc(MB_RGBSTATUS_COUNT * sizeof(recv_msg_t));

        aos_queue_new(g_cmd_msg_queue_id, g_cmd_msg_queue_buff, MB_RGBSTATUS_COUNT * sizeof(recv_msg_t),
                sizeof(recv_msg_t));
    }

    if (g_property_report_queue_buff == NULL) {
        g_property_report_queue_id = (aos_queue_t *) aos_malloc(sizeof(aos_queue_t));
        g_property_report_queue_buff = aos_malloc(MB_RGBSTATUS_COUNT * sizeof(property_report_msg_t));

        aos_queue_new(g_property_report_queue_id, g_property_report_queue_buff,
                MB_RGBSTATUS_COUNT * sizeof(property_report_msg_t), sizeof(property_report_msg_t));
    }
}

void send_msg_to_queue(recv_msg_t * cmd_msg)
{
    int ret = aos_queue_send(g_cmd_msg_queue_id, cmd_msg, sizeof(recv_msg_t));
    if (0 != ret)
        LOG_TRACE("###############ERROR: CMD MSG: aos_queue_send failed! #################\r\n");
}

void msg_process_task(void *argv)
{
    uint32_t h, s, v;
    unsigned int rcvLen;
    recv_msg_t msg;
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();
    device_status_t *device_status = &user_example_ctx->status;

    while (true) {
        if (aos_queue_recv(g_cmd_msg_queue_id, AOS_WAIT_FOREVER, &msg, &rcvLen) == 0) {
            if(msg.type == 1 && !product_set_power(msg.powerstate)){
                device_status->powerstate = msg.powerstate;
                report_device_property(msg.seq, msg.flag);
            }
            if(msg.type == 2 && !product_set_speed(msg.speed)){
                device_status->speed = msg.speed;
                report_device_property(msg.seq, msg.flag);
            }
            if(msg.type == 3 && !product_set_bypass(msg.bypass)){
                device_status->bypass = msg.bypass;
                report_device_property(msg.seq, msg.flag);
            }

            if(msg.type == 4){
                user_example_ctx_t *user_example_ctx = user_example_get_ctx();
                device_status_t *device_status = &user_example_ctx->status;
                if (msg.flag == 1) {
                    int powerstate = product_get_power();
                    if (device_status->powerstate != powerstate)
                    {
                        user_post_powerstate(powerstate);
                    }
                }
                else if (msg.flag == 2) {
                    int speed = product_get_speed();
                    if (device_status->speed != speed)
                    {
                        user_post_speed(speed);
                    }
                }
                else if (msg.flag == 3) {
                    int bypass = product_get_bypass();
                    if (device_status->bypass != bypass)
                    {
                        user_post_bypass(bypass);
                    }
                }
                else
                {
                    LOG_TRACE("###############ERROR: msg.type: %d, msg.flag: %d #################\r\n", msg.type, msg.flag);
                }
                
            }
            aos_msleep(250);
        }
    }
}

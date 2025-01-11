#ifndef __AIR_FRESH_METER_H__
#define __AIR_FRESH_METER_H__

#include "aos/aos.h"

#define DEBUG_OUT   1

#ifdef DEBUG_OUT
#define LOG_TRACE(...)                               \
    do {                                                     \
        HAL_Printf("%s.%d: ", __func__, __LINE__);  \
        HAL_Printf(__VA_ARGS__);                                 \
        HAL_Printf("\r\n");                                   \
    } while (0)
#else
#define LOG_TRACE(...)
#endif

typedef struct {
    uint8_t powerstate;   //开关
    uint8_t speed;        //风速
    uint8_t bypass;       //旁通
} device_status_t;

typedef struct {
    int master_devid;
    int cloud_connected;
    int master_initialized;
    int bind_notified;
    device_status_t status;
} user_example_ctx_t;

user_example_ctx_t *user_example_get_ctx(void);

void user_post_powerstate(int powerstate);
void user_post_speed(int speed);
void user_post_bypass(int state);

void update_power_state(int powerstate);
void update_speed(int speed);
void update_bypass(int state);
void example_free(void *ptr);

#endif

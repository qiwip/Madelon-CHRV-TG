/*
 *copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <aos/aos.h>
#include <hal/hal.h>
#include <aos/yloop.h>
#include "netmgr.h"
#include "nanomodbus.h"
#include "iot_export.h"
#include "iot_import.h"
#include "app_entry.h"
#include "aos/kv.h"
#include <hal/soc/gpio.h>
#include "vendor.h"
#include "device_state_manger.h"
#include "iot_import_awss.h"
#include "air_fresh.h"
#include "bl_flash.h"
#if defined(HF_LPT230) || defined(HF_LPT130)
#include "hfilop/hfilop.h"
#endif

#if defined(HF_LPT130) 
#define LED_GPIO    22
#define KEY_GPIO    4
#elif defined(HF_LPT230) || defined(UNO_91H) /* on hf-lpt230 EVB */
#define LED_GPIO    8
#define KEY_GPIO    25
#elif defined(MK3080) /* on mk3080 EVB */
#define LED_GPIO    9
#define KEY_GPIO    12
#elif defined(MX1270) /* on mx1270 EVB  */
#define LED_GPIO    11
#define KEY_GPIO    8
#elif defined(BK7231UDEVKITC)
#define LED_GPIO    15
#define KEY_GPIO    28
#elif defined(AMEBAZ_DEV) /* on amebaz_dev */
#define LED_GPIO    9
#define KEY_GPIO    12
#elif (defined (TG7100CEVB))
#define LED_GPIO    20  //14 for tg7100CA
#define KEY_GPIO    3
char *p_product_key = NULL;
char *p_product_secret = NULL;
char *p_device_name = NULL;
char *p_device_secret = NULL;
char *productidStr = NULL;
#else /* default config  */
#define LED_GPIO    22
#define KEY_GPIO    4
#endif
#define RTU_SERVER_ADDRESS 1

#define POWER_REG_ADDRESS 0
#define IN_WIND_REG_ADDRESS 7
#define OUT_WIND_REG_ADDRESS 8
#define BYPASS_REG_ADDRESS 9


extern uart_dev_t uart_1;

static gpio_dev_t io_led;
static gpio_dev_t io_key;

static int led_state = 0;
static int powerstate = 0;
static int windspeed = 1;
static int windspeed_in = 1;
static int windspeed_out = 1;
static int bypass = 0;

static nmbs_t nmbs;
static nmbs_platform_conf platform_conf;

int32_t read_serial(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
    uint32_t recv_size = 0;
    int ret = hal_uart_recv_II(&uart_1, buf, count, &recv_size,  byte_timeout_ms);
    if (ret != 0)
    {
        return -1;
    }
    return recv_size;
}


int32_t write_serial(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
    int ret = hal_uart_send(&uart_1, buf, count, byte_timeout_ms);
    if (ret != 0)
    {
        return -1;
    }
    return count;
    
}

void init_modbus(){
    nmbs_platform_conf_create(&platform_conf);
    platform_conf.transport = NMBS_TRANSPORT_RTU;
    platform_conf.read = read_serial;
    platform_conf.write = write_serial;

    nmbs_error err = nmbs_client_create(&nmbs, &platform_conf);
    if (err != NMBS_ERROR_NONE){
        LOG("modbus init error[%d]", err);
    }

    nmbs_set_read_timeout(&nmbs, 1000);
    nmbs_set_byte_timeout(&nmbs, 100);

    nmbs_set_destination_rtu_address(&nmbs, RTU_SERVER_ADDRESS);
}

void product_set_led(bool state)
{
    if (led_state == (int)state) {
        return;
    }

    if (state) {
        hal_gpio_output_high(&io_led);
    } else {
        hal_gpio_output_low(&io_led);
    }

    led_state = (int)state;
}

static bool product_get_led()
{
    return (bool)led_state;
}

void product_toggle_led()
{
    if (product_get_led() == ON) {
        product_set_led(OFF);
    } else {
        product_set_led(ON);
    }
}

int product_set_power(int state)
{

    if (powerstate == state) {
        return 0;
    }
    
    nmbs_error err;
    if (state) {
        err = nmbs_write_single_register(&nmbs, POWER_REG_ADDRESS, 1);
    } else {
        err = nmbs_write_single_register(&nmbs, POWER_REG_ADDRESS, 0);
    }

    if (err == NMBS_ERROR_NONE) {
        powerstate = (int)state;
        product_set_led(powerstate);
        LOG("product set power success, powerstate: %d\n", powerstate);
    } else{
        LOG("product set power faild, err:%d\n", err);
        return -1;
    }
    return 0;
}

int product_get_power(void)
{
    uint16_t value = 0;
    nmbs_error err = nmbs_read_holding_registers(&nmbs, POWER_REG_ADDRESS, 1, &value);
    if (err == NMBS_ERROR_NONE) {
        product_set_led(powerstate);
        powerstate = value;
    } else{
        LOG("product get power faild, err: %d\n", err);
    }

    return powerstate;
}

int product_set_speed(int speed)
{
    nmbs_error err;

    if (windspeed_in != speed) {
        err = nmbs_write_single_register(&nmbs, IN_WIND_REG_ADDRESS, speed);
        if (err == NMBS_ERROR_NONE) {
            windspeed_in = speed;
            LOG("product set in-wind success, speed: %d\n", speed);
        } else{
            LOG("product set in-wind faild, err:%d\n", err);
            return -1;
        }
        aos_msleep(200);
    }
    if (windspeed_out != speed) {
        err = nmbs_write_single_register(&nmbs, OUT_WIND_REG_ADDRESS, speed);
        if (err == NMBS_ERROR_NONE) {
            windspeed_out = speed;
            LOG("product set out-wind success, speed: %d\n", speed);
        } else{
            LOG("product set out-wind faild, err:%d\n", err);
            return -1;
        }
    }

    windspeed = speed;
    return 0;
}

int product_get_speed(void)
{
    uint16_t in_wind = 0;
    nmbs_error err;
    err = nmbs_read_holding_registers(&nmbs, IN_WIND_REG_ADDRESS, 1, &in_wind);
    if (err == NMBS_ERROR_NONE) {
        windspeed_in = in_wind;
    } else{
        LOG("product get in-wind speed faild, err: %d\n", err);
    }
    aos_msleep(50);

    uint16_t out_wind = 0;
    err = nmbs_read_holding_registers(&nmbs, OUT_WIND_REG_ADDRESS, 1, &out_wind);
    if (err == NMBS_ERROR_NONE) {
        windspeed_out = out_wind;
    } else{
        LOG("product get out_wind speed faild, err: %d\n", err);
    }

    windspeed = in_wind > out_wind ? in_wind : out_wind;

    return windspeed;
}

int product_set_bypass(int state)
{
    if (bypass == state) {
        return 0;
    }

    nmbs_error err;

    if (state) {
        err = nmbs_write_single_register(&nmbs, BYPASS_REG_ADDRESS, 1);
    } else {
        err = nmbs_write_single_register(&nmbs, BYPASS_REG_ADDRESS, 0);
    }

    if (err == NMBS_ERROR_NONE) {
        bypass = (int)state;
        LOG("product set bypass success, state: %d\n", bypass);
    } else{
        LOG("product set bypass faild, err:%d\n", err);
        return -1;
    }
    return 0;
}

int product_get_bypass(void)
{
    uint16_t value = 0;
    nmbs_error err = nmbs_read_holding_registers(&nmbs, BYPASS_REG_ADDRESS, 1, &value);
    if (err == NMBS_ERROR_NONE) {
        bypass = value;
    } else{
        LOG("product get bypass faild, err: %d\n", err);
    }
    return bypass;
}


void vendor_product_init(void)
{
    io_led.port = LED_GPIO;
    io_key.port = KEY_GPIO;
    LOG("LED_GPIO:%d, KEY_GPIO:%d", LED_GPIO, KEY_GPIO);

    io_led.config = OUTPUT_PUSH_PULL;    
    io_key.config = INPUT_PULL_UP;

    hal_gpio_init(&io_led);
    hal_gpio_init(&io_key);
    init_modbus();
    LOG("init_modbus DONE");

    // init
    led_state = OFF;
    powerstate = 0;
    windspeed = 1;
    windspeed_in = 1;
    windspeed_out = 1;
    bypass = 0;

    // product_get_power();
    // aos_msleep(200);
    // product_get_speed();
    // aos_msleep(200);
    // product_get_bypass();
    // aos_msleep(200);
}


bool product_get_key(void)
{
    uint32_t level;
    hal_gpio_input_get(&io_key, &level);
    return level;
}

int vendor_get_product_key(char *product_key, int *len)
{
    char *pk = NULL;
    int ret = -1;
    int pk_len = *len;

    ret = aos_kv_get(KV_KEY_PK, product_key, &pk_len);
#if defined(HF_LPT230) || defined(HF_LPT130)
    if ((ret != 0)&&((pk = hfilop_layer_get_product_key()) != NULL)) {
        pk_len = strlen(pk);
        memcpy(product_key, pk, pk_len);
        ret = 0;
    }
#else
    /*
        todo: get pk...
    */
#endif
    if (ret == 0) {
        *len = pk_len;
    }
#if (defined (TG7100CEVB))
    if(p_product_key != NULL && strlen(p_product_key) > 0){
        pk_len = strlen(p_product_key);
        memcpy(product_key, p_product_key, pk_len);
        *len = pk_len;
        ret = 0;
    }
#endif
    return ret;
}

int vendor_get_product_secret(char *product_secret, int *len)
{
    char *ps = NULL;
    int ret = -1;
    int ps_len = *len;

    ret = aos_kv_get(KV_KEY_PS, product_secret, &ps_len);
#if defined(HF_LPT230) || defined(HF_LPT130)
    if ((ret != 0)&&((ps = hfilop_layer_get_product_secret()) != NULL)) {
        ps_len = strlen(ps);
        memcpy(product_secret, ps, ps_len);
        ret = 0;
    }
#else
    /*
        todo: get ps...
    */
#endif
    if (ret == 0) {
        *len = ps_len;
    }
#if (defined (TG7100CEVB))
    if(p_product_secret != NULL && strlen(p_product_secret) > 0){
        ps_len = strlen(p_product_secret);
        memcpy(product_secret, p_product_secret, ps_len);
        *len = ps_len;
        ret = 0;
    }
#endif
    return ret;
}

int vendor_get_device_name(char *device_name, int *len)
{
    char *dn = NULL;
    int ret = -1;
    int dn_len = *len;

    ret = aos_kv_get(KV_KEY_DN, device_name, &dn_len);
#if defined(HF_LPT230) || defined(HF_LPT130)
    if ((ret != 0)&&((dn = hfilop_layer_get_device_name()) != NULL)) {
        dn_len = strlen(dn);
        memcpy(device_name, dn, dn_len);
        ret = 0;
    }
#else
    /*
        todo: get dn...
    */
#endif
    if (ret == 0) {
        *len = dn_len;
    }
#if (defined (TG7100CEVB))
    if(p_device_name != NULL && strlen(p_device_name) > 0){
        dn_len = strlen(p_device_name);
        memcpy(device_name, p_device_name, dn_len);
        *len = dn_len;
        ret = 0;
    }
#endif
    return ret;
}

int vendor_get_device_secret(char *device_secret, int *len)
{
    char *ds = NULL;
    int ret = -1;
    int ds_len = *len;

    ret = aos_kv_get(KV_KEY_DS, device_secret, &ds_len);
#if defined(HF_LPT230) || defined(HF_LPT130)
    if ((ret != 0)&&((ds = hfilop_layer_get_device_secret()) != NULL)) {
        ds_len = strlen(ds);
        memcpy(device_secret, ds, ds_len);
        ret = 0;
    }
#else
    /*
        todo: get ds...
    */
#endif
    if (ret == 0) {
        *len = ds_len;
    }
#if (defined (TG7100CEVB))
    if(p_device_secret != NULL && strlen(p_device_secret) > 0){
        ds_len = strlen(p_device_secret);
        memcpy(device_secret, p_device_secret, ds_len);
        *len = ds_len;
        ret = 0;
    }
#endif
    return ret;
}

int vendor_get_product_id(uint32_t *pid)
{
    int ret = -1;
    char pidStr[11] = {0};
    int len = sizeof(pidStr);

    ret = aos_kv_get(KV_KEY_PD, pidStr, &len);
    if (ret == 0 && len < sizeof(pidStr)) {
        *pid = atoi(pidStr);
    } else {
        ret = -1;
    }
#if (defined (TG7100CEVB))
    if(productidStr != NULL && strlen(productidStr) > 6){
        *pid = atoi(productidStr);
        // LOG("pid[%d]", *pid);
        return 0;
    }
#endif
    return ret;
}

int set_device_meta_info(void)
{
    int len = 0;
    char product_key[PRODUCT_KEY_LEN + 1] = {0};
    char product_secret[PRODUCT_SECRET_LEN + 1] = {0};
    char device_name[DEVICE_NAME_LEN + 1] = {0};
    char device_secret[DEVICE_SECRET_LEN + 1] = {0};

    memset(product_key, 0, sizeof(product_key));
    memset(product_secret, 0, sizeof(product_secret));
    memset(device_name, 0, sizeof(device_name));
    memset(device_secret, 0, sizeof(device_secret));

    len = PRODUCT_KEY_LEN + 1;
    vendor_get_product_key(product_key, &len);

    len = PRODUCT_SECRET_LEN + 1;
    vendor_get_product_secret(product_secret, &len);

    len = DEVICE_NAME_LEN + 1;
    vendor_get_device_name(device_name, &len);

    len = DEVICE_SECRET_LEN + 1;
    vendor_get_device_secret(device_secret, &len);

    if ((strlen(product_key) > 0) && (strlen(product_secret) > 0) \
            && (strlen(device_name) > 0) && (strlen(device_secret) > 0)) {
        HAL_SetProductKey(product_key);
        HAL_SetProductSecret(product_secret);
        HAL_SetDeviceName(device_name);
        HAL_SetDeviceSecret(device_secret);
        LOG("pk[%s]", product_key);
        LOG("dn[%s]", device_name);
        return 0;
    } else {
#if (defined (TG7100CEVB))
        /* check media valid, and update p */
	extern int ali_factory_media_get(char **pp_product_key, char **pp_product_secret,
        			char **pp_device_name, char **pp_device_secret, char **pp_pidStr);
        int res = ali_factory_media_get(
                    &p_product_key,
                    &p_product_secret,
                    &p_device_name,
                    &p_device_secret,
                    &productidStr);
        if (0 != res) {
            printf("ali_factory_media_get res = %d\r\n", res);
            return -1;
        } else {
            HAL_SetProductKey(p_product_key);
            HAL_SetProductSecret(p_product_secret);
            HAL_SetDeviceName(p_device_name);
            HAL_SetDeviceSecret(p_device_secret);
            LOG("pk[%s]", p_product_key);
            LOG("dn[%s]", p_device_name);
            // LOG("pid[%s]", productidStr);
            return 0;
        }
#endif
        LOG("no valid device meta data");
        return -1;
    }
}

void linkkit_reset(void *p);
void vendor_device_bind(void)
{
    set_net_state(APP_BIND_SUCCESS);
}

void vendor_device_unbind(void)
{
    linkkit_reset(NULL);
}
void vendor_device_reset(void)
{
    /* do factory reset */
    // clean kv ...
    // clean buffer ...
    /* end */
    do_awss_reset();
}

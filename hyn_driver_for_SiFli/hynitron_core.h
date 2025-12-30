#ifndef HYNITRON_CORE_H
#define HYNITRON_CORE_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "drv_io.h"
//#include <rtthread.h>
#include "board.h"
#include "drv_touch.h"
#define DBG_LEVEL DBG_ERROR // DBG_LOG //
#define LOG_TAG "drv.cst918"
//#include <drv_log.h>

// Function to be modified for drive migration

#ifndef bool
#define bool char
#endif

#define FALSE 0
#define TRUE 1

#ifndef uint8_t
#define uint8_t unsigned char
#endif
#ifndef int8_t
#define int8_t signed char
#endif
#ifndef uint16_t
#define uint16_t unsigned short
#endif
#ifndef int16_t
#define int16_t signed short
#endif
#ifndef uint32_t
#define uint32_t unsigned int
#endif
#ifndef int32_t
#define int32_t signed int
#endif

// The following does not need to be modified
#define HYNITRON_DRIVER_VERSION "CST92xx_MCU_Driver_V3.8_20240318"
#define HYNITRON_I2C_TRAN_PER_SIZE (130)
#define HYNITRON_PROGRAM_PAGE_SIZE (128)
#define HYNITRON_PROGRAM_PAGE_TIMEOUT (5) // 5ms ~ 20ms��wait write every package done,maybe affect update time.
#define CHIP_TYPE_CST9217 (0x9217)
#define CHIP_TYPE_CST9220 (0x9220)
#define HYN_REPORT_TOUCH_CHECKSUM_EN (0)
#define CONFIG_TP_FACTORY (0) // get debug data
#define HYN_RESUME_RESET (0)
#define HYN_ENABLE_PLUG_STATE (0)
#define HYN_ENABLE_WEAR_STATE (0)

#define CST9217_DEVICE_NAME ("dev/input0")
#define TP_CST9217_PMIC_VDD_PORT ("ldo21")
#define CST9217_GPIO_VCC_ID (147)
#define CST9217_GPIO_INT_ID (41)
#define CST9217_GPIO_RST_ID (42)
#define CST9217_I2C_FREQUENCY (100000)
#define CST9217_MAX_FINGER_NUM (1)
#define CST9217_BUFF_NUMS (2)
// #define TP_CST9217_ESD_CHECK 1

#define HYNITRON_FINGER_NUM (1)
#define HYNITRON_ENABLE_UPGRADE (1)
#define HYNITRON_I2C_ADDR (0x5A)
#define HYN_CHIP_TYPE (CHIP_TYPE_CST9217)
#define HYN_BOOT_I2C_ADDR (0x5A)
#define MEM_SIZE (0x7F80) // 31KB

#define DEBUG_RAWDATA_ENABLE (0)
#define HYN_IIC_RETRY_NUM 2

#define SINGLE_CLICK (1)
#define DOUBLE_CLICK (2)
#define SLIDE_UP (4)
#define SLIDE_DOWN (6)
#define SLIDE_LEFT (5)
#define SLIDE_RIGHT (3)

#define GPIO_TP_RESET  44

#define DBG_UART

#ifdef DBG_UART
#define HYNITRON_DEBUG(fmt, args...) LOG_D("[HYN][%s]" fmt "", __func__, ##args)
#define HYNITRON_INFO(fmt, args...) LOG_I("[HYN][%s]" fmt "", __func__, ##args)
#define HYNITRON_NOTICE(fmt, args...) LOG_I("[HYN][%s]" fmt "", __func__, ##args)
#define HYNITRON_ERROR(fmt, args...) LOG_E("[HYN][%s]" fmt "", __func__, ##args)
#define HYNITRON_ALERT(fmt, args...) LOG_I("[HYN][%s]" fmt "", __func__, ##args)
#else
#define HYNITRON_DEBUG(...)
#define HYNITRON_INFO(fmt, args...) LOG_I("[HYN][%s]" fmt "", __func__, ##args)
#define HYNITRON_NOTICE(fmt, args...) LOG_I("[HYN][%s]" fmt "", __func__, ##args)
#define HYNITRON_ERROR(fmt, args...) LOG_E("[HYN][%s]" fmt "", __func__, ##args)
#endif
extern void HAL_Delay_us(__IO uint32_t us);

#define gpio_set_level(pin, level) rt_pin_write(pin, level)
#define gpio_get_level(pin) rt_pin_read(pin)
#define DELAY_MS(ms) rt_thread_mdelay(ms)                            // delay 1ms
#define DELAY_US(us) HAL_Delay_us(us)                            // delay 1us


#define HYN_FUNC_ENTER // HYNITRON_NOTICE("enter");
#define HYN_FUNC_EXIT  // HYNITRON_NOTICE("exit");

// work mode
#define ENUM_MODE_NORMAL (0x00)
// #define ENUM_MODE_LOW_POWER (0X01)
// #define ENUM_MODE_DEEP_SLEEP (0X02)
// #define ENUM_MODE_WAKEUP (0x03)
#define ENUM_MODE_DEBUG_DIFF (0x04)
#define ENUM_MODE_DEBUG_RAWDATA (0X05)
#define ENUM_MODE_FACTORY (0x06)
#define ENUM_MODE_DEBUG_INFO (0x07)
#define ENUM_MODE_UPDATE_FW (0x08)
#define ENUM_MODE_PLUG_IN (0x09)
#define ENUM_MODE_PLUG_OUT (0x0A)
#define ENUM_MODE_WEAR (0x0B)
#define ENUM_MODE_UNWEAR (0x0C)
#define ENUM_MODE_NULL (0xFF)

#define TEST_SNS_OPEN_HIGHDRV (0x10)
#define TEST_SNS_OPEN_LOWDRV (0x11)
#define TEST_SNS_SHORT (0x12)
#define TEST_SNS_LPSCAN (0x13)
#define TEST_SYS_IDLE (0x14)
#define TEST_SYS_DEEPSLEEP (0x15)
#define TEST_IRQPIN_HIGH (0x16)
#define TEST_IRQPIN_LOW (0x17)
#define TEST_SCAP_SCANDATA (0x18)

#define ENUM_MODE_FACTORY_HIGHDRV (0x10)
#define ENUM_MODE_FACTORY_LOWDRV (0x11)
#define ENUM_MODE_FACTORY_SHORT (0x12)
#define ENUM_MODE_LPSCAN (0x13)

#define DEEP_SLEEP (0x01)
#define IDLE_SLEEP (0x02)
#define GESTURE_SLEEP (0x03)

#define X_RES (480)
#define Y_RES (480)
#define TX_NUM (8)
#define RX_NUM (7)
#define TRX_NUM (TX_NUM * RX_NUM)
#define TRX_KEY_NUM (0)
#define HYN_ENTER_BOOT_STARTTIME 13 // ms
#define HYN_ENTER_BOOT_TIMEOUT 25   // ms
#define HYN_RESET_DELAY_TIME 40     // ms

#define HYN_DEVICE_TOUCH_EVENT_NONE (0)
#define HYN_DEVICE_TOUCH_EVENT_DOWN (1)
#define HYN_DEVICE_TOUCH_EVENT_MOVE (2)
#define HYN_DEVICE_TOUCH_EVENT_STAY (3)
#define HYN_DEVICE_TOUCH_EVENT_UP (4)
#define HYN_DEVICE_TOUCH_EVENT_CLICK (5)
#define HYN_DEVICE_TOUCH_EVENT_PALM (6)
#define HYN_DEVICE_TOUCH_EVENT_SLIDE_UP (7)

#define HYN_GESTURE_PALM 0x80
#define HYN_GESTURE_SCLICK 0x10
#define HYN_GESTURE_DCLICK 0x20
#define HYN_GESTURE_UP 0x40
#define HYN_GESTURE_DOWN 0x60
#define HYN_GESTURE_LEFT 0x50
#define HYN_GESTURE_RIGHT 0x30

#define HYNITRON_ENABLE_UPGRADE_H_FILE (1)
#define HYNITRON_READ_INFO_FROM_FLASH (0)
#define HYNITRON_DEBUG_VAR  0

#if HYNITRON_DEBUG_VAR
#define CST92XX_BIN_FW_WXN_PATH "/log/hyn_firmware_p62.bin"
#define CST92XX_BIN_FW_WXN_PATH_ESIM "/log/hyn_firmware_p62.bin"
#define CST92XX_BIN_FW_WXN_PATH_TEST "/log/hyn_firmware_p62.bin"
#else
#define CST92XX_BIN_FW_WXN_PATH "/vendor/touch/hyn_firmware_p62.bin"
#define CST92XX_BIN_FW_WXN_PATH_ESIM "/vendor/touch/hyn_firmware_p62.bin"
#define CST92XX_BIN_FW_WXN_PATH_TEST "/log/hyn_firmware_p62.bin"
#endif

struct hyn_chip
{
    struct
    {
        uint8_t int_trig : 1;
        uint8_t ic_init_done : 1;
        uint8_t esd_enable : 1;
        uint8_t reproting_flag : 1;
        uint8_t sleep_done : 2;

    } status;

    uint8_t chip_ic_workmode;
    uint32_t chip_type;
    uint32_t partno_chip_type; // only read,not changed
    uint32_t module_id;        // only read,not changed

    uint32_t esd_lock;
    uint32_t esd_value;
    uint32_t esd_value_pre;

    int rst_pin; /* Reset pin*/

    struct
    {
        bool firmware_info_ok;
        uint32_t firmware_ic_type;
        uint32_t firmware_version;
        uint32_t firmware_checksum;
        uint32_t firmware_project_id;
        uint8_t tx_num;
        uint8_t rx_num;
        uint8_t key_num;
    } IC_firmware;

    // file
    struct
    {
        bool ok;
        uint8_t *head_data;
        uint8_t *data;
        uint32_t checksum;
        uint32_t version;
        uint32_t project_id;
        uint32_t chip_type;
    } bin_data;

    struct
    {
        bool valid;
        uint8_t finger_id;
        uint8_t evt;
        uint16_t x;
        uint16_t y;
        uint16_t z;
        uint16_t checksum;
    } point_info[HYNITRON_FINGER_NUM];

    struct
    {
        bool valid;
        uint8_t gesture;
        uint8_t palm;
        uint8_t finger_num;
        uint8_t key_id;
        uint8_t key_status;
    } touch_info;

    int16_t (*i2c_write)(uint8_t addr, uint8_t *buf, uint16_t len);
    int16_t (*i2c_write_read)(uint8_t addr, uint8_t *cmd, uint16_t cmd_len, uint8_t *buf, uint16_t buf_len);

    int16_t (*enter_boot)(void);
    int16_t (*read_chip_id)(void);
    int16_t (*read_info)(void);

    int16_t (*judge_module)(void);
    int16_t (*upgrade_firmware)(void);
    int16_t (*upgrade_firmware_judge)(void);
    int16_t (*bin_firmware_parse)(void);
    int16_t (*get_bin_addr)(uint8_t data_seq, uint16_t data_len);

    int16_t (*read_point)(void);
    int16_t (*enter_sleep)(uint8_t sleep_type);
    int16_t (*wake_up)(void);
    int16_t (*poweron_ic)(bool on);

    void (*reset_ic)(void);
    void (*esd_check)(void);

    int16_t (*get_firmware_info)(void);
    int16_t (*set_work_mode)(uint8_t mode);

    int16_t (*get_diff_data)(void);
    int16_t (*get_rawdata_data)(void);
    int16_t (*get_debug_data)(void);

    int16_t (*get_factory_test_result)(uint16_t *cval_buf);
};
extern struct hyn_chip *p_g_chip_obj;
// #define int rt_int8_t
// #define uint8_t rt_uint8_t
// #define uint16_t rt_uint16_t
// #define uint32_t rt_uint32_t
// #define bool rt_bool_t

// Function to be modified for drive migration

// port
int16_t read_chip_id(void);
void hyn_drv_init(void);
void hyn_on_int_trig_evt(void);
void hyn_on_wake(void);
void hyn_on_sleep(void);

void hyn_sys_init(void);
void hyn_sys_handler(void);
void hyn_set_work_mode(uint8_t mode);
uint8_t hyn_get_irq_signal(void);

int hynitron_i2c_read(uint8_t addr, void *buffer, uint16_t length);
int hynitron_i2c_write(uint8_t addr, void *buffer, uint16_t length);
bool disable_I2C_window(void); // close iic window ,in case communication fails

// cstx enter bootloader
bool hyn_cst9217_enter_boot(void);

uint8_t hyn_cstx_try_type(void);

uint8_t *get_bin_addr(struct hyn_chip *self);

uint32_t get_bin_data_size(void);
// chip object
void hyn_cst92xx_init_obj(struct hyn_chip *chip);

bool hyn_get_rawdata_test(uint16_t *cval_buf);
bool hyn_factory_test(void);
void hynitron_sysinfo(void);

void cst9217_ts_esd_check(void);

#endif

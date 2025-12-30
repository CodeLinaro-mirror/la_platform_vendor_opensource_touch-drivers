/**
 ******************************************************************************
 * @file   hynitron_core.c
 * @author Sifli software development team
 ******************************************************************************
 */
/**
 * @attention
 * Copyright (c) 2019 - 2022,  Sifli Technology
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Sifli integrated circuit
 *    in a product or a software update for such product, must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of Sifli nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Sifli integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY SIFLI TECHNOLOGY "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SIFLI TECHNOLOGY OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "hynitron_core.h"
#if HYNITRON_ENABLE_UPGRADE_H_FILE
#include ".\hyn_chips\hyn_firmware_b898.h"
#endif

/* function and value-----------------------------------------------------------*/
static struct rt_i2c_bus_device *ft_bus = NULL;
static struct touch_drivers hyn_driver;
struct hyn_chip g_chip_obj;
struct hyn_chip *p_g_chip_obj;
static void (*const g_init_chip_obj[])(struct hyn_chip *) =
{
    hyn_cst92xx_init_obj,
};

// Function to be modified for drive migration
int16_t hyn_i2c_write_port(uint8_t addr, uint8_t *buf, uint16_t len)
{
    int16_t ret = -1;
    struct rt_i2c_msg msgs;
    HYN_FUNC_ENTER;

    msgs.addr = addr; /* slave address */
    msgs.flags = RT_I2C_WR;              /* write flag */
    msgs.buf = buf;                      /* Send data pointer */
    msgs.len = len;
    
    if (rt_i2c_transfer(ft_bus, &msgs, 1) == 1)
    {
        ret = 0;
    }
    else
    {
        ret = -1;
    }

    HYN_FUNC_EXIT;
    return ret;
}
// Function to be modified for drive migration
int16_t hyn_i2c_read_port(uint8_t addr, uint8_t *buf, uint16_t len)
{
    int16_t ret = -1;
    struct rt_i2c_msg msgs;
    HYN_FUNC_ENTER;

    msgs.addr = addr; /* Slave address */
    msgs.flags = RT_I2C_RD;              /* Read flag */
    msgs.buf = buf;                      /* Read data pointer */
    msgs.len = len;                      /* Number of bytes read */

    if (rt_i2c_transfer(ft_bus, &msgs, 1) == 1)
    {
        ret = 0;
    }
    else
    {
        ret = -1;
    }
    HYN_FUNC_EXIT;
    return ret;
}

// Function to be modified for drive migration
void hyn_reset_ic(void)
{
    HYN_FUNC_ENTER;
    gpio_set_level(GPIO_TP_RESET, 0);
    DELAY_MS(5);    // 10 -> 5ms  20250820
    gpio_set_level(GPIO_TP_RESET, 1);
    if (NULL == p_g_chip_obj)
    {
        HYNITRON_ERROR("p_g_chip_obj NULL return");
        return;
    }
    p_g_chip_obj->chip_ic_workmode = ENUM_MODE_NORMAL;
    p_g_chip_obj->esd_value = 0;
    p_g_chip_obj->esd_value_pre = 0;
    HYN_FUNC_EXIT;
}

// Function to be modified for drive migration
int16_t hyn_poweron_ic(bool on)
{
    HYN_FUNC_ENTER;
    if (on == FALSE)
    {

    }
    else
    {

    }
    HYN_FUNC_EXIT;
    return 0;
}

int16_t hyn_i2c_write(uint8_t addr, uint8_t *buf, uint16_t len)
{
    int16_t ret = 0;
    HYN_FUNC_ENTER;
    for (uint8_t i = 0;; i++)
    {
        if (i >= HYN_IIC_RETRY_NUM)
        {
            return -1;
        }
        ret = hyn_i2c_write_port(addr, buf, len);
        //      HYNITRON_DEBUG("iic_write_data:%d",ret);
        if (ret)
        {
            DELAY_US(200);
            continue;
        }
        break;
    }
    HYN_FUNC_EXIT;
    return 0;
}
// return -1 : fail
// return 0 : success
int16_t hyn_i2c_write_read(uint8_t addr, uint8_t *cmd, uint16_t cmd_len, uint8_t *buf, uint16_t buf_len)
{
    int16_t retry;
    int ret;
    HYN_FUNC_ENTER;
    for (retry = 0; retry < 2; retry++) {
        ret = hyn_i2c_write_port(addr, cmd, cmd_len);
        if (ret) {
            // HYNITRON_ERROR("hyn_i2c_write retry :%d;", retry);
            DELAY_US(200);
            continue;
        }
        ret = hyn_i2c_read_port(addr, buf, buf_len);
        if (ret) {
            // HYNITRON_ERROR("hyn_i2c_read retry :ret:%d retry%d ",ret, retry);
            DELAY_US(200);
            continue;
        }
        break;
    }

    if (retry == 5) {
        HYNITRON_ERROR("hyn_i2c_write_read failed");
        return -1;
    }
    HYN_FUNC_EXIT;
    return 0;
}

int16_t hyn_get_bin_addr(uint8_t data_seq, uint16_t data_len)
{
    HYN_FUNC_ENTER;
    if (p_g_chip_obj->bin_data.head_data == NULL)
    {
        // GET firmware bin data point
        HYNITRON_ERROR("hyn_get_bin_addr data NULL or len error return");
        return -1;
    }
    p_g_chip_obj->bin_data.data = (uint8_t *)p_g_chip_obj->bin_data.head_data + (data_seq * data_len);
    // HYNITRON_DEBUG("get_bin_addr data_seq:0x%04x,data_len:0x%04x.data_point:0x%04x",data_seq,data_len,(data_seq*data_len));
    if ((p_g_chip_obj->bin_data.data == NULL) || ((data_seq * data_len) > MEM_SIZE))
    {
        HYNITRON_ERROR("hyn_get_bin_addr data NULL or len error return");
        return -1;
    }
    HYN_FUNC_EXIT;
    return 0;
}

int get_fw_bin_addr(const char *name)
{
    uint16_t length=0;
    FILE* fp = fopen(name, "rbe");
    if(fp == NULL)
    {
        HYNITRON_ERROR("Open TP firmware file %s failed!\n",name);
        return -1;
    }
    else
    {
        // lseek(fp, 0, 0);
        length=fread((uint8_t *)(g_chip_obj.bin_data.head_data),1,0x7f80,fp);//0x7F80
        HYNITRON_DEBUG("########%s:%d 0x%x\r\n",__func__,__LINE__,*g_chip_obj.bin_data.head_data);
        if(length != 0x7F80)
        {
            HYNITRON_ERROR("Read TP firmware file failed=0x%x!\n",length);
            fclose(fp);
            return -1;
        }
    }
    fclose(fp);
    return 0;
}

int16_t hyn_upgrade_firmware(int force_upgrade)
{
    HYN_FUNC_ENTER;
#if (HYNITRON_ENABLE_UPGRADE == 1)
    {
        uint8_t need_upgrade = 0;
        int16_t ret = -1;
#if (HYNITRON_ENABLE_UPGRADE_H_FILE==0)
        g_chip_obj.bin_data.head_data = (uint8_t *)malloc(0x7f80);
        if (g_chip_obj.bin_data.head_data == NULL)
        {
            HYNITRON_ERROR("###malloc#####%s:%d\r\n", __func__, __LINE__);
        }
        int ret1 = -1;
        if(force_upgrade)
        {
            ret1 = get_fw_bin_addr(CST92XX_BIN_FW_WXN_PATH_TEST);
        }
        else
        {
            ret1 = get_fw_bin_addr(CST92XX_BIN_FW_WXN_PATH);
        }
        if (ret1 < 0)
        {
            HYNITRON_ERROR("get_fw_bin_addr fail\r\n");
            ret = false;
        }
        else
#else
        g_chip_obj.bin_data.head_data = (uint8_t *)fw_data;
#endif
        {
            ret = p_g_chip_obj->bin_firmware_parse();
            if (ret < 0)
            {
                HYNITRON_ERROR("bin_firmware_parse fail.");
                goto END_UPGRADE;
            }
            if(force_upgrade == 1)
            {
                need_upgrade = 1;
            }
            else
            {
                ret = p_g_chip_obj->upgrade_firmware_judge();
                if (ret)
                {
                    HYNITRON_DEBUG("upgrade_firmware_judge return,no need update fw.");
                    goto END_UPGRADE;
                    // need_upgrade = 1;
                }
                else
                {
                    need_upgrade = 1;
                }
            }
            HYNITRON_DEBUG("need_upgrade=%d, firmware_version=0x%04X.", need_upgrade, p_g_chip_obj->bin_data.version);
            if (need_upgrade)
            {
                ret = p_g_chip_obj->upgrade_firmware();
                if (ret)
                {
                    HYNITRON_NOTICE("upgrade_firmware failed");
                    goto END_UPGRADE;
                }
                HYNITRON_DEBUG("upgrade_firmware OK done.");
                p_g_chip_obj->reset_ic();
                DELAY_MS(40);
                ret = p_g_chip_obj->get_firmware_info();
                if (ret)
                {
                    HYNITRON_ERROR("get_firmware_info failed");
                    p_g_chip_obj->reset_ic();
                }
            }
            return 0;
        }
    END_UPGRADE:
        p_g_chip_obj->reset_ic();
        DELAY_MS(40);
    }
#endif
    HYN_FUNC_EXIT;
    return -1;
}

void hyn_irq_handler(void *arg)
{

    HYNITRON_INFO("hyn touch_irq_handler\n");

    rt_touch_irq_pin_enable(0);

    rt_sem_release(hyn_driver.isr_sem);
}

static rt_err_t hyn_report(touch_msg_t p_msg)
{
    rt_touch_irq_pin_enable(1);
    p_g_chip_obj->status.int_trig = 1;
    if((p_g_chip_obj->chip_ic_workmode == ENUM_MODE_DEBUG_DIFF || p_g_chip_obj->chip_ic_workmode == ENUM_MODE_DEBUG_RAWDATA)&& (p_g_chip_obj->status.sleep_done == 0))
    {
        p_g_chip_obj->get_debug_data();
        p_g_chip_obj->status.int_trig = 0;
        return RT_EEMPTY;
    }

    if(p_g_chip_obj->chip_ic_workmode!= ENUM_MODE_NORMAL)
    {
        // HYNITRON_ERROR("ERROR: TP hyn chip_ic_workmode unnormal \n");
        return RT_EEMPTY;
    }
    else
    {
        if(p_g_chip_obj->read_point())
        {
            if (p_g_chip_obj->read_point())
            {
                p_msg->event = 0;
                return RT_EEMPTY;
            }
        }
        // gesture
        if ((p_g_chip_obj->touch_info.palm == 1)&&(p_g_chip_obj->status.sleep_done==0)) {
            // palm
            return RT_EEMPTY; // No more data to be read
        } else if ((p_g_chip_obj->touch_info.gesture == HYN_GESTURE_SCLICK)&&(p_g_chip_obj->status.sleep_done > 0)) {
            // sclick
            return RT_EEMPTY; // No more data to be read
        }

        if (6 == p_g_chip_obj->point_info[0].evt || 7 == p_g_chip_obj->point_info[0].evt) {
            p_msg->event = TOUCH_EVENT_DOWN;
        } else {
            p_msg->event = TOUCH_EVENT_UP;
        }
        p_msg->x = p_g_chip_obj->point_info[0].x;
        p_msg->y = p_g_chip_obj->point_info[0].y;

    }
    p_g_chip_obj->status.int_trig = 0;
    return RT_EEMPTY; // No more data to be read
}

static int hyn_read_chip(void)
{
    int ret = -1;
    for (int i = 0; i < 3; i++)
    {
        ret = p_g_chip_obj->judge_module();
        if (ret) {
            DELAY_MS(2);
        } else {
            break;
        }
    }
    if (ret)
    {
        ret = p_g_chip_obj->enter_boot();
        if (ret)
        {
            HYNITRON_ERROR("enter_boot error");
            return RT_ERROR;
        }
        ret = p_g_chip_obj->read_chip_id();
        if (ret)
        {
            HYNITRON_ERROR("read_chip_id fail,error return");
            return RT_ERROR;
        }
        p_g_chip_obj->reset_ic();
        DELAY_MS(HYN_RESET_DELAY_TIME);
    }
    return ret;
}

static rt_err_t hyn_init(void)
{
    int ret = -1;

    ret = p_g_chip_obj->get_firmware_info();
    if (ret)
    {
        HYNITRON_ERROR("get_firmware_info fail,maybe chip null");
        p_g_chip_obj->set_work_mode(ENUM_MODE_NORMAL);
    }
    hyn_upgrade_firmware(0);
    p_g_chip_obj->chip_ic_workmode = ENUM_MODE_NORMAL;
    p_g_chip_obj->status.ic_init_done = 1;
    p_g_chip_obj->status.esd_enable = TRUE;

    rt_touch_irq_pin_attach(PIN_IRQ_MODE_FALLING, hyn_irq_handler, NULL);
    rt_touch_irq_pin_enable(1); // Must enable before read I2C

    HYNITRON_DEBUG("hynitron init OK");
    return RT_EOK;
}

uint32_t get_tp_cst_chip_id(void)
{
    int16_t ret = 0;
    uint32_t chip_id = p_g_chip_obj->partno_chip_type;
    if ((chip_id != 0x9217) && (chip_id != 0x9220))
    {
        ret = p_g_chip_obj->read_chip_id();
        if (ret) {
            HYNITRON_INFO("HYN_REG_CHIP_ID Failed");
        }
        chip_id = p_g_chip_obj->partno_chip_type;
    }
    HYNITRON_INFO("HYN_REG_CHIP_ID id1=0x%02x\n", chip_id);
    return chip_id;
}
MSH_CMD_EXPORT(get_tp_cst_chip_id, get hyn chip id);

uint32_t get_tp_fimw_version(void)
{
    uint32_t fwr = p_g_chip_obj->IC_firmware.firmware_version;
    HYNITRON_INFO("HYN_REG_FW_VERSION=0x%02x\n", fwr);
    return fwr;
}
MSH_CMD_EXPORT(get_tp_fimw_version, get hyn fw version);

void get_factory_test_result_test(void)
{
    uint16_t buff [TRX_NUM];
    p_g_chip_obj->get_factory_test_result(buff);
    for(uint8_t i = 0; i < 8;i++)
    {
        for(uint8_t j = 0; j < 7 ; j++)
        {
            rt_kprintf("%d ",buff[(i*7)+j]);
        }
        rt_kprintf("\n");
    }
}
MSH_CMD_EXPORT(get_factory_test_result_test, get hyn test_result);

void set_debug_rawdata_mode(void)
{
    int16_t ret = -1;
    ret = p_g_chip_obj->set_work_mode(ENUM_MODE_DEBUG_RAWDATA);
    if (ret) {
        HYNITRON_ERROR("set raw mode failed");
    }
    HYNITRON_ERROR("set raw mode OK");
}
MSH_CMD_EXPORT(set_debug_rawdata_mode, set hyn rawdata);

void set_debug_diff_mode(void)
{
    int16_t ret = -1;
    ret = p_g_chip_obj->set_work_mode(ENUM_MODE_DEBUG_DIFF);
    if (ret) {
        HYNITRON_ERROR("set diff mode failed");
    }
    HYNITRON_ERROR("set diff mode OK");
}
MSH_CMD_EXPORT(set_debug_diff_mode, set hyn diff);

void set_exit_debug_mode(void)
{    
    int16_t ret = -1;
    ret = p_g_chip_obj->set_work_mode(ENUM_MODE_NORMAL);
    if (ret) {
        HYNITRON_ERROR("set normal mode failed");
    }
    HYNITRON_ERROR("set normal mode OK");
}
MSH_CMD_EXPORT(set_exit_debug_mode, set hyn normal);

static rt_err_t hyn_suspend(void)
{
    if (1)  // gesture
    {
        p_g_chip_obj->enter_sleep(GESTURE_SLEEP);
    }
    else
    {
        p_g_chip_obj->enter_sleep(DEEP_SLEEP);
        rt_touch_irq_pin_enable(0);
    }
    return RT_EOK;
}

static rt_err_t hyn_resume(void)
{
    HYN_FUNC_ENTER;
    HYNITRON_DEBUG("workmode=%d\r\n", g_chip_obj.chip_ic_workmode);
    if (g_chip_obj.chip_ic_workmode != ENUM_MODE_UPDATE_FW)
    {
        if (p_g_chip_obj->status.sleep_done == DEEP_SLEEP) {
            rt_touch_irq_pin_enable(1);
        }
        p_g_chip_obj->wake_up();
#if HYN_ENABLE_PLUG_STATE
        if (plugin)
        {
            hynitron_plug_cb(1); // set D11F plug_in flag after reset
        }
#endif

#if HYN_ENABLE_WEAR_STATE
        if (wear_state_by_hyn)
        {
            hynitron_wear_cb(1);
        }
#endif
    }
    HYN_FUNC_EXIT;
    return RT_EOK;
}

static rt_err_t hyn_esd_check(void)
{
    HYN_FUNC_ENTER;
    if (NULL == p_g_chip_obj)
    {
        HYNITRON_ERROR("p_g_chip_obj NULL return");
        return RT_ERROR;
    }
    p_g_chip_obj->esd_check();
    HYN_FUNC_EXIT;
    return RT_EOK;
}


static rt_err_t hyn_deinit(void)
{
    HYNITRON_INFO("hynitron deinit");

    rt_touch_irq_pin_enable(0);

    return RT_EOK;
}

static rt_bool_t hyn_probe(void)
{
    ft_bus = (struct rt_i2c_bus_device *)rt_device_find(TOUCH_DEVICE_NAME);
    if (RT_Device_Class_I2CBUS != ft_bus->parent.type)
    {
        ft_bus = NULL;
    }
    if (ft_bus)
    {
        rt_device_open((rt_device_t)ft_bus, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_TX | RT_DEVICE_FLAG_INT_RX);
    }
    else
    {
        LOG_I("bus not find\n");
        return RT_FALSE;
    }

    {
        struct rt_i2c_configuration configuration =
            {
                .mode = 0,
                .addr = 0,
                .timeout = 500,
                .max_hz = 400000,
            };

        rt_i2c_configure(ft_bus, &configuration);
    }

    HYNITRON_DEBUG("%s:start driver init.", HYNITRON_DRIVER_VERSION);
    memset(&g_chip_obj, 0, sizeof(g_chip_obj));
    g_chip_obj.rst_pin = GPIO_TP_RESET;
    g_init_chip_obj[0](&g_chip_obj);
    p_g_chip_obj = &g_chip_obj;
    p_g_chip_obj->reset_ic = hyn_reset_ic;
    p_g_chip_obj->poweron_ic = hyn_poweron_ic;
    p_g_chip_obj->i2c_write = hyn_i2c_write;
    p_g_chip_obj->i2c_write_read = hyn_i2c_write_read;
    p_g_chip_obj->get_bin_addr = hyn_get_bin_addr;
    p_g_chip_obj->status.ic_init_done = 0;
    p_g_chip_obj->poweron_ic(TRUE);
    
    if(hyn_read_chip())
    {
        HYNITRON_ERROR("hynitron probe fail");
        return RT_FALSE;
    }
    HYNITRON_INFO("hynitron probe OK");
    return RT_TRUE;
}

static struct touch_ops hyn_ops =
    {
        hyn_report,
        hyn_init,
        hyn_deinit,
        // hyn_suspend,
        // hyn_resume,
        // hyn_esd_check,
    };

static int rt_hyn_init(void)
{
    hyn_driver.probe = hyn_probe;
    hyn_driver.ops = &hyn_ops;
    hyn_driver.user_data = RT_NULL;
    hyn_driver.isr_sem = rt_sem_create("hyn", 0, RT_IPC_FLAG_FIFO);

    rt_touch_drivers_register(&hyn_driver);

    return 0;
}
INIT_COMPONENT_EXPORT(rt_hyn_init);

/************************ (C) COPYRIGHT Sifli Technology *******END OF FILE****/

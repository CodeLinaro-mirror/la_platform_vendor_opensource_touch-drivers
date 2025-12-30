/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief TP Keyboard driver for Actions SoC
 */
#include "hyn_tpkey_acts.h"
#include "./../../../../framework/uAT/include/at_core.h"
LOG_MODULE_DECLARE(tpkey, LOG_LEVEL_INF);
static struct hyn_ts_data *hyn_data = NULL;
static int _hyn_set_tp_debug(uint8_t mode);

void hyn_irq_set(struct hyn_ts_data *ts_data, u8 value)
{

}

void hyn_esdcheck_switch(struct hyn_ts_data *ts_data, u8 enable)
{
    
}

void _hyn_reset(void)
{
    const struct device *dev = tpkey_dev_get();
    tpkey_reset_pin_set(dev, 1);    // low
    hyn_msleep(10);
    tpkey_reset_pin_set(dev, 0);    // high
}

void _hyn_poweron(const struct device *dev, bool is_on)
{
    if (is_on) {
        tpkey_power_pin_set(dev, 1);
        hyn_msleep(20);//need verify 20
        _hyn_reset();
    } else {
        tpkey_reset_pin_set(dev, 1);
        tpkey_power_pin_set(dev, 0);
    }
}

void hyn_set_i2c_addr(struct hyn_ts_data *ts_data,u8 addr)
{
    ts_data->salve_addr = addr;
}

int hyn_write_data(struct hyn_ts_data *ts_data, u8 *buf, u8 reg_len, u16 len)
{
    const struct device *dev = tpkey_dev_get();
    int ret = 0;
    ret = tpkey_dev_i2c_write_addr(dev, ts_data->salve_addr, buf, len);
    return ret < 0 ? -1:0;
}

int hyn_read_data(struct hyn_ts_data *ts_data,u8 *buf, u16 len)
{
    const struct device *dev = tpkey_dev_get();
    int ret = 0;
    ret = tpkey_dev_i2c_read_addr(dev, ts_data->salve_addr, buf, len);
    return ret < 0 ? -1:0;
}

int hyn_wr_reg(struct hyn_ts_data *ts_data, u32 reg_addr, u8 reg_len, u8 *rbuf, u16 rlen)
{
    const struct device *dev = tpkey_dev_get();
    int ret = 0,i=0;
    u8 wbuf[4]={0};
    reg_len = reg_len&0x0F;
    memset(wbuf,0,sizeof(wbuf));
    i = reg_len;
    while(i--){
        wbuf[i] = reg_addr;
        reg_addr >>= 8;
    }
    ret = tpkey_dev_i2c_write_addr(dev, ts_data->salve_addr, wbuf, reg_len);
    if(rlen){   
        ret = tpkey_dev_i2c_read_addr(dev, ts_data->salve_addr, rbuf, rlen);
    }

    if (ret) {
        HYN_ERROR("IIC addr-%x,err-%x",ts_data->salve_addr,reg_addr);
    }
    return ret < 0 ? -1:0;
}

static int _i2c_async_read_point_cb(void *cb_data, struct i2c_msg *msgs,
                    uint8_t num_msgs, bool is_err)
{
    struct input_value input_val;
    uint32_t timestamp;
    uint8_t finger_num;
    uint8_t i = 0;

    if (is_err) {
        HYN_ERROR("i2c read err\n");
        goto fail_exit;
    }

    timestamp = k_cycle_get_32();

    hyn_data->hyn_fuc_used->tp_report(msgs->buf);

    // gesture wake up
    if (hyn_data->gesture_id == IDX_POWER) {
        HYN_INFO("gesture click");
        goto fail_exit;
    }

    // big palm
    if (hyn_data->gesture_id == IDX_O) {
        HYN_INFO("tpkey big palm");

		if (input_val.point.pessure_value) {
			input_val.point.pessure_value = 0;
			tpkey_report(&input_val, timestamp);
		}
		struct tpkey_data *tpkey = (struct tpkey_data *)cb_data;
		if (!tpkey->suspended) {
			tpkey->input_val.point.gesture = BIG_PAN_GESTURE;
			tpkey->notify_cb(tpkey->this_device, &tpkey->input_val);
		}

		goto fail_exit;
	}

    finger_num = hyn_data->rp_buf.rep_num;
	if (finger_num > MAX_POINTS_REPORT)
		goto fail_exit;

	for (i=0;i<finger_num;i++) {
        uint8_t id = hyn_data->rp_buf.pos_info[i].pos_id;
        if (id < finger_num) {
            input_val.point.loc_x = hyn_data->rp_buf.pos_info[i].pos_x;
            input_val.point.loc_y = hyn_data->rp_buf.pos_info[i].pos_y;
            input_val.point.pessure_value = hyn_data->rp_buf.pos_info[i].event;
            input_val.point.gesture = 0;
            tpkey_report(&input_val, timestamp);
            HYN_INFO("report-(%d,%d):%d",input_val.point.loc_x,input_val.point.loc_y,input_val.point.pessure_value);
        }
    }

fail_exit:
	return 0;
}

static int _i2c_async_debug_cb(void *cb_data, struct i2c_msg *msgs,
                    uint8_t num_msgs, bool is_err)
{
    HYN_INFO("DEBUG_MODE");
    if (is_err) {
        HYN_ERROR("i2c read err\n");
        return -1;
    }
    hyn_data->hyn_fuc_used->tp_get_dbg_data(msgs->buf,CUSTOM_SENSOR_ALL*2);
    return 0;
}

static void _hyn_read_touch(const struct device *dev)
{
	static uint8_t i2c_buf[3+6*MAX_POINTS_REPORT] = { 0 };
	static uint8_t reg_buf = 0x00;

	struct tpkey_data *tpkey = dev->data;
	int ret = 0;
    if (hyn_data->work_mode == DIFF_MODE) {
        reg_buf = 0x61;
    } else if (hyn_data->work_mode == RAWDATA_MODE) {
        reg_buf = 0x41;
    } else {
        reg_buf = 0x00;
    }

	if (tpkey->suspended) {
		tpkey->input_val.point.gesture = TOUCH_ON_GESTURE;
		tpkey->notify_cb(tpkey->this_device, &tpkey->input_val);
	}

    ret = i2c_write_async(tpkey->bus, (void *)&reg_buf, sizeof(reg_buf),
                    hyn_data->salve_addr, tpkey_i2c_async_dummy_cb, NULL);
    if (ret)
        goto out_exit;

    if (reg_buf==0x41 || reg_buf==0x61) {
        static uint8_t debug_buf[CUSTOM_SENSOR_ALL*2];
        ret = i2c_read_async(tpkey->bus, (void *)debug_buf, sizeof(debug_buf),
                        hyn_data->salve_addr, _i2c_async_debug_cb, tpkey);
        if (ret)
            goto out_exit;
    } else {
        ret = i2c_read_async(tpkey->bus, (void *)i2c_buf, sizeof(i2c_buf),
                        hyn_data->salve_addr, _i2c_async_read_point_cb, tpkey);
        if (ret)
            goto out_exit;
    }

out_exit:
    return;
}

static int _hyn_check_existed(const struct device *dev)
{
    int ret = 0;
    static struct hyn_ts_data ts_data;
    memset((void*)&ts_data,0,sizeof(ts_data));
    hyn_data = &ts_data;
    HYN_INFO(HYN_DRIVER_VERSION);

    hyn_data->hyn_fuc_used = &cst8xxT_fuc;  //根据芯片型号赋值
    hyn_data->plat_data.max_touch_num = MAX_POINTS_REPORT;   //最大手指数
    hyn_data->plat_data.x_resolution = 390;  //x最大分辨率
    hyn_data->plat_data.y_resolution = 450;  //y最大分辨率
    hyn_data->plat_data.swap_xy = 0;         //xy坐标交换
    hyn_data->plat_data.reverse_x = 0;       //x坐标反向
    hyn_data->plat_data.reverse_y = 0;       //y坐标反向
    hyn_set_i2c_addr(hyn_data,MAIN_I2C_ADDR);

    _hyn_poweron(dev, true);
    hyn_msleep(20);
    tpkey_reset_pin_set(dev, 0);    //high
    
    ret = hyn_data->hyn_fuc_used->tp_chip_init(hyn_data);
    if (!ret) {
        hyn_data->boot_is_pass = 1;
    }
    
    hyn_set_i2c_addr(hyn_data,MAIN_I2C_ADDR);

    return ret;
}

static int _hyn_check_upgrade(void)
{
#if HYN_POWER_ON_UPDATA
    if(hyn_data->need_updata_fw)
    {
        hyn_data->fw_file_name[0] = 0; //use .h to updata
        hyn_data->hyn_fuc_used->tp_updata_fw(hyn_data->fw_updata_addr,hyn_data->fw_updata_len);  
    }
#endif
    return 0;
}

static int _hyn_init(const struct device *dev, bool first)
{

    if (first && _hyn_check_existed(dev))
    {
        _hyn_poweron(dev, false);
        hyn_data->power_is_on = 0;
        return -ENODEV;
    }
    
    hyn_data->power_is_on = 1;
    _hyn_check_upgrade();
    _hyn_reset();
    hyn_msleep(50);

    // test
    // hyn_data->hyn_fuc_used->tp_get_test_result(NULL,NULL);
    // _hyn_set_tp_debug(DIFF_MODE);

    hyn_data->hyn_fuc_used->tp_set_workmode(NOMAL_MODE,1);

    return 0;
}

static int _hyn_uninit(const struct device *dev)
{
    hyn_data->hyn_fuc_used->tp_supend();
    return 0;
}

static int _hyn_idle_on(const struct device *dev)
{
    hyn_data->work_mode = GESTURE_MODE;
    return 0;
}

static int _hyn_idle_off(const struct device *dev)
{
    hyn_data->hyn_fuc_used->tp_resum();
    return 0;
}

static int _hyn_esd_check(const struct device *dev)
{
    hyn_data->hyn_fuc_used->tp_check_esd();
    return 0;
}

static int _hyn_get_fac_result(const struct device *dev, uint16_t *tp_cap_buf_high, uint16_t *tp_cap_buf_low)
{
    hyn_data->hyn_fuc_used->tp_get_test_result(tp_cap_buf_high,tp_cap_buf_low);
    return -1;
}

static uint16_t _hyn_get_chip_id(void)
{
    AT_TRANS("817m chip:0x%x firmware_version:%d\n",(uint16_t)hyn_data->hw_info.fw_chip_type,hyn_data->hw_info.fw_ver);
    return (uint16_t)hyn_data->hw_info.fw_chip_type;
}

static int _hyn_set_tp_debug(uint8_t mode)
{
    int ret = -1;
    if (mode == RAWDATA_MODE) {
        HYN_INFO("set tp raw mode");
        ret = hyn_data->hyn_fuc_used->tp_set_workmode(RAWDATA_MODE,1);
    } else if (mode == DIFF_MODE) {
        HYN_INFO("set tp diff mode");
        ret = hyn_data->hyn_fuc_used->tp_set_workmode(DIFF_MODE,1);
    } else if (mode == NOMAL_MODE) {
        HYN_INFO("set tp normal mode");
        hyn_data->hyn_fuc_used->tp_rest();
        ret = hyn_data->hyn_fuc_used->tp_set_workmode(NOMAL_MODE,1);
    }
    if (ret) {
        HYN_ERROR("set tp debug failed");
    }
    return ret;
}

static const struct tpkey_ops hyn_tpkey_ops = {
    .power_on = _hyn_init,
    .power_off = _hyn_uninit,
    .idle_on = _hyn_idle_on,
    .idle_off = _hyn_idle_off,
    .esd_check = _hyn_esd_check,
    .read_cb = _hyn_read_touch,
    .get_chip_id = _hyn_get_chip_id,
    .get_factory_test_result = _hyn_get_fac_result,
    .set_tp_debug = _hyn_set_tp_debug,
};

STRUCT_SECTION_ITERABLE(tpkey_custom_config, hyn_tpkey) = {
    .ops = &hyn_tpkey_ops,
    .supported_gestures = INPUT_GESTURE_DIRECTION,
    .addr = MAIN_I2C_ADDR,
};

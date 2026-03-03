
#include "hyn_core.h"

#define HYN_DRIVER_NAME  "hyn_ts"
#define RTPM_PRIO_TPD               0x04
static struct hyn_ts_data *hyn_data = NULL;
static const struct hyn_ts_fuc* hyn_fun = NULL;
static const struct of_device_id hyn_of_match_table[] = {
    {.compatible = "hyn,66xx", .data = &cst66xx_fuc,},   /*suport 36xx 35xx 66xx 68xx 148E*/
	{.compatible = "hyn,36xxes", .data = &cst36xxes_fuc,}, /*suport 154es 3654es 3640es 3140*/
    {.compatible = "hyn,3240", .data = &cst3240_fuc,},   /*suport 3240 */
    {.compatible = "hyn,923xx", .data = &cst923xx_fuc,},   /*suport 9217、9220 、916e、9317、317q、3217 */
    {.compatible = "hyn,3xx",  .data = &cst3xx_fuc,},    /*suport 340 348 328 128 140 148*/
    {.compatible = "hyn,7xx",  .data = &cst7xx_fuc,},    /*suport 726 826 836u*/
    {.compatible = "hyn,8xxt", .data = &cst8xxT_fuc,},   /*suport 816t 816d 820 08C*/
    {.compatible = "hyn,226se", .data = &cst226se_fuc,}, /*suport 226se 8922*/
	{.compatible = "hyn,76xx", .data = &cst76xx_fuc,},   /*suport 7864BG 7964BG HYT7864JL HYT7760BG HYT7760TR CST6960BG*/
    {.compatible = "hyn,840u", .data = &cst840u_fuc,},   /*suport 840u*/
    {},
};
MODULE_DEVICE_TABLE(of, hyn_of_match_table);

static int hyn_check_ic(struct hyn_ts_data *ts_data)
{
    const struct of_device_id *of_dev;
    of_dev = of_match_device(hyn_of_match_table,ts_data->dev);
	if (!of_dev)
		return -EINVAL;
    hyn_fun = of_dev->data;
    if(IS_ERR_OR_NULL(hyn_fun) || IS_ERR_OR_NULL(hyn_fun->tp_chip_init) 
        || hyn_fun->tp_chip_init(ts_data)){
        return -ENODEV;
    }
    ts_data->hyn_fuc_used = hyn_fun;
    return 0;
}

static int hyn_parse_dt(struct hyn_ts_data *ts_data)
{
    int ret = 0;
    struct device *dev = ts_data->dev;
    struct hyn_plat_data* dt = &ts_data->plat_data;

    if(tpd_dts_data.use_tpd_button){
        u8 i;
        dt->key_num = tpd_dts_data.tpd_key_num;
        dt->key_y_coords = tpd_dts_data.tpd_key_dim_local[0].key_y;
        HYN_INFO("key_num:%d", dt->key_num);
        for(i = 0; i<dt->key_num; i++){
            dt->key_x_coords[i] = tpd_dts_data.tpd_key_dim_local[i].key_x;
            dt->key_code[i] = tpd_dts_data.tpd_key_local[i];
            HYN_INFO("key%d(%d,%d,%d)", i,dt->key_x_coords[i],dt->key_y_coords,dt->key_code[i]);
        }
    }

    if(!IS_ERR_OR_NULL(dev->of_node)){
        u32 buf[2];
        struct device_node *np = dev->of_node;
        dt->reset_gpio = 0;
        dt->irq_gpio = 1;
        HYN_INFO("reset_gpio:%d irq_gpio:%d",dt->reset_gpio,dt->irq_gpio);

        ret = of_property_read_u32(np, "pos-swap", &dt->swap_xy);
        ret |= of_property_read_u32(np, "posx-reverse", &dt->reverse_x);
        ret |= of_property_read_u32(np, "posy-reverse", &dt->reverse_y);
        if(ret){
            HYN_ERROR("pos-swap posx-reverse posy-reverse");
            goto dts_read_end;
        }
        dt->max_touch_num = tpd_dts_data.touch_max_num > 10 ? 10:tpd_dts_data.touch_max_num;
        dt->x_resolution = tpd_dts_data.tpd_resolution[0];
        dt->y_resolution = tpd_dts_data.tpd_resolution[1];
        
        np = tpd->tpd_dev->of_node; //TPD &touch node
        if(of_property_read_u32_array(np, "interrupts", buf, 2)){
            HYN_ERROR("interrupts");
            goto dts_read_end;
        }
        dt->tpd_irg_gpio = buf[0];
        HYN_INFO("dts x_res = %d,y_res = %d,touch-number = %d tpd_irg_gpio = %d",
                    dt->x_resolution,dt->y_resolution,dt->max_touch_num,dt->tpd_irg_gpio);
    }
    return 0 ;
dts_read_end:
     HYN_ERROR("dts match failed");
    return -ENODEV;
}


static int hyn_poweron(struct hyn_ts_data *ts_data)
{
    int ret = 0;
    tpd->reg = regulator_get(tpd->tpd_dev, "vtouch");
    ret = regulator_set_voltage(tpd->reg, 2800000, 2800000);
    if(ret){
        HYN_ERROR("regulator_set_voltage(%d) failed!\n", ret);
        ts_data->plat_data.vdd_ana = NULL;
    }
    else{
        ts_data->plat_data.vdd_ana = tpd->reg;
        ts_data->power_is_on = 1;
    }
    ret = regulator_enable(tpd->reg);

    // if(gpio_request(ts_data->plat_data.tpd_irg_gpio, "hyn_irq_gpio")<0){
    //     HYN_ERROR("tpd_irg_gpio request");
    //     return -1;
    // }
    tpd_gpio_output(ts_data->plat_data.reset_gpio, 0);
    tpd_gpio_output(ts_data->plat_data.irq_gpio, 1);
    mdelay(5);
    tpd_gpio_output(ts_data->plat_data.reset_gpio, 1);
    return ret;
}

static int hyn_input_dev_init(struct hyn_ts_data *ts_data)
{
    int key_num = 0;//,ret =0;
    struct hyn_plat_data *dt = &ts_data->plat_data;
    struct input_dev *input_dev;

    HYN_ENTER(); 
    input_dev = input_allocate_device();
    if (!input_dev) {
        HYN_ERROR("Failed to allocate memory for input device");
        return -ENOMEM;
    }
    input_dev->name = HYN_DRIVER_NAME;
    input_dev->id.bustype = ts_data->bus_type;
    input_dev->dev.parent = ts_data->dev;
    input_set_drvdata(input_dev, ts_data);

    __set_bit(EV_SYN, input_dev->evbit);
    __set_bit(EV_ABS, input_dev->evbit);
    __set_bit(EV_KEY, input_dev->evbit);
    __set_bit(BTN_TOUCH, input_dev->keybit);
    __set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

    for (key_num = 0; key_num < dt->key_num; key_num++)
			input_set_capability(input_dev, EV_KEY, dt->key_code[key_num]);
#if HYN_MT_PROTOCOL_B_EN
    set_bit(BTN_TOOL_FINGER,input_dev->keybit);
    //input_mt_init_slots(input_dev, dt->max_touch_num);
    input_mt_init_slots(input_dev, dt->max_touch_num+1, INPUT_MT_DIRECT);
#else
    input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
#endif

	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0,  dt->max_touch_num, 0, 0); 
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, dt->x_resolution,0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, dt->y_resolution,0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);

    ts_data->input_dev = input_dev;

    return 0;
}

static void release_all_finger(struct hyn_ts_data *ts_data)
{
    HYN_ENTER();
    if(ts_data->report_id_flg){
#if HYN_MT_PROTOCOL_B_EN
        u8 i;
        for(i=0; i< ts_data->plat_data.max_touch_num; i++) {
            if(ts_data->report_id_flg & (1UL<<i)){
                // HYN_INFO("release %d",i);
                input_mt_slot(ts_data->input_dev, i);
                input_report_abs(ts_data->input_dev, ABS_MT_TRACKING_ID, -1);
                input_mt_report_slot_state(ts_data->input_dev, MT_TOOL_FINGER, false);
            }	
        }
        input_report_key(ts_data->input_dev, BTN_TOUCH, 0);
#else
        input_report_key(ts_data->input_dev, BTN_TOUCH, 0);
        input_mt_sync(ts_data->input_dev);
#endif
    }
    ts_data->report_id_flg = 0;
}

static void touch_updata(u8 idx,u8 event)
{
    struct ts_frame *rep_frame = &hyn_data->rp_buf;
    struct input_dev *dev = hyn_data->input_dev;
    u16 zpress = rep_frame->pos_info[idx].pres_z;

    if(event){
        if(zpress < 10){
            zpress += (10+(rep_frame->pos_info[idx].pos_x&0x03));
        }
        hyn_data->report_id_flg |= (1UL<< rep_frame->pos_info[idx].pos_id);
    }
    else{
        hyn_data->report_id_flg &= ~(1UL<< rep_frame->pos_info[idx].pos_id);
    }
#if HYN_MT_PROTOCOL_B_EN
    if(event){
        input_report_key(dev, BTN_TOUCH, 1);
        input_mt_slot(dev, rep_frame->pos_info[idx].pos_id);
        input_mt_report_slot_state(dev, MT_TOOL_FINGER, 1);
        input_report_abs(dev, ABS_MT_TRACKING_ID, rep_frame->pos_info[idx].pos_id);
        input_report_abs(dev, ABS_MT_POSITION_X, rep_frame->pos_info[idx].pos_x);
        input_report_abs(dev, ABS_MT_POSITION_Y, rep_frame->pos_info[idx].pos_y);
        input_report_abs(dev, ABS_MT_TOUCH_MAJOR, zpress>>3);
        input_report_abs(dev, ABS_MT_WIDTH_MAJOR, zpress>>3);
        input_report_abs(dev, ABS_MT_PRESSURE, zpress);
    }
    else{
        input_mt_slot(dev, rep_frame->pos_info[idx].pos_id);
        input_report_abs(dev, ABS_MT_TRACKING_ID, -1);
        input_mt_report_slot_state(dev, MT_TOOL_FINGER, 0);
    }
#else
    if(event){
        input_report_key(dev, BTN_TOUCH, 1);
        input_report_abs(dev, ABS_MT_PRESSURE, zpress);
        input_report_abs(dev, ABS_MT_TRACKING_ID, rep_frame->pos_info[idx].pos_id);
        input_report_abs(dev, ABS_MT_TOUCH_MAJOR, zpress>>3);
        // input_report_abs(dev, ABS_MT_WIDTH_MAJOR, zpress>>3);
        input_report_abs(dev, ABS_MT_POSITION_X, rep_frame->pos_info[idx].pos_x);
        input_report_abs(dev, ABS_MT_POSITION_Y, rep_frame->pos_info[idx].pos_y);
        input_mt_sync(dev);
    }
#endif
}

static void hyn_irq_report(void)
{
    struct hyn_ts_data *ts_data = hyn_data;
    struct ts_frame *rep_frame = &hyn_data->rp_buf;
    struct input_dev *dev = hyn_data->input_dev;
    struct hyn_plat_data *dt = &hyn_data->plat_data;
    u16 xpos,ypos;
    int reprot_state_clr = 0;
    if(hyn_fun->tp_report()){
        HYN_INFO("Ignore illegal data");
        return;
    } 
    mutex_lock(&ts_data->mutex_report);
    if(rep_frame->report_need & REPORT_KEY){ //key
#if KEY_USED_POS_REPORT
        rep_frame->pos_info[0].pos_id = 0;
        rep_frame->pos_info[0].pos_x = dt->key_x_coords[rep_frame->key_id];
        rep_frame->pos_info[0].pos_y = dt->key_y_coords;
        rep_frame->pos_info[0].pres_z = 100;
        touch_updata(0,rep_frame->key_state ? 1:0);
#else
        input_report_key(dev,dt->key_code[rep_frame->key_id],rep_frame->key_state ? 1:0); 
#endif
        if(rep_frame->key_state==0){
            reprot_state_clr = 1;
        } 
        input_sync(dev);
        HYN_INFO2("report keyid:%d keycode:%d",rep_frame->key_id,dt->key_code[rep_frame->key_id]);
    }

    if(rep_frame->report_need & REPORT_POS){ //pos
        u8 i;
        if(rep_frame->rep_num == 0){
            release_all_finger(ts_data);
            reprot_state_clr = 1;
        }
        else{
            u8 touch_down = 0;
            for(i = 0; i < rep_frame->rep_num; i++){
                HYN_INFO2("id,%d,xy,%d,%d",rep_frame->pos_info[i].pos_id,rep_frame->pos_info[i].pos_x,rep_frame->pos_info[i].pos_y);
                if(dt->swap_xy){
                    xpos = rep_frame->pos_info[i].pos_y;
                    ypos = rep_frame->pos_info[i].pos_x;
                }
                else{
                    xpos = rep_frame->pos_info[i].pos_x;
                    ypos = rep_frame->pos_info[i].pos_y;
                }
                if(ypos > dt->y_resolution || xpos > dt->x_resolution){
                    HYN_ERROR("Please check dts or FW config,maybe resolution or origin issue!!!");
                }
                if(rep_frame->pos_info[i].pos_id >= ts_data->plat_data.max_touch_num){
                    continue;
                }
                if(dt->reverse_x){
                    xpos = dt->x_resolution-xpos;
                }
                if(dt->reverse_y){
                    ypos = dt->y_resolution-ypos;
                }
                rep_frame->pos_info[i].pos_x = xpos;
                rep_frame->pos_info[i].pos_y = ypos;
                touch_updata(i,rep_frame->pos_info[i].event? 1:0);
                if(rep_frame->pos_info[i].event) touch_down++;
            }
            if(touch_down==0){
                reprot_state_clr = 1;
                input_report_key(dev, BTN_TOUCH, 0);
#if HYN_MT_PROTOCOL_B_EN==0
                input_mt_sync(dev);
#endif
            }
        }
        input_sync(dev);
    }
    if(rep_frame->report_need & REPORT_PROX){
        hyn_proximity_report(ts_data->prox_state);
        reprot_state_clr = 1;
    }
#if (HYN_GESTURE_EN)
    if(rep_frame->report_need & REPORT_GES){
        hyn_gesture_report(ts_data);
        reprot_state_clr = 1;
    }
#endif 
    if(reprot_state_clr){
        rep_frame->report_need = REPORT_NONE;
    }
    mutex_unlock(&ts_data->mutex_report);
}

static int hyn_restore_scene(void)
{
    int ret = 0;
    HYN_ENTER();
    if(hyn_data->prox_is_enable){
        ret |= hyn_fun->tp_prox_handle(1);
    }
    else if(hyn_data->gesture_is_enable && hyn_data->state_is_sunpend){
        ret |= hyn_fun->tp_set_workmode(GESTURE_MODE,1);
    }
    if(hyn_data->charge_is_enable){
        ret |= hyn_fun->tp_set_workmode(CHARGE_ENTER,1);
    }
    if(hyn_data->glove_is_enable){
        ret |= hyn_fun->tp_set_workmode(GLOVE_ENTER,1);
    }
    return ret;
}

static void hyn_esdcheck_work(struct work_struct *work)
{
#if ESD_CHECK_EN
    int ret;
    HYN_ENTER(); 
    if(hyn_data->esd_block_cnt==0){
        ret = hyn_fun->tp_check_esd();
    #if ESD_READ_TIME_EN
        if(hyn_data->esd_last_value != ret){
            hyn_data->esd_fail_cnt = 0;
            hyn_data->esd_last_value = ret;
        }
        else
            hyn_data->esd_fail_cnt++;
			
        if(hyn_data->esd_fail_cnt > 2){
                hyn_data->esd_fail_cnt = 0;
    #else
        if (ret) {
            if (ret == 2) 
    #endif
            {
                HYN_INFO("esd check fail");
                hyn_power_source_ctrl(hyn_data,0);
                mdelay(1);
                hyn_power_source_ctrl(hyn_data,1);
                hyn_fun->tp_rest();
                mdelay(50);
            }
            hyn_restore_scene();
        }
    }
    else{
        hyn_data->esd_block_cnt--;
    }
    queue_delayed_work(hyn_data->hyn_workqueue, &hyn_data->esdcheck_work,
                           msecs_to_jiffies(1000));
#endif
}

static void hyn_resum(struct device *dev)
{
    int ret = 0;
    struct ts_frame *rep_frame;
    struct hyn_plat_data *dt;
    HYN_ENTER();
    if(IS_ERR_OR_NULL(hyn_data)){
        return;
    }
    hyn_data->state_is_sunpend = 0;
    rep_frame = &hyn_data->rp_buf;
    dt = &hyn_data->plat_data;
#if (HYN_WAKE_LOCK_EN==1)
    wake_unlock(&hyn_data->tp_wakelock);
#endif
    hyn_power_source_ctrl(hyn_data, 1);
    hyn_fun->tp_resum();
    //restore_scene
    hyn_restore_scene();
    if(hyn_data->gesture_is_enable && hyn_data->prox_is_enable==0){
        hyn_irq_set(hyn_data,DISABLE);
        ret = disable_irq_wake(hyn_data->client->irq);
        ret |= irq_set_irq_type(hyn_data->client->irq,dt->irq_gpio_flags); 
        if(ret < 0){
            HYN_ERROR("gesture irq_set_irq failed");
        }  
    }

    
    rep_frame->report_need = REPORT_NONE;
    hyn_irq_set(hyn_data,ENABLE);
}

static void hyn_suspend(struct device *dev)
{
    int ret = 0;
    HYN_ENTER();
    if(IS_ERR_OR_NULL(hyn_data)){
        return;
    }
    hyn_data->state_is_sunpend = 1;
#if (HYN_WAKE_LOCK_EN==1)
    wake_lock(&hyn_data->tp_wakelock);
#endif
    if(hyn_data->prox_is_enable ==1){
    }
    else if(hyn_data->gesture_is_enable){
        hyn_irq_set(hyn_data,DISABLE);
        ret = enable_irq_wake(hyn_data->client->irq);
        ret |= irq_set_irq_type(hyn_data->client->irq,IRQF_TRIGGER_FALLING|IRQF_NO_SUSPEND|IRQF_ONESHOT); 
        if(ret < 0){
            HYN_ERROR("gesture irq_set_irq failed");
        }  
        hyn_fun->tp_set_workmode(GESTURE_MODE,1);
        hyn_irq_set(hyn_data,ENABLE);
        hyn_power_source_ctrl(hyn_data, 1);
    }
    else{
        hyn_irq_set(hyn_data,DISABLE);
        hyn_fun->tp_supend();
        //hyn_power_source_ctrl(hyn_data, 0);
    }
    //compensate for lifting
    release_all_finger(hyn_data);
    input_sync(hyn_data->input_dev);
}

static void hyn_updata_fw_work(struct work_struct *work)
{
    int ret = 0;
    if(!IS_ERR_OR_NULL(hyn_data->fw_updata_addr)){
        ret = hyn_fun->tp_updata_fw(hyn_data->fw_updata_addr,hyn_data->fw_updata_len);
    }
    else{
        HYN_ERROR("fw_updata_addr is erro");
    }
}

static irqreturn_t hyn_irq_handler(int irq, void *data)
{
    atomic_set(&hyn_data->hyn_irq_flg,1);
    if(hyn_data->work_mode < DIFF_MODE){
        wake_up_interruptible(&hyn_data->wait_report);
    }
    else{
        wake_up(&hyn_data->wait_irq);
    }
    return IRQ_HANDLED;
}


static int touch_event_handler(void *unused)
{
    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
    sched_setscheduler(current, SCHED_RR, &param);
    do {
        set_current_state(TASK_INTERRUPTIBLE);
        wait_event_interruptible(hyn_data->wait_report, atomic_read(&hyn_data->hyn_irq_flg)==1);
        atomic_set(&hyn_data->hyn_irq_flg,0);
        set_current_state(TASK_RUNNING);
		hyn_data->esd_block_cnt = 2;
        hyn_irq_report();
    } while (!kthread_should_stop());
    return 0;
}

#ifdef I2C_PORT
static int hyn_ts_remove(struct i2c_client *client);
static int hyn_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
#else

#ifdef CONFIG_BUS_SPI
static struct mt_chip_conf hyn_mt_spi_conf = {
    .setuptime = 100,
    .holdtime = 100,
    .high_time = 25,
    .low_time = 25,
    .cs_idletime = 2,
    .ulthgh_thrsh = 0,
    .cpol = 0,
    .cpha = 0,
    .rx_mlsb = 1,
    .tx_mlsb = 1,
    .tx_endian = 0,
    .rx_endian = 0,
    .com_mod = DMA_TRANSFER,/*FIFO_TRANSFER,*/
    .pause = 1, /*0 : twice cs   1: one cs*/
    .finish_intr = 1,
    .deassert = 0,
    .ulthigh = 0,
    .tckdly = 0,
};
#endif

static int hyn_ts_remove(struct spi_device *client);
static int hyn_ts_probe(struct spi_device *client)
#endif
{
    int ret = 0;
    u16 bus_type;
    struct hyn_ts_data *ts_data = 0;
    struct device_node *node = NULL;

    HYN_ENTER();
    HYN_INFO(HYN_DRIVER_VERSION);
#ifdef I2C_PORT
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        HYN_ERROR("I2C not supported");
        return -ENODEV;
    }
    bus_type = BUS_I2C;
#else
    client->mode = SPI_MODE;
	client->max_speed_hz = SPI_CLOCK_FREQ;
	client->bits_per_word = 8;

#ifdef CONFIG_BUS_SPI
    client->controller_data = (void *)&hyn_mt_spi_conf;
#endif

    if (spi_setup(client) < 0){
        HYN_ERROR("SPI not supported");
        return -ENODEV;
    }
    bus_type = BUS_SPI;
#endif
    if(!IS_ERR_OR_NULL(hyn_data)){
        HYN_ERROR("other dev is insmode");
        return -ENOMEM;
    }
    ts_data = kzalloc(sizeof(*ts_data), GFP_KERNEL);
    if (!ts_data) {
        HYN_ERROR("alloc ts data failed");
        return -ENOMEM;
    }
    ts_data->bus_type = bus_type;
    ts_data->rp_buf.key_id = 0xFF;
    ts_data->work_mode = NOMAL_MODE;
    ts_data->log_level = 0;
    
    hyn_data = ts_data;
    ts_data->client = client;
    ts_data->dev = &client->dev;
    dev_set_drvdata(ts_data->dev, ts_data);

    #if (I2C_USE_DMA==2)
        ts_data->dev->coherent_dma_mask = DMA_BIT_MASK(32);
        ts_data->dma_buff_va = (u8 *)dma_alloc_coherent(ts_data->dev,
                        2048, &ts_data->dma_buff_pa, GFP_KERNEL);
        if (!ts_data->dma_buff_va) {
            HYN_ERROR("Allocate I2C DMA Buffer fail");
        }
    #endif
    tpd->reg = NULL;
    ret = hyn_parse_dt(ts_data);
    if(ret){
        HYN_ERROR("hyn_parse_dt failed");
        goto FREE_RESOURCE;
    }

    ret = hyn_poweron(ts_data);
    if(ret){
        HYN_ERROR("hyn_poweron failed");
        goto FREE_RESOURCE;
    }
    // spin_lock_init(&ts_data->irq_lock);
    mutex_init(&ts_data->mutex_report);
    mutex_init(&ts_data->mutex_bus);
    mutex_init(&ts_data->mutex_fs);
    init_waitqueue_head(&ts_data->wait_irq);
    init_waitqueue_head(&ts_data->wait_report);
    
    ret = hyn_check_ic(ts_data);
    if(ret){
        HYN_ERROR("hyn_check_ic failed");
        goto FREE_RESOURCE;
    }
    INIT_WORK(&ts_data->work_updata_fw,hyn_updata_fw_work);
    INIT_DELAYED_WORK(&ts_data->esdcheck_work,hyn_esdcheck_work);

    ts_data->hyn_workqueue = create_singlethread_workqueue("hyn_wq");
    if (IS_ERR_OR_NULL(ts_data->hyn_workqueue)) {
        HYN_ERROR("create work queue failed");
        goto FREE_RESOURCE;
    }

    ret = hyn_input_dev_init(ts_data);
    if(ret){
        if(!IS_ERR_OR_NULL(ts_data->input_dev)){
            input_set_drvdata(ts_data->input_dev, NULL);
            input_free_device(ts_data->input_dev);
            ts_data->input_dev = NULL;
        }
        HYN_ERROR("hyn_input_dev_init failed");
        goto FREE_RESOURCE;
    }
    ret = input_register_device(ts_data->input_dev);
    if(ret){
        HYN_ERROR("input_register_device failed");
        goto FREE_RESOURCE;
    }

#if (HYN_WAKE_LOCK_EN==1)
    wake_lock_init(&ts_data->tp_wakelock, WAKE_LOCK_SUSPEND, "suspend_tp_lock");
#endif

#if (HYN_GESTURE_EN)
    ret = hyn_gesture_init(ts_data);
    if(ret){
        HYN_ERROR("gesture_init failed");
        goto FREE_RESOURCE;
    }
#endif

    ts_data->thread_tpd = kthread_run(touch_event_handler, 0, TPD_DEVICE);
    if(IS_ERR_OR_NULL(ts_data->thread_tpd)){
        HYN_ERROR("create kernel thread_tpd failed");
        ts_data->thread_tpd = NULL;
        goto FREE_RESOURCE;
    }

    node = tpd->tpd_dev->of_node;
    tpd_gpio_as_int(ts_data->plat_data.irq_gpio);
    ts_data->gpio_irq = irq_of_parse_and_map(node, 0);
    HYN_INFO("ts_data->gpio_irq = %d",ts_data->gpio_irq);
    ts_data->plat_data.irq_gpio_flags = IRQF_TRIGGER_FALLING;
    ret = request_irq(ts_data->gpio_irq,hyn_irq_handler,
                                (IRQF_TRIGGER_FALLING | IRQF_ONESHOT), HYN_DRIVER_NAME, ts_data);
    if(ret){
        HYN_ERROR("request_irq failed");
        goto FREE_RESOURCE;
    }
    atomic_set(&ts_data->irq_is_disable,ENABLE);
    hyn_irq_set(ts_data , DISABLE);

	hyn_create_sysfs(ts_data);
#if (HYN_APK_DEBUG_EN)
    hyn_tool_fs_int(ts_data);
#endif
    hyn_proximity_int(ts_data);
    hyn_irq_set(ts_data,ENABLE);
    hyn_esdcheck_switch(ts_data,ENABLE);

#if HYN_POWER_ON_UPDATA
    if(ts_data->need_updata_fw){
        queue_work(ts_data->hyn_workqueue,&ts_data->work_updata_fw);
    }
#endif
    tpd_load_status = 1;
    return 0;
FREE_RESOURCE:
    hyn_ts_remove(client);
    return -1;
}

#ifdef I2C_PORT
static int hyn_ts_remove(struct i2c_client *client)
#else
static int hyn_ts_remove(struct spi_device *client)
#endif
{
    struct hyn_ts_data *ts_data = hyn_data;
    HYN_ENTER();    
    if(!IS_ERR_OR_NULL(ts_data)){
        if(ts_data->gpio_irq != 0)
            free_irq(ts_data->gpio_irq, ts_data);
        HYN_INFO("ts_remove1");
        if (!IS_ERR_OR_NULL(ts_data->hyn_workqueue)){
            flush_workqueue(ts_data->hyn_workqueue);
            hyn_esdcheck_switch(ts_data,DISABLE);
            destroy_workqueue(ts_data->hyn_workqueue);
        } 
        hyn_proximity_exit();
#if (HYN_GESTURE_EN)
        hyn_gesture_exit(ts_data);
#endif
        HYN_INFO("ts_remove2");
        if(!IS_ERR_OR_NULL(ts_data->thread_tpd)){
            atomic_set(&hyn_data->hyn_irq_flg,1);
            kthread_stop(hyn_data->thread_tpd);
        }
        HYN_INFO("ts_remove3");

        if(!IS_ERR_OR_NULL(ts_data->input_dev)){
            input_unregister_device(ts_data->input_dev);
        }
        // if(gpio_is_valid(ts_data->plat_data.tpd_irg_gpio))
        //     gpio_free(ts_data->plat_data.tpd_irg_gpio);
        HYN_INFO("ts_remove4");
#if (HYN_APK_DEBUG_EN)
        hyn_tool_fs_exit();
#endif
        HYN_INFO("ts_remove6");
        hyn_release_sysfs(ts_data);
        HYN_INFO("ts_remove7");
#if (I2C_USE_DMA==2)
        if(!IS_ERR_OR_NULL(ts_data->dma_buff_va)){
            dma_free_coherent(NULL, 2048, ts_data->dma_buff_va, ts_data->dma_buff_pa);
        }
#endif
        if(!IS_ERR_OR_NULL(tpd->reg)){
            regulator_disable(tpd->reg);
            regulator_put(tpd->reg);
        }
        kfree(ts_data);
        hyn_data = NULL;
        HYN_INFO("ts_remove8");
    }
    return 0;
}

/*****************************************************************************
*  BUS Driver
*****************************************************************************/
#ifdef I2C_PORT
static const struct i2c_device_id hyn_id_table[] = {
    {.name = HYN_DRIVER_NAME, .driver_data = 0,},
    {},
};

static struct i2c_driver hyn_ts_driver = {
    .probe = hyn_ts_probe,
    .remove = hyn_ts_remove,
    .driver = {
        .name = HYN_DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = hyn_of_match_table,
    },
    .id_table = hyn_id_table,
};
#else
static struct spi_driver hyn_ts_driver = {
	.driver = {
		   .name = HYN_DRIVER_NAME,
		   .of_match_table = hyn_of_match_table,
		   .owner = THIS_MODULE,
		   },
	.probe = hyn_ts_probe,
	.remove = hyn_ts_remove,
};
#endif


/*****************************************************************************
*  TPD Device Driver
*****************************************************************************/
static int hyn_tpd_local_init(void)
{
    int ret =0;
#ifdef I2C_PORT  
    ret = i2c_add_driver(&hyn_ts_driver);
#else
    ret = spi_register_driver(&hyn_ts_driver);
#endif
    if (ret) {
        HYN_ERROR("add bus driver failed");
        return -ENODEV;
    }
    if(tpd_load_status==0){
#ifdef I2C_PORT  
        i2c_del_driver(&hyn_ts_driver);
#else
        spi_unregister_driver(&hyn_ts_driver);
#endif
         return -ENODEV;
    }

    if (tpd_dts_data.use_tpd_button) {
        tpd_button_setting(tpd_dts_data.tpd_key_num, tpd_dts_data.tpd_key_local,
                           tpd_dts_data.tpd_key_dim_local);
    }

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
    TPD_DO_WARP = 1;
    memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT * 4);
    memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT * 4);
#endif

#if (defined(CONFIG_TPD_HAVE_CALIBRATION) && !defined(CONFIG_TPD_CUSTOM_CALIBRATION))
    memcpy(tpd_calmat, tpd_def_calmat_local_factory, 8 * 4);
    memcpy(tpd_def_calmat, tpd_def_calmat_local_factory, 8 * 4);

    memcpy(tpd_calmat, tpd_def_calmat_local_normal, 8 * 4);
    memcpy(tpd_def_calmat, tpd_def_calmat_local_normal, 8 * 4);
#endif

    tpd_type_cap = 1;
    return -1;    
}


static struct tpd_driver_t tpd_device_driver =
{
    .tpd_device_name = HYN_DRIVER_NAME,
    .tpd_local_init = hyn_tpd_local_init,
    .suspend = hyn_suspend,
    .resume = hyn_resum,
};


static int __init hyn_ts_init(void)
{
    HYN_ENTER();
    tpd_get_dts_info();
    HYN_INFO("tpd max:%d key enable:%d",tpd_dts_data.touch_max_num,tpd_dts_data.use_tpd_button);
    if(tpd_driver_add(&tpd_device_driver) < 0){
        HYN_INFO("add tpd driver failed");
    }
    return 0;
}

static void __exit hyn_ts_exit(void)
{
    HYN_ENTER();

#ifdef I2C_PORT  
    i2c_del_driver(&hyn_ts_driver);
#else
    spi_unregister_driver(&hyn_ts_driver);
#endif
}

module_init(hyn_ts_init);
module_exit(hyn_ts_exit);

MODULE_AUTHOR("Hynitron Driver Team");
MODULE_DESCRIPTION("Hynitron Touchscreen Driver");
MODULE_LICENSE("GPL v2");


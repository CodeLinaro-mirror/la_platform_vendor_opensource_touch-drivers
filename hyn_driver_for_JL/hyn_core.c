#include "hyn_core.h"

static struct hyn_ts_data *hyn_data = NULL;

void hyn_delay_ms(int cnt)
{
    int delay = (cnt + 9) / 10;
    os_time_dly(delay);
}

/**gpio ctl*/
int gpio_set_value(uint32_t gpio_id,bool vlue)
{
    gpio_direction_output(gpio_id,vlue);
    return 0;
}   
bool gpio_get_value(uint32_t gpio_id) 
{
 return gpio_read(gpio_id);
}

static int touch_int_handler()
{
    int ret;
    hyn_data->hyn_irq_flg = 1;
    if(hyn_data->work_mode < DIFF_MODE){
        ret = hyn_data->hyn_fuc_used->tp_report(); //读点

        for(u8 i=0; i< hyn_data->plat_data.max_touch_num; i++){   //根据配置修改坐标原点
            if(hyn_data->plat_data.swap_xy){
                u16 tmp = hyn_data->rp_buf.pos_info[i].pos_x;
                hyn_data->rp_buf.pos_info[i].pos_x = hyn_data->rp_buf.pos_info[i].pos_y;
                hyn_data->rp_buf.pos_info[i].pos_y = tmp;
            }
            if(hyn_data->plat_data.reverse_x)
                hyn_data->rp_buf.pos_info[i].pos_x = hyn_data->plat_data.x_resolution-hyn_data->rp_buf.pos_info[i].pos_x;
            if(hyn_data->plat_data.reverse_y)
                hyn_data->rp_buf.pos_info[i].pos_y = hyn_data->plat_data.y_resolution-hyn_data->rp_buf.pos_info[i].pos_y;
        }
        HYN_INFO("ret:%d num:%d xy:(%d,%d)",ret,hyn_data->rp_buf.rep_num,hyn_data->rp_buf.pos_info[0].pos_x,hyn_data->rp_buf.pos_info[0].pos_y);
    }
    hyn_data->rp_buf.report_need = REPORT_NONE;
	return 0;
}

void touch_init()
{
    int ret = 0;
    static struct hyn_ts_data ts_data;
    memset((void*)&ts_data,0,sizeof(ts_data));
    hyn_data = &ts_data;
	HYN_INFO(HYN_DRIVER_VERSION);
/*************************************************************/
//    handle            chip types
//&cst66xx_fuc,},   /*suport 36xx、35xx、66xx、68xx */
//&cst36xxes_fuc,}, /*suport 154es 3654es 3640es*/
//&cst3240_fuc,},   /*suport 3240 */
//&cst923xx_fuc,},   /*suport 9217、9220 、916e、9317、317q、3217 */
//&cst3xx_fuc,},    /*suport 340、348、328、128、140、148*/
//&cst7xx_fuc,},    /*suport 726、826、836u*/
//&cst8xxT_fuc,},   /*suport 816t、816d、820、08C*/
//&cst226se_fuc,}, /*suport 226se 8922*/
//&cst840u_fuc,},   /*suport 840u*/
//&cst76xx_fuc,},   /*suport 7864BG 7964BG HYT7864JL HYT7760BG HYT7760TR CST6960BG*/
//&cst840u_fuc,},   /*suport 840u*/
/*************************************************************/
    hyn_data->hyn_fuc_used = &cst226se_fuc;  //根据芯片型号赋值
    hyn_data->plat_data.max_touch_num = MAX_POINTS_REPORT;   //最大手指数
    hyn_data->plat_data.x_resolution = 480;  //x最大分辨率
    hyn_data->plat_data.y_resolution = 800;  //y最大分辨率
    hyn_data->plat_data.swap_xy = 0;         //xy坐标交换
    hyn_data->plat_data.reverse_x = 0;       //x坐标反向
    hyn_data->plat_data.reverse_y = 0;       //y坐标反向

    hyn_data->plat_data.irq_gpio = xx;    //中断脚配置
    hyn_data->plat_data.reset_gpio = xx;  //rest脚配置

    //配置 int脚为 输入pull up，开启gpio中断
    gpio_direction_input(hyn_data->plat_data.irq_gpio);
    gpio_set_pull_up(hyn_data->plat_data.irq_gpio,HIGH);
    //配置rst 脚为push-pullp输出模式，输出1
    gpio_direction_output(hyn_data->plat_data.reset_gpio,HIGH)
    //初始化I2c master ,配置速率、master addr
    iic_init(ts_data->salve_addr);

    //触摸芯片初始化
    ret = hyn_data->hyn_fuc_used->tp_chip_init(hyn_data); 
    if(ret){
        HYN_ERROR("I2c NAk");
        return;
    }
    HYN_INFO("IC_info fw_project_id:%04x ictype:%04x fw_ver:%x"\
					,hyn_data->hw_info.fw_project_id,hyn_data->hw_info.fw_chip_type,hyn_data->hw_info.fw_ver);

#if HYN_POWER_ON_UPDATA
    if(ts_data.need_updata_fw){
        hyn_data->fw_file_name[0] = 0; //use .h to updata
        hyn_data->hyn_fuc_used->tp_updata_fw(hyn_data->fw_updata_addr,hyn_data->fw_updata_len);
    }
 #endif
    //regest irq callback
    io_ext_interrupt_init(hyn_data->plat_data.irq_gpio, falling_edge, touch_int_handler);
}




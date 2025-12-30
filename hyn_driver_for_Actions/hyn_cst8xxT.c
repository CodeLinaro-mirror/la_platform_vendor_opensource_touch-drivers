#include "hyn_tpkey_acts.h"

#define BOOT_I2C_ADDR   (0x6A)
#define RW_REG_LEN   (2)

#define CST8xxT_BIN_SIZE    (15*1024)
static struct hyn_ts_data *hyn_8xxTdata = NULL;

static int cst8xxT_updata_judge(u8 *p_fw, u16 len);
static u32 cst8xxT_read_checksum(void);
static int cst8xxT_updata_tpinfo(void);
static int cst8xxT_enter_boot(void);
static int cst8xxT_set_workmode(enum work_mode mode,u8 enable);

#if HYN_POWER_ON_UPDATA
#include "cst8xxT_fw.h"
static int cst8xxT_init(struct hyn_ts_data* ts_data)
{
    int ret = 0;
    u8 buf[4];
    HYN_ENTER();
    hyn_8xxTdata = ts_data;
    ret = cst8xxT_enter_boot();
    if(ret == FALSE){
        HYN_ERROR("cst8xxT_enter_boot failed");
        return FALSE;
    }
    hyn_8xxTdata->fw_updata_addr = (u8*)app_bin;
    hyn_8xxTdata->fw_updata_len = CST8xxT_BIN_SIZE;

    hyn_8xxTdata->hw_info.ic_fw_checksum = cst8xxT_read_checksum();
    if(hyn_8xxTdata->need_updata_fw ==0){
        hyn_wr_reg(hyn_8xxTdata,0xA006EE,3,buf,0); //exit boot
        hyn_8xxTdata->hyn_fuc_used->tp_rest();
        hyn_mdelay(50);
        hyn_set_i2c_addr(hyn_8xxTdata,MAIN_I2C_ADDR);
        ret = cst8xxT_updata_tpinfo();
        cst8xxT_set_workmode(NOMAL_MODE,0);
        hyn_8xxTdata->need_updata_fw = cst8xxT_updata_judge((u8*)app_bin,CST8xxT_BIN_SIZE);
    }
    if(hyn_8xxTdata->need_updata_fw){
        HYN_INFO("need updata FW !!!");
    }
    return TRUE;
}
#else
static int cst8xxT_init(struct hyn_ts_data* ts_data)
{
    int ret = 0;
    hyn_8xxTdata = ts_data;
    ret = cst8xxT_enter_boot();
    if(ret == FALSE){
        HYN_ERROR("cst8xxT_enter_boot failed");
        return FALSE;
    }
    hyn_8xxTdata->hyn_fuc_used->tp_rest();
    hyn_mdelay(50);
    hyn_set_i2c_addr(hyn_8xxTdata,MAIN_I2C_ADDR);
    ret = cst8xxT_updata_tpinfo();
    ret |= cst8xxT_set_workmode(NOMAL_MODE,0);
    if(ret == FALSE){
        HYN_ERROR("no firmware,need updata FW.Please open HYN_POWER_ON_UPDATA!!!");
        return FALSE;
    }
    return ret;
}
#endif 


static int  cst8xxT_enter_boot(void)
{
    uint8_t t;
    hyn_set_i2c_addr(hyn_8xxTdata,BOOT_I2C_ADDR);
    for (t = 5;; t += 2)
    {
        int ok = FALSE;
        uint8_t i2c_buf[4] = {0};

        if (t >= 15){
            return FALSE;
        }

        hyn_8xxTdata->hyn_fuc_used->tp_rest();
        hyn_mdelay(t);

        ok = hyn_wr_reg(hyn_8xxTdata, 0xA001AB, 3, i2c_buf, 0);
        if(ok == FALSE){
            continue;
        }

        ok = hyn_wr_reg(hyn_8xxTdata, 0xA003,  2, i2c_buf, 1);
        if(ok == FALSE){
            continue;
        }

        if (i2c_buf[0] != 0xC1){
            continue;
        }
        break;
    }
    return TRUE;
}


static int write_code(u8 *bin_addr,uint8_t retry)
{
    uint16_t i,t;//,j;
    int ok = FALSE;
    u8 i2c_buf[512+2];
	
	bin_addr+=6;
	
    for ( i = 0;i < CST8xxT_BIN_SIZE; i += 512)
    {
        i2c_buf[0] = 0xA0;
        i2c_buf[1] = 0x14;
        i2c_buf[2] = i;
        i2c_buf[3] = i >> 8;
        ok = hyn_write_data(hyn_8xxTdata, i2c_buf,RW_REG_LEN, 4);
        if (ok == FALSE){
            break;
        }

        i2c_buf[0] = 0xA0;
        i2c_buf[1] = 0x18;
		if(0 == hyn_8xxTdata->fw_file_name[0]){
			memcpy(i2c_buf + 2, bin_addr + i, 512); 
		}
		// else{
		// 	ok = copy_for_updata(hyn_8xxTdata,i2c_buf + 2,i+6,512);
		// 	if(ok)break;
		// }
        ok = hyn_write_data(hyn_8xxTdata, i2c_buf,RW_REG_LEN, 514);
        if (ok == FALSE){
            break;
        }

        ok = hyn_wr_reg(hyn_8xxTdata, 0xA004EE, 3,i2c_buf,0);
        if (ok == FALSE){
            break;
        }

        hyn_mdelay(100 * retry);

        for (t = 0;; t ++)
        {
            if (t >= 50){
                return FALSE;
            }
            hyn_mdelay(5);

            ok = hyn_wr_reg(hyn_8xxTdata,0xA005,2,i2c_buf,1);
            if (ok == FALSE){
                continue;
            }
            if (i2c_buf[0] != 0x55){
                continue;
            }
            break;
        }
    }
    return ok;
}


static uint32_t cst8xxT_read_checksum(void)
{
    int ok = FALSE,t;
    uint8_t i2c_buf[4] = {0};
    uint32_t value = 0;
    int chip_checksum_ok = FALSE;
    // firmware checksum
    ok = hyn_wr_reg(hyn_8xxTdata, 0xA00300,  3, i2c_buf, 0);
    if (ok == FALSE){
        return value;
    }

    hyn_mdelay(100);

    for (t = 0;; t += 10)
    {
        if (t >= 1000){
            //return FALSE;
            break;
        }

        hyn_mdelay(10);

        ok = hyn_wr_reg(hyn_8xxTdata, 0xA000,  2, i2c_buf, 1);
        if (ok == FALSE){
            continue;
        }

        if (i2c_buf[0] == 1){
            chip_checksum_ok = TRUE;
            break;
        }
        else if (i2c_buf[0] == 2){
            chip_checksum_ok = FALSE;
            continue;
        }
    }

    if(chip_checksum_ok == FALSE){
        hyn_8xxTdata->need_updata_fw = 1;
    }
    else{
        ok = hyn_wr_reg(hyn_8xxTdata, 0xA008,  2, i2c_buf, 2);
        if (ok == FALSE){
            //return FALSE;
            return value;
        }
        value = i2c_buf[0];
        value |= (uint16_t)(i2c_buf[1]) << 8;
    }

    return value;
}


static int cst8xxT_updata_fw(u8 *bin_addr, u16 len)
{ 
    int retry = 0;
    int ok_copy = TRUE;
    int ok = FALSE;
    u8 i2c_buf[4];
    u32 fw_checksum = 0;
    // len = len;
    HYN_ENTER();
    if(0 == hyn_8xxTdata->fw_file_name[0]){
        fw_checksum =U8TO16(bin_addr[5],bin_addr[4]);
    }
    // else{
    //     ok = copy_for_updata(hyn_8xxTdata,i2c_buf,4,2);
    //     if(ok)  goto UPDATA_END;
    //     fw_checksum = U8TO16(i2c_buf[1],i2c_buf[0]);
    // }
    hyn_irq_set(hyn_8xxTdata,DISABLE);

    for(retry = 1; retry<10; retry++){
        ok = cst8xxT_enter_boot();
        if (ok == FALSE){
            continue;
        }

        ok = write_code(bin_addr,retry);
        if (ok == FALSE){
            continue;
        }

        hyn_8xxTdata->hw_info.ic_fw_checksum = cst8xxT_read_checksum();
        if(fw_checksum != hyn_8xxTdata->hw_info.ic_fw_checksum){
            continue;
        }
            
        if(retry>=5){
            ok_copy = FALSE;
            break;
        }
        break;
    }
    // UPDATA_END:
    hyn_wr_reg(hyn_8xxTdata,0xA006EE,3,i2c_buf,0); //exit boot
    hyn_mdelay(2);
    hyn_8xxTdata->hyn_fuc_used->tp_rest();
    hyn_mdelay(50);

    hyn_set_i2c_addr(hyn_8xxTdata,MAIN_I2C_ADDR);   
    if(ok_copy == TRUE){
        cst8xxT_updata_tpinfo();
        HYN_INFO("updata_fw success");
    }
    else{
        HYN_ERROR("updata_fw failed");
    }

    hyn_irq_set(hyn_8xxTdata,ENABLE);

    return ok_copy;
}

static int cst8xxT_updata_tpinfo(void)
{
    u8 buf[8];
    struct tp_info *ic = &hyn_8xxTdata->hw_info;
    int ret = 0;

    ret = hyn_wr_reg(hyn_8xxTdata,0xA7,1,buf,4);
    if(ret == FALSE){
        HYN_ERROR("cst8xxT_updata_tpinfo failed");
        return FALSE;
    }

    ic->fw_sensor_txnum = CUSTOM_SENSOR_NUM_TX;
    ic->fw_sensor_rxnum = CUSTOM_SENSOR_NUM_RX;
    ic->fw_key_num = hyn_8xxTdata->plat_data.key_num;
    ic->fw_res_y = hyn_8xxTdata->plat_data.y_resolution;
    ic->fw_res_x = hyn_8xxTdata->plat_data.x_resolution;
    ic->fw_project_id = buf[1];
    ic->fw_chip_type = buf[0];
    ic->fw_ver = buf[2];

    HYN_INFO("IC_info fw_project_id:%04x ictype:%04x fw_ver:%x checksum:%#x",ic->fw_project_id,ic->fw_chip_type,ic->fw_ver,ic->ic_fw_checksum);
    return TRUE;
}

static int cst8xxT_updata_judge(u8 *p_fw, u16 len)
{
    u32 f_checksum,f_fw_ver,f_ictype,f_fw_project_id;
    u8 *p_data = p_fw ; 
    struct tp_info *ic = &hyn_8xxTdata->hw_info;

    f_checksum = U8TO16(p_data[5],p_data[4]);

    p_data = p_fw + 6 + CST8xxT_BIN_SIZE - 1 - 15 ; 

    f_ictype = p_data[0];
    f_fw_project_id = (p_data[1]<<24) + (p_data[2]<<16) + (p_data[3]<<8) + (p_data[4]<<0);
    f_fw_ver = p_data[5];

    HYN_INFO("Bin_info fw_project_id:%04x ictype:%04x fw_ver:%x checksum:%#x",f_fw_project_id,f_ictype,f_fw_ver,f_checksum);

    if(f_checksum != ic->ic_fw_checksum && f_fw_ver >= ic->fw_ver){
        return 1; //need updata
    }
    return 0;
}

//------------------------------------------------------------------------------//

static int cst8xxT_set_workmode(enum work_mode mode,u8 enable)
{
    hyn_8xxTdata->work_mode = mode;
    if(mode != NOMAL_MODE){
        hyn_esdcheck_switch(hyn_8xxTdata,DISABLE);
    }
    if(hyn_wr_reg(hyn_8xxTdata,0x00,1,NULL,0)){ //check_lp mode
        hyn_8xxTdata->hyn_fuc_used->tp_rest();
        hyn_mdelay(80);
    }
    switch(mode){
        case NOMAL_MODE:
			hyn_esdcheck_switch(hyn_8xxTdata,ENABLE);
            hyn_irq_set(hyn_8xxTdata,ENABLE);
            break;
        case GESTURE_MODE:
            hyn_wr_reg(hyn_8xxTdata,0xE501,2,NULL,0);
            break;
        case LP_MODE:
            break;
        case DIFF_MODE:
            hyn_wr_reg(hyn_8xxTdata,0xFE07,2,NULL,0);
            hyn_wr_reg(hyn_8xxTdata,0xFEF6,2,NULL,0);
            hyn_wr_reg(hyn_8xxTdata,0xFEF4,2,NULL,0);
            hyn_wr_reg(hyn_8xxTdata,0xFA80,2,NULL,0);
            break;
        case RAWDATA_MODE:
            hyn_wr_reg(hyn_8xxTdata,0xFE07,2,NULL,0);
            hyn_wr_reg(hyn_8xxTdata,0xFEF5,2,NULL,0);
            hyn_wr_reg(hyn_8xxTdata,0xFEF4,2,NULL,0);
            hyn_wr_reg(hyn_8xxTdata,0xFA80,2,NULL,0);
            break;
        case FAC_TEST_MODE:
            //hyn_wr_reg(hyn_8xxTdata,0xD119,2,NULL,0);
            break;
        case DEEPSLEEP:
            hyn_irq_set(hyn_8xxTdata,DISABLE);
            hyn_wr_reg(hyn_8xxTdata,0xE503,2,NULL,0);
            break;
        default :
            //hyn_esdcheck_switch(hyn_8xxTdata,ENABLE);
            hyn_8xxTdata->work_mode = NOMAL_MODE;
            break;
    }
    return 0;
}

static int cst8xxT_supend(void)
{
    HYN_ENTER();
    cst8xxT_set_workmode(DEEPSLEEP,0);
    return 0;
}

static int cst8xxT_resum(void)
{
    hyn_8xxTdata->hyn_fuc_used->tp_rest();
    hyn_msleep(50);
    cst8xxT_set_workmode(NOMAL_MODE,0);
    return 0;
}


static int cst8xxT_report(uint8_t *i2c_buf)
{
    uint8_t i = 0;
    uint16_t x,y;
    uint8_t id = 0,index = 0;
    struct hyn_plat_data *dt = &hyn_8xxTdata->plat_data;

    memset(&hyn_8xxTdata->rp_buf,0,sizeof(hyn_8xxTdata->rp_buf));
    hyn_8xxTdata->rp_buf.report_need = REPORT_NONE;

    if(hyn_8xxTdata->work_mode == GESTURE_MODE){
        hyn_8xxTdata->gesture_id  = IDX_NULL;
        if(i2c_buf[1] == 0x05){ //click
            hyn_8xxTdata->gesture_id = IDX_POWER;
            hyn_8xxTdata->rp_buf.report_need = REPORT_GES;
        }
    }
    else{
        if (i2c_buf[1] == 0xAA) { //click
            hyn_8xxTdata->gesture_id = IDX_O;
            hyn_8xxTdata->rp_buf.report_need = REPORT_GES;
        }
        hyn_8xxTdata->rp_buf.rep_num  = i2c_buf[2];
        // HYN_INFO("rep_num = %d",hyn_8xxTdata->rp_buf.rep_num);
        for(i = 0 ; i < MAX_POINTS_REPORT ; i++)
        {
            id = (i2c_buf[5 + i*6] & 0xf0)>>4;
            if(id > 1) continue;

            x = (i2c_buf[3 + i*6] & 0x0f);
            x = (x<<8) + i2c_buf[4 + i*6];

            y = (i2c_buf[5 + i*6] & 0x0f);
            y = (y<<8) + i2c_buf[6 + i*6];

            hyn_8xxTdata->rp_buf.pos_info[index].pos_id = id;
            hyn_8xxTdata->rp_buf.pos_info[index].event = (i2c_buf[3 + i*6] & 0x40) ? 0:1;
            hyn_8xxTdata->rp_buf.pos_info[index].pos_x = x ;
            hyn_8xxTdata->rp_buf.pos_info[index].pos_y = y ;
            // hyn_8xxTdata->rp_buf.pos_info[index].pres_z = (i2c_buf[7 + i*6] <<8) + i2c_buf[8 + i*6] ;
            hyn_8xxTdata->rp_buf.pos_info[index].pres_z = 3+(x&0x03); //press mast chang
            index++;
        }
        if(index != 0) hyn_8xxTdata->rp_buf.report_need = REPORT_POS;
        if(dt->key_num){
            i = dt->key_num;
            while(i){
                i--;
                    if(dt->key_y_coords ==hyn_8xxTdata->rp_buf.pos_info[0].pos_y && dt->key_x_coords[i] == hyn_8xxTdata->rp_buf.pos_info[0].pos_x){
                        hyn_8xxTdata->rp_buf.key_id = i;
                        hyn_8xxTdata->rp_buf.key_state = hyn_8xxTdata->rp_buf.pos_info[0].event;
                        hyn_8xxTdata->rp_buf.report_need = REPORT_KEY;
                }
            }
        }
    }
    return TRUE;
}

static u32 cst8xxT_check_esd(void)
{
    uint8_t data = 0xFF;
    uint8_t retry = 3,flag = 0;
    int ret = -1;
    while(retry--) {
        ret = hyn_wr_reg(hyn_8xxTdata, 0x00, 1, &data, 1);
        if (0 == ret && data != 0xFF) {
            flag = 1;
            break;
        }
    }
    if (0 == flag) {
        const struct device *dev = tpkey_dev_get();
        _hyn_poweron(dev, false);
        hyn_msleep(10);
        _hyn_poweron(dev, true);
        hyn_msleep(10);
    }
    return TRUE;
}

static int cst8xxT_prox_handle(u8 cmd)
{
    return TRUE;
}


static int cst8xxT_get_dbg_data(u8 *buf, u16 len)
{
    uint16_t data = 0;
    uint8_t i = 0;
    HYN_DEBUG("[HYN] tp debug:");
    for(i=0;i<len;i++) {
        data = ((uint16_t)buf[i]<<8) + buf[i+1];
        HYN_DEBUG("%d,",data);
    }
    HYN_DEBUG("\n");
    
    return 0;
}

const u16 cp_value[CUSTOM_SENSOR_ALL] = {
    15000,15000,15000,15000,15000,15000,15000,
    15000,15000,15000,15000,15000,15000,15000,15000,15000
};
const u16 delta_value[CUSTOM_SENSOR_ALL] = {
    8000,8000,8000,8000,8000,8000,8000,
    8000,8000,8000,8000,8000,8000,8000,8000,8000
};
const u8 white_node[CUSTOM_SENSOR_ALL] = {
    1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,0
};
const u16 cmod_value = 350;
static int cst8xxT_get_test_result(u16 *cp_buf, u16 *delta_buf)
{
    uint8_t cmd_cfg1[5] = FAC_CFG1;
    uint8_t cmd_cfg2[5] = FAC_CFG2;
    uint8_t write_cmd = 0;
    uint8_t data = 0xFF;
    uint8_t short_data[CUSTOM_SENSOR_ALL] = {0};
    uint8_t i = 0;
    uint8_t err_code = 0;
    union
    {
        uint8_t buff_u8[2*2];
        uint16_t buff_u16[2];
    } cbuf = { .buff_u16 = 0};
    union
    {
        uint8_t buff_u8[CUSTOM_SENSOR_ALL*2];
        uint16_t buff_u16[CUSTOM_SENSOR_ALL];
    } cp_data = { .buff_u16 = 0};
    union
    {
        uint8_t buff_u8[CUSTOM_SENSOR_ALL*2];
        uint16_t buff_u16[CUSTOM_SENSOR_ALL];
    } delta_data = { .buff_u16 = 0};
    int ret = -1;
    // read cp and cbuf
    hyn_8xxTdata->hyn_fuc_used->tp_rest();
    hyn_msleep(30);
    ret = hyn_write_data(hyn_8xxTdata, cmd_cfg1, 0, 5);
    ret |= hyn_write_data(hyn_8xxTdata, cmd_cfg1, 0, 5);
    hyn_msleep(10);
    ret |= hyn_wr_reg(hyn_8xxTdata, 0xFF01, 2, &data, 0);
    hyn_msleep(200);
    ret |= hyn_wr_reg(hyn_8xxTdata, 0xFF, 1, &data, 1);
    hyn_msleep(20);
    write_cmd = 0xB0;
    ret |= hyn_wr_reg(hyn_8xxTdata, write_cmd, 1, cbuf.buff_u8, sizeof(cbuf.buff_u8));
    hyn_msleep(20);
    write_cmd = 0x40;
    ret |= hyn_wr_reg(hyn_8xxTdata, write_cmd, 1, cp_data.buff_u8, sizeof(cp_data.buff_u8));
    if (ret) {
        HYN_ERROR("read cp and cbuf");
        return -1;
    }
    cbuf.buff_u16[0] = SWAP_HL(cbuf.buff_u16[0]);
    cbuf.buff_u16[1] = SWAP_HL(cbuf.buff_u16[1]);
    if (cbuf.buff_u16[0]<cmod_value*0.7 || cbuf.buff_u16[0]>cmod_value*1.3) {
        err_code |= 0x10;
        HYN_ERROR("cmod sensor-%d:%d",i,cbuf.buff_u16[0]);
    } else {
        HYN_INFO("cmod sensor-%d:%d",i,cbuf.buff_u16[0]);
    }
    if (cbuf.buff_u16[1]<cmod_value*0.7 || cbuf.buff_u16[1]>cmod_value*1.3) {
        err_code |= 0x20;
        HYN_ERROR("cmod sensor-%d:%d",i,cbuf.buff_u16[1]);
    } else {
        HYN_INFO("cmod sensor-%d:%d",i,cbuf.buff_u16[1]);
    }
    for(i=0;i<CUSTOM_SENSOR_ALL;i++) {
        cp_data.buff_u16[i] = SWAP_HL(cp_data.buff_u16[i]);
        if (white_node[i] && (cp_data.buff_u16[i] < cp_value[i]*0.5 || cp_data.buff_u16[i] < cp_value[i]*1.5)) {
            HYN_ERROR("cp sensor-%d:%d",i,cp_data.buff_u16[1]);
            err_code |= 0x01;
        } else {
            HYN_INFO("cp sensor-%d:%d",i,cp_data.buff_u16[1]);
        }
    }
    // read delta and short
    hyn_8xxTdata->hyn_fuc_used->tp_rest();
    hyn_msleep(30);
    ret = hyn_write_data(hyn_8xxTdata, cmd_cfg2, 0, 5);
    ret |= hyn_write_data(hyn_8xxTdata, cmd_cfg2, 0, 5);
    hyn_msleep(10);
    ret |= hyn_wr_reg(hyn_8xxTdata, 0xFF01, 2, &data, 0);
    hyn_msleep(200);
    ret |= hyn_wr_reg(hyn_8xxTdata, 0xFF, 1, &data, 1);
    hyn_msleep(20);
    write_cmd = 0x40;
    ret |= hyn_wr_reg(hyn_8xxTdata, write_cmd, 1, delta_data.buff_u8, sizeof(delta_data.buff_u8));
    hyn_msleep(20);
    write_cmd = 0x80;
    ret |= hyn_wr_reg(hyn_8xxTdata, write_cmd, 1, short_data, sizeof(short_data));
    if (ret) {
        HYN_ERROR("read dalta and short");
        return -1;
    }
    for(i=0;i<CUSTOM_SENSOR_ALL;i++) {
        delta_data.buff_u16[i] = SWAP_HL(delta_data.buff_u16[i]);
        if (white_node[i] && (delta_data.buff_u16[i] < delta_value[i]*0.5 || delta_data.buff_u16[i] < delta_value[i]*1.5)) {
            HYN_ERROR("delta sensor-%d:%d",i,delta_data.buff_u16[1]);
            err_code |= 0x02;
        } else {
            HYN_INFO("delta sensor-%d:%d",i,delta_data.buff_u16[1]);
        }
    }
    for(i=0;i<CUSTOM_SENSOR_ALL;i++) {
        if (white_node[i] && short_data[i] == 0) {
            HYN_ERROR("short sensor-%d:%d",i,short_data[1]);
            err_code |= 0x04;
        } else {
            HYN_INFO("short sensor-%d:%d",i,short_data[1]);
        }
    }
    memcmp(cp_buf, cp_data.buff_u16, CUSTOM_SENSOR_ALL*2);
    memcmp(delta_buf, delta_data.buff_u16, CUSTOM_SENSOR_ALL*2);

    hyn_8xxTdata->hyn_fuc_used->tp_rest();
    hyn_msleep(50);
    if (err_code) {
        return -1;
    }
    return 0;
}

const struct hyn_ts_fuc cst8xxT_fuc = {
    .tp_rest = _hyn_reset,
    .tp_report = cst8xxT_report,
    .tp_supend = cst8xxT_supend,
    .tp_resum = cst8xxT_resum,
    .tp_chip_init = cst8xxT_init,
    .tp_updata_fw = cst8xxT_updata_fw,
    .tp_set_workmode = cst8xxT_set_workmode,
    .tp_check_esd = cst8xxT_check_esd,
    .tp_prox_handle = cst8xxT_prox_handle,
    .tp_get_dbg_data = cst8xxT_get_dbg_data,
    .tp_get_test_result = cst8xxT_get_test_result
};



#include "../hyn_core.h"
#include "cst923xx_fw.h"

#define BOOT_I2C_ADDR   (0x5A)
#define MAIN_I2C_ADDR   (0x5A)
#define RW_REG_LEN   (2)

#define MAX_FINGER (2)
#define MODULE_ID_EN  (0)

enum chip_group_t{
    CHIP_92xx = 0,
    CHIP_93xx = 1
};

#define ADDR_CHIP_ID            (0x077C)
#define ADDR_MODULE_ID          (0x7FC0)

static struct {
    u16 bin_size;
    u16 page_size;
    u16 check_sum_offset;
}ct92xx_addr_tabl[2]={
    {
        0x7F80, 128, 0x7F6C  //92xx
    },
    {
        0x7E00, 512, 0x7DFC  //93xx
    }
};

static u8 chip_group_id = CHIP_92xx;
static struct hyn_ts_data *hyn_92xxdata = NULL;

static int cst923xx_updata_judge(u8 *p_fw, u16 len);
static int cst923xx_updata_tpinfo(void);
static int cst923xx_enter_boot(void);
static void cst923xx_rst(void);
static int16_t read_word_from_mem(uint8_t type, uint16_t addr, uint32_t *value);
static int cst923xx_set_workmode(enum work_mode mode,u8 enable);

static const struct hyn_chip_series cst923xx_fw_list[] = {
    {0x9fff,0x0000,"cst9xxx",(u8*)fw_bin0},//default bin
    {0x9317,0x0000,"cst9317 id0",(u8*)fw_bin0},
    {0x92FF,0x0000,"cst92xx id0",(u8*)fw_bin1}, 
    {0x9217,0x0001,"cst9217 id0",(u8*)fw_bin1},  
    {0x9217,0x0002,"cst9217 id1",(u8*)fw_bin1},
    {0x9220,0x0002,"cst9220 id0",(u8*)fw_bin1},
    {0x916e,0x0003,"cst916e",(u8*)fw_bin1},
    {0xFF,0,"null",NULL}
};

static int cst923xx_init(struct hyn_ts_data* ts_data)
{
    int ret = 0;
    struct tp_info *ic = &ts_data->hw_info;
    HYN_ENTER();
    hyn_92xxdata = ts_data;
    hyn_set_i2c_addr(hyn_92xxdata,MAIN_I2C_ADDR);
    cst923xx_rst();
    msleep(40);
    ret = cst923xx_updata_tpinfo();
    if(ret){
        int retry = 3;
        HYN_INFO("cst923xx_updata_tpinfo failed");
        hyn_92xxdata->work_mode=ENTER_BOOT_MODE;
        ret = cst923xx_enter_boot();
        if(ret){
            HYN_ERROR("cst923xx_ic check failed\r\n");
            return FALSE;
        }
        while(retry--){
            ret = read_word_from_mem(1, ADDR_CHIP_ID, &ic->fw_chip_type);
            ret |= read_word_from_mem(0, ADDR_MODULE_ID, &ic->fw_module_id);
            if(ret==0)
            break;
        }
        cst923xx_rst();
    }
    ret = 0;
    HYN_INFO("CHIP_GROUP:[%s]  MODULE_ID is %s\r\n",chip_group_id==CHIP_92xx ? "92xx":"93xx", MODULE_ID_EN ? "enable":"disable");
    hyn_92xxdata->fw_updata_addr = cst923xx_fw_list[0].fw_bin;
    hyn_92xxdata->fw_updata_len = ct92xx_addr_tabl[chip_group_id].bin_size;
#if MODULE_ID_EN
    {
        u8 i = 0;
        ret=-1;
        for(i = 0; ;i++){
            if(cst923xx_fw_list[i].fw_bin== NULL){
                break;
            }
            if(cst923xx_fw_list[i].moudle_id == ic->fw_module_id){
                hyn_92xxdata->fw_updata_addr = cst923xx_fw_list[i].fw_bin;
                ret = 0;
                break;
            }
        }
        HYN_INFO("module id:0x%04x match fw %s\n",ic->fw_module_id,ret ? "faild":"success");
    }
#endif
    if(ret==0){
        hyn_92xxdata->need_updata_fw = cst923xx_updata_judge(hyn_92xxdata->fw_updata_addr,hyn_92xxdata->fw_updata_len);
    }
    HYN_INFO("cst923xx_init done !!!");
    return TRUE;
}


static int  cst923xx_enter_boot(void)
{
    #define START_MS    (6) //6ms
    int ok = FALSE,t,retry = 0;
    uint8_t i2c_buf[4] = {0};
    hyn_set_i2c_addr(hyn_92xxdata,BOOT_I2C_ADDR);
    for (t = START_MS;; t += 2)
    {
        if(t >= 30){
            return FALSE;
        }
        cst923xx_rst();
        mdelay(t);
        ok = hyn_wr_reg(hyn_92xxdata, 0xA001A2, 3, i2c_buf, 0); //93xx
        ok |= hyn_wr_reg(hyn_92xxdata, 0xA001AA, 3, i2c_buf, 0); //92xx
        if(ok == FALSE){
            continue;
        }
        mdelay(1);
        ok = hyn_wr_reg(hyn_92xxdata, 0xA002,  2, i2c_buf, 2);
        if(ok == FALSE || (i2c_buf[0]==0xA5 && i2c_buf[1]==0xA5)){
            if(ok==TRUE && retry<4){ //miss boot win
                retry++;
                t = START_MS-2-retry;
            }
            continue;
        }
        if (i2c_buf[0] == 0x55 && (i2c_buf[1] == 0xB0 || i2c_buf[1] == 0xB2)) {
            if(i2c_buf[1] == 0xB2){
                chip_group_id = CHIP_93xx;
            }
            break;
        }
    }
    ok = hyn_wr_reg(hyn_92xxdata, 0xA00100, 3, i2c_buf, 0);
    if(ok == FALSE){
        return FALSE;
    }
    return TRUE;
}

static int erase_all_mem(void)
{
    int ok = FALSE,t;
    u8 i2c_buf[8];
    if(chip_group_id == CHIP_93xx){
        return 0;
    }
	//erase_all_mem
    ok = hyn_wr_reg(hyn_92xxdata, 0xA0140000, 4, i2c_buf, 0);
    ok |= hyn_wr_reg(hyn_92xxdata, 0xA00C807F, 4, i2c_buf, 0);
    ok |= hyn_wr_reg(hyn_92xxdata, 0xA004EC, 3, i2c_buf, 0);
    if (ok == FALSE){
        return FALSE;
    }
    mdelay(400);
    for (t = 0;; t += 10) {
        if (t >= 1000) {
           return FALSE;
        }
        mdelay(10);
        ok = hyn_wr_reg(hyn_92xxdata, 0xA005, 2, i2c_buf, 1);
        if (ok == FALSE) {
            continue;
        }
        if (i2c_buf[0] == 0x88) {
            break;
        }
    }
    return TRUE;
}

//
static int write92xx_mem_page(uint16_t addr, uint8_t *buf, uint16_t len)
{
    int ok = FALSE,t;
    uint8_t i2c_buf[1024+2] = {0};

    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x0C;
    i2c_buf[2] = len;
    i2c_buf[3] = len >> 8;
    //ok = hyn_i2c_write_r16(HYN_BOOT_I2C_ADDR, 0xA00C, i2c_buf, 2);
    ok = hyn_write_data(hyn_92xxdata, i2c_buf,RW_REG_LEN, 4);

    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x14;
    i2c_buf[2] = addr;
    i2c_buf[3] = addr >> 8;
    ok |= hyn_write_data(hyn_92xxdata, i2c_buf,RW_REG_LEN, 4);
    if(ok == FALSE) {
        return FALSE;
    }

    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x18;
    memcpy(i2c_buf + 2, buf, len);            
    ok = hyn_write_data(hyn_92xxdata, i2c_buf,RW_REG_LEN, len+2);
    if(ok == FALSE){
        return FALSE;
    }

    ok =  hyn_wr_reg(hyn_92xxdata,0xA004EE,3,i2c_buf,0);
    if(ok == FALSE){
        return FALSE;
    }

    for (t = 0;; t += 10) {
        if (t >= 1000) {
            return FALSE;
        }
        mdelay(5);
        ok =  hyn_wr_reg(hyn_92xxdata,0xA005,2,i2c_buf,1);
        if (i2c_buf[0] == 0x55 && ok==TRUE) {
            break;
        }
    }

    return TRUE;
}

static int write93xx_mem_page(uint16_t addr, uint8_t *buf, uint16_t len)
{
    int ok = FALSE,t;
    uint16_t page_idx = addr/ct92xx_addr_tabl[chip_group_id].page_size;
    uint8_t sram_buf[514] = {0};

    sram_buf[0] = (page_idx&0x01) ? 0xA2:0xA0;//dev_addr;
    sram_buf[1] = 0x18;
    memcpy(sram_buf + 2, buf, len);            
    ok = hyn_write_data(hyn_92xxdata, sram_buf, RW_REG_LEN, len+2);  //512 + 2 
    if(ok == FALSE){
        return FALSE;
    }

    memcpy(sram_buf,(u8[]){0xA0,0x14,0x00,0x00,0x00,0x50},6);
    sram_buf[3] = page_idx * 2;
    ok = hyn_write_data(hyn_92xxdata, sram_buf, RW_REG_LEN, 6); //addr

    memcpy(sram_buf,(u8[]){0xA0,0x0C,0x00,0x02,0x00,0x00},6);
    ok |= hyn_write_data(hyn_92xxdata, sram_buf, RW_REG_LEN, 6); //ctl

    memcpy(sram_buf,(u8[]){0xA0,0x0E,0x00, 0x00,0x00,0x00},6);
    sram_buf[3] = (page_idx&0x01) ? 0x02:0x00;;
    ok |= hyn_write_data(hyn_92xxdata, sram_buf, RW_REG_LEN, 6); //cfg

    ok |=  hyn_wr_reg(hyn_92xxdata, 0xA004EE, 3, NULL, 0);
    if(ok == FALSE){
        return FALSE;
    }
    mdelay(100);
    for (t = 0;; t += 10) {
        uint8_t i2c_buf[2] = {0};
        if (t >= 1000) {
            return FALSE;
        }
        mdelay(5);
        ok =  hyn_wr_reg(hyn_92xxdata,0xA005,2,i2c_buf,1);      
        if (i2c_buf[0] == 0x55 && ok==0) {
            break; 
        }
    }
    return TRUE;
}


static int write_code(u8 *bin_addr,uint8_t retry)
{
    uint8_t data[512];//= (uint8_t *)bin_addr;
    uint16_t addr = 0;
    uint16_t remain_len = ct92xx_addr_tabl[chip_group_id].bin_size;
    uint16_t page_size = ct92xx_addr_tabl[chip_group_id].page_size;
    int ret = 0;
    int (*write_mem_page)(uint16_t addr, uint8_t *buf, uint16_t len);

    write_mem_page = chip_group_id==CHIP_92xx ? write92xx_mem_page : write93xx_mem_page;
    while (remain_len > 0) {
        uint16_t cur_len = remain_len;
        if (cur_len > page_size) {
            cur_len = page_size;
        }
        
        if(0 == hyn_92xxdata->fw_file_name[0]){
            memcpy(data, bin_addr + addr, page_size);
        }else{
            ret = copy_for_updata(hyn_92xxdata,data,addr,page_size);
            if(ret == FALSE){
                HYN_ERROR("copy_for_updata error");
                goto wite_end;
            }
        }
        //HYN_INFO("write_code addr 0x%x 0x%x",addr,*data);
        if (write_mem_page(addr, data, cur_len) ==  FALSE) {
            ret = -2;
             goto wite_end;
        }
        //data += cur_len;
        addr += cur_len;
        remain_len -= cur_len;
    }
    if(chip_group_id==CHIP_93xx){ //ram to flash
        ret  =  hyn_write_data(hyn_92xxdata, (u8[]){0xA0,0x0C,0x00,0x00,0x00,0x50}, RW_REG_LEN, 6);
        ret |=  hyn_write_data(hyn_92xxdata, (u8[]){0xA0,0x10,0xFC,0x7D,0x00,0x50}, RW_REG_LEN, 6);
        ret |=  hyn_write_data(hyn_92xxdata, (u8[]){0xA0,0x04,0xE0}, RW_REG_LEN, 3);
    }
    wite_end:
    return ret;
}


static int cst923xx_read_checksum(u32*checksum)
{
    int ok = FALSE;
    uint8_t i2c_buf[4] = {0};
    uint8_t retry = 3 ,time_out = 0;
    
    while(--retry){
        if(chip_group_id==CHIP_92xx){//92xx
            ok =  hyn_wr_reg(hyn_92xxdata,0xA00300,3,i2c_buf,0);
        }
        else{ //93xx
            ok =  hyn_wr_reg(hyn_92xxdata,0xA001A2,3,i2c_buf,0);
            ok |=  hyn_wr_reg(hyn_92xxdata,0xA00355,3,i2c_buf,0);
        }
        if(ok){
            continue;
        }
        mdelay(2); 
        time_out = 5;
        while(time_out--){
            mdelay(5);
            ok =  hyn_wr_reg(hyn_92xxdata,0xA000,2,i2c_buf,1);
            if(ok ==0 && i2c_buf[0]!=0){ //92:0x01 93:0x88
                break;
            }
            ok = -2;
        }
        ok |= hyn_wr_reg(hyn_92xxdata,0xA008,2,i2c_buf,4);
        if(ok == 0){
            *checksum = U8TO32(i2c_buf[3],i2c_buf[2],i2c_buf[1],i2c_buf[0]);
        }
    }
    return ok;
}


static int cst923xx_updata_fw(u8 *bin_addr, u32 len)
{ 
    int retry = 0;
    int ok = FALSE;
    u8 i2c_buf[4];
    u16 checksum_addr = ct92xx_addr_tabl[chip_group_id].check_sum_offset;
    u32 fw_checksum=0;
    HYN_ENTER();

    if(len < ct92xx_addr_tabl[chip_group_id].bin_size){
        HYN_ERROR("len = %d",len);
        goto UPDATA_END;
    }
    if(len > ct92xx_addr_tabl[chip_group_id].bin_size) len = ct92xx_addr_tabl[chip_group_id].bin_size;

    if(0 != hyn_92xxdata->fw_file_name[0]){
        //node to update
        ok = copy_for_updata(hyn_92xxdata,i2c_buf,checksum_addr,4);
        fw_checksum = U8TO32(i2c_buf[3],i2c_buf[2],i2c_buf[1],i2c_buf[0]);
        if(hyn_92xxdata->hw_info.ic_fw_checksum == fw_checksum || ok != 0){
             HYN_INFO("no update,fw_checksum is same:0x%04x",fw_checksum);
             goto UPDATA_END;
        }
    }else{
        fw_checksum = U8TO32(bin_addr[checksum_addr+3],bin_addr[checksum_addr+2],bin_addr[checksum_addr+1],bin_addr[checksum_addr+0]);
    }
    HYN_INFO("updating fw checksum:0x%04x",fw_checksum);

    hyn_irq_set(hyn_92xxdata,DISABLE);
    hyn_esdcheck_switch(hyn_92xxdata,DISABLE);

    HYN_INFO("updata_fw start");
    for(retry = 1; retry<5; retry++){
        hyn_92xxdata->fw_updata_process = 0;
        ok = cst923xx_enter_boot();
        if (ok == FALSE){
            HYN_ERROR("updata enter boot faild");
            continue;
        }
        hyn_92xxdata->fw_updata_process = 10;
        ok = erase_all_mem();
        if (ok == FALSE){
            HYN_ERROR("updata erase flash faild");
            continue;
        }
        hyn_92xxdata->fw_updata_process = 20;
        ok = write_code(bin_addr,retry);
        if (ok == FALSE){
            HYN_ERROR("updata write flash faild");
            continue;
        }
        hyn_92xxdata->fw_updata_process = 30;
        cst923xx_read_checksum(&hyn_92xxdata->hw_info.ic_fw_checksum);
        if(fw_checksum != hyn_92xxdata->hw_info.ic_fw_checksum){
            HYN_INFO("out data fw checksum err:0x%04x",hyn_92xxdata->hw_info.ic_fw_checksum);
            hyn_92xxdata->fw_updata_process |= 0x80;
            ok = -4;   
            continue;
        }
        else{
            hyn_92xxdata->fw_updata_process = 100;
            break;
        }
    }

    hyn_wr_reg(hyn_92xxdata,0xA006EE,3,i2c_buf,0); //exit boot
    mdelay(2);

UPDATA_END:   
    cst923xx_rst();
    mdelay(50);
    hyn_set_i2c_addr(hyn_92xxdata,MAIN_I2C_ADDR);   
    if(ok == TRUE){
        cst923xx_updata_tpinfo();
    }
    hyn_irq_set(hyn_92xxdata,ENABLE);

    HYN_INFO("updata_fw %s",ok == TRUE  ? "success":"failed");
    return ok;
}

static int16_t read_word_from_mem(uint8_t type, uint16_t addr, uint32_t *value)
{
    int16_t ret = 0;
    uint8_t i2c_buf[4] = {0},t;

    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x10;
    i2c_buf[2] = type;
    ret = hyn_write_data(hyn_92xxdata,i2c_buf,2,3); 
    if (ret){
        return -1;
    }

    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x0C;
    i2c_buf[2] = addr;
    i2c_buf[3] = addr >> 8;
    ret = hyn_write_data(hyn_92xxdata,i2c_buf,2,4); 
    if (ret){
        return -1;
    }

    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x04;
    i2c_buf[2] = 0xE4;
    ret = hyn_write_data(hyn_92xxdata,i2c_buf,2,3); 
    if (ret){
        return -1;
    }

    for (t = 0;; t++)
    {
        if (t >= 100)
        {
            return -1;
        }
        ret =hyn_wr_reg(hyn_92xxdata,0xA004,2,i2c_buf,1);
        if (ret)
        {
            continue;
        }
        if (i2c_buf[0] == 0x00)
        {
            break;
        }
    }
    ret =hyn_wr_reg(hyn_92xxdata,0xA018,2,i2c_buf,4);
    if (ret){
        return -1;
    }
    *value = U8TO32(i2c_buf[3],i2c_buf[2],i2c_buf[1],i2c_buf[0]);
    return 0;
}


static int cst923xx_updata_tpinfo(void)
{
    u8 buf[30],retry=8;
    struct tp_info *ic = &hyn_92xxdata->hw_info;
    int ret = 0;
    hyn_92xxdata->boot_is_pass = 0;
    while(retry--){
        cst923xx_set_workmode(NOMAL_MODE,0); //wakeup ic
        ret = hyn_wr_reg(hyn_92xxdata,0xD101,2,buf,0);
        if(ret) continue;
        // tx rx res
        ret = hyn_wr_reg(hyn_92xxdata,0xD1F4,2,buf,8);     
        // partno  projectid fw_ver
        ret |= hyn_wr_reg(hyn_92xxdata,0xD204,2,buf+8,8);  
        // module_id
        ret |= hyn_wr_reg(hyn_92xxdata,0xD220,2,buf+16,4); 
        // checksum
        ret |= hyn_wr_reg(hyn_92xxdata,0xD228,2,buf+20,4); 
        // chipid
        ret |= hyn_wr_reg(hyn_92xxdata,0xD224,2,buf+24,4);
        if(ret == 0 && buf[11]==buf[25] && buf[26]==0xCA && buf[27]==0xCA){
            break;
        }
        ret = -1;
    }
    if(ret){
         HYN_ERROR("updata_tpinfo faild:%d",ret);
         return FALSE;
    }
    hyn_92xxdata->boot_is_pass = 1;

    if(buf[11]==0x93 || buf[11]==0x32){ 
        chip_group_id = CHIP_93xx;
    }
    //tx_num   rx_num   key_num
    ic->fw_sensor_txnum = ((uint16_t)buf[1]<<8) + buf[0];
    ic->fw_sensor_rxnum = buf[2];
    ic->fw_key_num = buf[3];
    ic->fw_res_y = (buf[7]<<8)|buf[6];
    ic->fw_res_x = (buf[5]<<8)|buf[4];

    //firmware_version
    ic->fw_project_id = ((uint16_t)buf[9] <<8) + buf[8];
    ic->fw_chip_type = ((uint16_t)buf[11] <<8) + buf[10];
    ic->fw_ver = (buf[15]<<24)|(buf[14]<<16)|(buf[13]<<8)|buf[12];

    //module_id
    ic->fw_module_id = U8TO32(buf[19],buf[18],buf[17],buf[16]);

    //fw_checksum
    ic->ic_fw_checksum = (buf[23]<<24)|(buf[22]<<16)|(buf[21]<<8)|buf[20];

    HYN_INFO("ic tx_num:%d rx_num:%d res_x:%d res_y:%d",ic->fw_sensor_txnum,ic->fw_sensor_rxnum,ic->fw_res_x,ic->fw_res_y);
    HYN_INFO("IC_info project_id:%#04x ictype:%#04x fw_ver:%#04x checksum:%#x",ic->fw_project_id,ic->fw_chip_type,ic->fw_ver,ic->ic_fw_checksum);
   
    cst923xx_set_workmode(NOMAL_MODE,ENABLE);
   
    return 0;
}

static int cst923xx_updata_judge(u8 *p_fw, u16 len)
{
    u32 f_checksum,f_fw_ver,f_ictype,f_fw_project_id;
    u8 *p_data = p_fw + len - 28; 
    struct tp_info *ic = &hyn_92xxdata->hw_info;

    f_fw_project_id = U8TO16(p_data[1],p_data[0]);
    f_ictype = U8TO16(p_data[3],p_data[2]);
    f_fw_ver = U8TO32(p_data[7],p_data[6],p_data[5],p_data[4]);

    p_data = p_fw + ct92xx_addr_tabl[chip_group_id].check_sum_offset;
    f_checksum = U8TO32(p_data[3],p_data[2],p_data[1],p_data[0]);

    HYN_INFO("Bin_info project_id:0x%04x ictype:0x%04x fw_ver:0x%x checksum:0x%x",f_fw_project_id,f_ictype,f_fw_ver,f_checksum);
    if(
        (f_checksum != ic->ic_fw_checksum && f_fw_ver >= ic->fw_ver && f_ictype == ic->fw_chip_type)
        || (hyn_92xxdata->boot_is_pass==0) //empty chip
    ){
        HYN_INFO("need update!");
        return 1; //need updata
    }
    HYN_INFO("cst923xx_updata_judge done, no need update");
    return 0;
}

//------------------------------------------------------------------------------//

static int cst923xx_set_workmode(enum work_mode mode,u8 enable)
{
    int ok = FALSE;
    uint8_t i2c_buf[4] = {0};
    uint8_t i = 0;

    for(i=0;i<3;i++){
        ok = hyn_wr_reg(hyn_92xxdata,0xD11E,2,NULL,0);
        udelay(200);
        ok |= hyn_wr_reg(hyn_92xxdata,0x0002,2,i2c_buf,2);
        if(ok==TRUE && i2c_buf[1] == 0x1E){
            break;
        }     
    }    
    hyn_esdcheck_switch(hyn_92xxdata,enable);
    msleep(1);
    switch(mode){
        case NOMAL_MODE:
            ok = hyn_wr_reg(hyn_92xxdata,0xD109,2,NULL,0);
            break;
        case GESTURE_MODE:
            ok = hyn_wr_reg(hyn_92xxdata,0xD104,2,NULL,0);
            break;
        case LP_MODE:
            ok = hyn_wr_reg(hyn_92xxdata,0xD107,2,NULL,0);
            break;
        case DIFF_MODE:
            ok = hyn_wr_reg(hyn_92xxdata,0xD10D,2,NULL,0);
            break;
        case RAWDATA_MODE:
            ok = hyn_wr_reg(hyn_92xxdata,0xD10A,2,NULL,0);
            break;
        case FAC_TEST_MODE:
            hyn_wr_reg(hyn_92xxdata,0xD114,2,NULL,0);
            msleep(50);
            break;
        case CHARGE_EXIT:
        case CHARGE_ENTER:
            hyn_92xxdata->charge_is_enable = mode&0x01;
            ok = hyn_wr_reg(hyn_92xxdata,(mode&0x01)? 0xD11f:0xD120,2,0,0); //charg mode
            mode = hyn_92xxdata->work_mode; //not switch work mode
            HYN_INFO("set_charge:%d",hyn_92xxdata->charge_is_enable);
            break;
        case GLOVE_EXIT:
        case GLOVE_ENTER:
            hyn_92xxdata->glove_is_enable = mode&0x01;
            ok = hyn_wr_reg(hyn_92xxdata,(mode&0x01)? 0xD126:0xD127,2,0,0); //glove mode
            mode = hyn_92xxdata->work_mode; //not switch work mode
            HYN_INFO("set_glove:%d",hyn_92xxdata->glove_is_enable);
            break;
        case ENTER_BOOT_MODE:
            ok = cst923xx_enter_boot();
            break;
        case DEEPSLEEP:
            hyn_wr_reg(hyn_92xxdata,0xD105,2,NULL,0);
            break;
        default :
            hyn_92xxdata->work_mode = NOMAL_MODE;
            ok = -2;
            break;
    }
    if(ok != -2){
        hyn_92xxdata->work_mode = mode;
    }
    HYN_INFO("set_workmode:%d ret:%d",mode,ok);
    return ok;
}

static void cst923xx_rst(void)
{
    if(hyn_92xxdata->work_mode==ENTER_BOOT_MODE){
        hyn_set_i2c_addr(hyn_92xxdata,MAIN_I2C_ADDR);
    }
    gpio_set_value(hyn_92xxdata->plat_data.reset_gpio,0);
    msleep(8);
    gpio_set_value(hyn_92xxdata->plat_data.reset_gpio,1);
}

static int cst923xx_supend(void)
{
    HYN_ENTER();
    cst923xx_set_workmode(DEEPSLEEP,0);
    return 0;
}

static int cst923xx_resum(void)
{
    cst923xx_rst();
    msleep(50);
    cst923xx_set_workmode(NOMAL_MODE,1);
    return 0;
}


static int cst923xx_report(void)
{
    int ret = FALSE;
    uint8_t i2c_buf[MAX_FINGER*5+5] = {0};
    uint8_t finger_num = 0,key_state=0,retry=3,re_len=0,ges_state;

    while(--retry){
        hyn_wr_reg(hyn_92xxdata,0xD000,2,NULL,0); //wakeup
        udelay(200);
        ret = hyn_wr_reg(hyn_92xxdata,0xD000,2,i2c_buf,7);
        if(ret || i2c_buf[6]!=0xAB || i2c_buf[0]==0xAB){
            ret = -1;
            continue;
        }
        finger_num = i2c_buf[5] & 0x7F;
        if(finger_num > MAX_FINGER){
            ret = -2;
            continue;
        }
        key_state = i2c_buf[5]>>7;
        re_len = ((finger_num>1 ? finger_num-1:0)+key_state)*5;
        if(re_len){
            ret = hyn_wr_reg(hyn_92xxdata,0xD007,2,i2c_buf+7,re_len);
            if(ret){
                ret = -2;
                continue;
            }
        }
        ret = hyn_wr_reg(hyn_92xxdata,0xD000AB,3,NULL,0); //write end flag
        if(ret){
            ret = -3;
            continue;
        }
        else{
            break;
        }
    }
    
    if(ret){
        HYN_ERROR("read point erro:%d\r\n",ret);
        hyn_wr_reg(hyn_92xxdata,0xD000AB,3,NULL,0); //write end flag
        return ret;
    }

    ges_state = i2c_buf[4]>>4;
    if(ges_state){ //gesture
        if((i2c_buf[4]&0x80) == 0x80){ //palm
            hyn_92xxdata->gesture_id = IDX_F11;//IDX_Z;// palm 
        }
        else{ //other gesture
            hyn_92xxdata->gesture_id = IDX_POWER;//GESTURE wakeup
        }
        HYN_INFO("gesture buf4:%#x\r\n",i2c_buf[4]);
        hyn_92xxdata->rp_buf.rep_num = 0;
        hyn_92xxdata->rp_buf.report_need |= REPORT_GES;
        return 0;
    }

    if(finger_num+key_state==0){ //release all finger
        hyn_92xxdata->rp_buf.rep_num = 0;
        hyn_92xxdata->rp_buf.report_need |= REPORT_POS;
    }

    if(key_state){
        uint8_t *data_ptr = i2c_buf;
        if(finger_num){
            data_ptr += (finger_num*5+2);
        }
        if( hyn_92xxdata->rp_buf.report_need==REPORT_NONE){
            hyn_92xxdata->rp_buf.report_need |= REPORT_KEY;
        }
        hyn_92xxdata->rp_buf.key_id = (data_ptr[1]>>4)-1; //0x17  0x27  0x37
        hyn_92xxdata->rp_buf.key_state = data_ptr[0]==0x83 ? 1:0; //0x83  0x80
    }

    if(finger_num){
        u8 i = 0,touch_cnt = 0, index = 0,id = 0;
        u8 *data_ptr = i2c_buf;
        if( hyn_92xxdata->rp_buf.report_need==REPORT_NONE){
            hyn_92xxdata->rp_buf.report_need |= REPORT_POS;
        }
        hyn_92xxdata->rp_buf.rep_num = finger_num;
        for (i = 0; i < finger_num; i++) {
            id = data_ptr[0] >> 4;
            hyn_92xxdata->rp_buf.pos_info[index].pos_id = id;
            hyn_92xxdata->rp_buf.pos_info[index].event = (data_ptr[0]&0x0F) ? 1 : 0;  
            hyn_92xxdata->rp_buf.pos_info[index].pos_x = (data_ptr[1] << 4) | (data_ptr[3] >> 4);
            hyn_92xxdata->rp_buf.pos_info[index].pos_y = (data_ptr[2] << 4) | (data_ptr[3] & 0x0F);
            hyn_92xxdata->rp_buf.pos_info[index].pres_z = (data_ptr[4] & 0x7F);
            if(hyn_92xxdata->rp_buf.pos_info[index].event){
                touch_cnt++;
            }
            index++;
            data_ptr += (i==0 ? 7:5);
        }
        if(touch_cnt==0){
            hyn_92xxdata->rp_buf.rep_num = 0;
        }
    }

    return TRUE;
}


static u32 cst923xx_check_esd(void)
{
    int ok = FALSE;
    uint8_t i2c_buf[6], retry;
    uint32_t esd_value = 0;
    retry = 4;
    while (retry--){
        hyn_wr_reg(hyn_92xxdata,0xD048,2,i2c_buf,0);
        udelay(200);
        ok = hyn_wr_reg(hyn_92xxdata,0xD048,2,i2c_buf,1);
        if (ok == 0 &&((i2c_buf[0]&0xF0) == 0x20 || (i2c_buf[0]&0xF0) == 0xA0)&&(i2c_buf[0]&0x0F)<10){
            break;
        }
        else{
            ok = -2;
        }
    }
    if(-2==ok){   //esdcheck failed rest system
        hyn_92xxdata->esd_fail_cnt = 3;
        esd_value = hyn_92xxdata->esd_last_value;
    }
    else{
        esd_value = hyn_92xxdata->esd_last_value+1;
    }
    return esd_value;
}


static int cst923xx_prox_handle(u8 cmd)
{
    return TRUE;
}


static int cst923xx_get_dbg_data(u8 *buf, u16 len)
{
    int ret = -1;  
    u16 read_len = (hyn_92xxdata->hw_info.fw_sensor_txnum * hyn_92xxdata->hw_info.fw_sensor_rxnum)*2;
    u16 total_len = read_len + (hyn_92xxdata->hw_info.fw_sensor_txnum + hyn_92xxdata->hw_info.fw_sensor_rxnum)*2;
    HYN_ENTER();
    if(total_len > len){
        HYN_ERROR("buf too small");
        return -1;
    }
    switch(hyn_92xxdata->work_mode){
        case DIFF_MODE:
        case RAWDATA_MODE:
            ret = hyn_wr_reg(hyn_92xxdata,0x1000,2,buf,read_len); //mt 

            buf += read_len;
            read_len = hyn_92xxdata->hw_info.fw_sensor_rxnum*2;
            ret |= hyn_wr_reg(hyn_92xxdata,0x7000,2,buf,read_len); //rx

            buf += read_len;
            read_len = hyn_92xxdata->hw_info.fw_sensor_txnum*2;
            ret |= hyn_wr_reg(hyn_92xxdata,0x7200,2,buf,read_len); //tx

            ret |= hyn_wr_reg(hyn_92xxdata,0x000500,3,0,0); //end
            break;
        default:
            HYN_ERROR("work_mode:%d",hyn_92xxdata->work_mode);
            break;
    }
    return ret==0 ? total_len:-1;
}


static int cst923xx_get_test_result(u8 *buf, u16 len)
{
    int ret = 0;
    struct tp_info *ic = &hyn_92xxdata->hw_info;
    u16 scap_len = (ic->fw_sensor_txnum + ic->fw_sensor_rxnum)*2;
    u16 mt_len = ic->fw_sensor_rxnum*ic->fw_sensor_txnum*2,i = 0;
    u8 *raw_s;

    HYN_ENTER();
    if((mt_len*3 + scap_len) > len || mt_len==0){
        HYN_ERROR("buf too small");
        return FAC_GET_DATA_FAIL;
    }
    HYN_INFO("---open_higdrv---");
    hyn_wr_reg(hyn_92xxdata,0xD110,2,buf,0); ////test open high
    hyn_wait_irq_timeout(hyn_92xxdata,7000);
    if(hyn_wr_reg(hyn_92xxdata,0x3000,2,buf,mt_len)){ //open high
        ret = FAC_GET_DATA_FAIL;
        HYN_ERROR("read open high failed");
        goto selftest_end;
    }
    hyn_wr_reg(hyn_92xxdata,0x000005,3,buf,0); 

    HYN_INFO("---open_low---");
    hyn_wr_reg(hyn_92xxdata,0xD111,2,buf,0); ////test open low
    hyn_wait_irq_timeout(hyn_92xxdata,7000);
    if(hyn_wr_reg(hyn_92xxdata,0x1000,2,buf + mt_len,mt_len)){ //open low
        ret = FAC_GET_DATA_FAIL;
        HYN_ERROR("read open low failed");
        goto selftest_end;
    }
    hyn_wr_reg(hyn_92xxdata,0x000005,3,buf,0); 

    //short test
    HYN_INFO("---short---");
    hyn_wr_reg(hyn_92xxdata,0xD112,2,buf,0); //// short test
    hyn_wait_irq_timeout(hyn_92xxdata,7000);
    if(hyn_wr_reg(hyn_92xxdata,0x5000,2,buf+(mt_len*2),scap_len)){
        ret = FAC_GET_DATA_FAIL;
        HYN_ERROR("read fac short failed");
        goto selftest_end;
    }
    else{
        raw_s = buf + mt_len*2;
        for(i = 0; i< ic->fw_sensor_rxnum+ic->fw_sensor_txnum; i++){
            u16 tmp_s = U8TO16(*(raw_s+1),*raw_s);
            if(tmp_s != 0){
                tmp_s = 2000/tmp_s;
            }
            *raw_s = tmp_s;
            *(raw_s+1) = tmp_s>>8;
            raw_s += 2;
        }
    }
    //self cap
    hyn_wr_reg(hyn_92xxdata,0xD118,2,buf,0); //// self cap
    hyn_wait_irq_timeout(hyn_92xxdata,7000);
    if(hyn_wr_reg(hyn_92xxdata,0x1000,2,buf+(mt_len*2)+scap_len,scap_len)){
        ret = FAC_GET_DATA_FAIL;
        HYN_ERROR("read fac self cap failed");
        goto selftest_end;
    }

selftest_end:
    cst923xx_resum();
    return ret;
}

const struct hyn_ts_fuc cst923xx_fuc = {
    .tp_rest = cst923xx_rst,
    .tp_report = cst923xx_report,
    .tp_supend = cst923xx_supend,
    .tp_resum = cst923xx_resum,
    .tp_chip_init = cst923xx_init,
    .tp_updata_fw = cst923xx_updata_fw,
    .tp_set_workmode = cst923xx_set_workmode,
    .tp_check_esd = cst923xx_check_esd,
    .tp_prox_handle = cst923xx_prox_handle,
    .tp_get_dbg_data = cst923xx_get_dbg_data,
    .tp_get_test_result = cst923xx_get_test_result
};



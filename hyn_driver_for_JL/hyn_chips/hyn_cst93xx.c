#include "../hyn_core.h"
#include "cst93xx_fw.h"

#define BOOT_I2C_ADDR   (0x5A)
#define MAIN_I2C_ADDR   (0x5A)
#define RW_REG_LEN   (2)

#define CST93XX_BIN_SIZE    (0x7E00)

#define HYNITRON_PROGRAM_PAGE_SIZE (512)

static struct hyn_ts_data *hyn_93xxdata = NULL;

static const struct hyn_chip_series hyn_93xx_fw[] = {
    {0xCACA9317,0xffffffff,"cst9317",(u8*)fw_bin},//if PART_NO_EN==0 use default chip
    {0xCACA9320,0xffffffff,"cst9320",(u8*)fw_bin},
    {0,0,"",NULL}
};

static int cst93xx_updata_judge(u8 *p_fw, u16 len);
static u32 cst93xx_read_checksum(void);
static int cst93xx_updata_tpinfo(void);
static int cst93xx_enter_boot(void);
static void cst93xx_rst(void);
static int cst93xx_set_workmode(enum work_mode mode,u8 enable);
static int cst93xx_read_chip_id(void);
static int cst93xx_judge_module(void);

static int cst93xx_init(struct hyn_ts_data* ts_data)
{
    int ret = 0;
    HYN_ENTER();
    hyn_93xxdata = ts_data;
    hyn_set_i2c_addr(hyn_93xxdata,BOOT_I2C_ADDR);
    if (cst93xx_judge_module()) {
        ret = cst93xx_read_chip_id();
        if(ret == FALSE){
            HYN_INFO("cst93xx_read_chip_id failed");
            return FALSE;
        }
        cst93xx_rst();
        msleep(40);
    }
    ret = cst93xx_updata_tpinfo();
    if(ret == FALSE){
        HYN_INFO("cst93xx_updata_tpinfo failed");
    }

    hyn_93xxdata->fw_updata_addr = (u8*)fw_bin;
    hyn_93xxdata->fw_updata_len = CST93XX_BIN_SIZE;
    hyn_93xxdata->need_updata_fw = cst93xx_updata_judge((u8*)fw_bin,CST93XX_BIN_SIZE);  

    if(hyn_93xxdata->need_updata_fw){
        HYN_INFO("need updata FW !!!");
    }
 
    HYN_INFO("cst93xx_init done !!!");
    return TRUE;
}

static uint32_t get_u32_from_ptr(const void *ptr) {
    uint32_t temp = 0;
    uint8_t *data = (uint8_t *)ptr;
    temp |= *data;
    data++;
    temp |= (uint32_t)(*data) << 8;
    data++;
    temp |= (uint32_t)(*data) << 16;
    data++;
    temp |= (uint32_t)(*data) << 24;
    data++;
    return temp;
}

static int cst93xx_judge_module(void)
{
    int ret = 0;
    uint8_t buf[12];
    uint8_t retry = 3;
    uint32_t partno = 0;
    uint32_t moduleId = 0;
    HYN_ENTER();
    mdelay(18);
    for (; retry > 0; retry--) {
        //module id
        ret = hyn_wr_reg(hyn_93xxdata, 0xD260, 2, buf, 12);
        if(ret){
            continue;
        }
        moduleId = get_u32_from_ptr(buf);
        //partno id
        partno = get_u32_from_ptr(buf + 4);

        //mainCheckSum
        hyn_93xxdata->hw_info.ic_fw_checksum = get_u32_from_ptr(buf + 8);

        if ((partno >> 16) == 0xCACA) {
            partno &= 0xffff;
            break;
        }
    }
    HYN_INFO("moduleId: 0x%04x", moduleId);
    HYN_INFO("partno: 0x%04x", partno);
    HYN_INFO("checksum: 0x%04x", hyn_93xxdata->hw_info.ic_fw_checksum);
    if ((partno != 0x9317) && (partno != 0x9320)) {
        HYN_ERROR("partno error 0x%04x", partno);
        return FALSE;
    }
    return TRUE;
}

static int  cst93xx_enter_boot(void)
{
    int ok = FALSE,t;
    uint8_t i2c_buf[4] = {0};

    for (t = 10;; t += 2)
    {
        if (t >= 30){
            return FALSE;
        }
        cst93xx_rst();
        mdelay(t);
        ok = hyn_wr_reg(hyn_93xxdata, 0xA001A2, 3, i2c_buf, 0);
        if(ok == FALSE){
            continue;
        }
        udelay(1000);
        ok = hyn_wr_reg(hyn_93xxdata, 0xA002,  2, i2c_buf, 2);
        if(ok == FALSE){
            continue;
        }
        if ((i2c_buf[0] == 0x55) && (i2c_buf[1] == 0xB2)) {
            break;
        }
    }
    return TRUE;
}


static int write_mem_page(uint16_t addr, uint8_t *buf, uint16_t len)
{
    int ok = FALSE,t;
    uint8_t i2c_buf[64] = {0};

  	uint8_t dev_addr = (addr % 2 == 0) ? 0xA0 : 0xA2;
    uint8_t sram_buf[514] = {0};
    sram_buf[0] = dev_addr;
    sram_buf[1] = 0x18;

    memcpy(sram_buf + 2, buf, len);            
    ok = hyn_write_data(hyn_93xxdata, sram_buf, RW_REG_LEN, len+2);  //512 + 2 
    if(ok == FALSE){
        return FALSE;
    }

    memset(sram_buf,0,len + 2);

    uint8_t cmd14 = addr * 2;
    uint8_t cmd0E = (addr % 2 == 0) ? 0x00 : 0x02;
    uint8_t addr_buf[6] = {
        0xA0, 0x14, 0x00, cmd14, 0x00, 0x50 
    };
    ok = hyn_write_data(hyn_93xxdata, addr_buf, RW_REG_LEN, sizeof(addr_buf));
    if(ok == FALSE) {
        return FALSE;
    }

	uint8_t ctrl_buf[6] = {
		0xA0, 0x0C, 0x00, 0x02, 0x00, 0x00 
    };
    ok = hyn_write_data(hyn_93xxdata, ctrl_buf, RW_REG_LEN, sizeof(ctrl_buf));
    if(ok == FALSE){
         return FALSE;
    }

    uint8_t cfg_buf[6] = {
        0xA0, 0x0E, 0x00, cmd0E, 0x00, 0x00 
    };
    ok =  hyn_write_data(hyn_93xxdata, cfg_buf, RW_REG_LEN, sizeof(cfg_buf));
    if(ok == FALSE){
        return FALSE;
    }

    ok =  hyn_wr_reg(hyn_93xxdata, 0xA004EE, 3, i2c_buf, 0);
    if(ok == FALSE){
        return FALSE;
    }
    mdelay(100);
    for (t = 0;; t += 10) {
        if (t >= 1000) {
            return FALSE;
        }
        mdelay(5);
        ok =  hyn_wr_reg(hyn_93xxdata,0xA005,2,i2c_buf,1);  //读函数
        if(ok == FALSE){
            continue;
        }        
        if (i2c_buf[0] == 0x55) {
            break; 
        }
    }
    return TRUE;
}


static int16_t finalize_programming(void) 
{
    int ok = FALSE;
    uint8_t i2c_buf[6] = {0};
    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x0C;
    i2c_buf[2] = 0x00;
    i2c_buf[3] = 0x00;
    i2c_buf[4] = 0x00;
    i2c_buf[5] = 0x50;
    ok =  hyn_write_data(hyn_93xxdata, i2c_buf, RW_REG_LEN, sizeof(i2c_buf));
    if(ok == FALSE){
        return FALSE;
    }
    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x10;
    i2c_buf[2] = 0xFC;
    i2c_buf[3] = 0x7D;
    i2c_buf[4] = 0x00;
    i2c_buf[5] = 0x50;
    ok =  hyn_write_data(hyn_93xxdata, i2c_buf, RW_REG_LEN, sizeof(i2c_buf));
    if(ok == FALSE){
        return FALSE;
    }
    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x04;
    i2c_buf[2] = 0xE0;
    ok =  hyn_write_data(hyn_93xxdata, i2c_buf, RW_REG_LEN, sizeof(i2c_buf));
    if(ok == FALSE){
        return FALSE;
    }

    return true;
}

static int write_code(u8 *bin_addr,uint8_t retry)
{
    uint8_t data[HYNITRON_PROGRAM_PAGE_SIZE];
    uint16_t addr = 0;
    uint16_t remain_len = CST93XX_BIN_SIZE;
    int ret;
    int cnt = 0;
   
    while (remain_len > 0) {   
        uint16_t cur_len = remain_len;
        if (cur_len > HYNITRON_PROGRAM_PAGE_SIZE) { 
            cur_len = HYNITRON_PROGRAM_PAGE_SIZE;  
        }
        
        if(0 == hyn_93xxdata->fw_file_name[0]){
            memcpy(data, bin_addr + addr, HYNITRON_PROGRAM_PAGE_SIZE);
        }else{
            ret = copy_for_updata(hyn_93xxdata, data, addr, HYNITRON_PROGRAM_PAGE_SIZE);
            if(ret == FALSE){
                HYN_ERROR("copy_for_updata error");
                return FALSE;   
            }
        }
       // HYN_INFO("write_code addr 0x%x 0x%x",addr,*data);
        if (write_mem_page(cnt, data, cur_len) ==  FALSE) {
            return FALSE;
        }
        cnt++;  
        addr += cur_len;
        remain_len -= cur_len;
    }

    if (!finalize_programming()) {
            HYN_INFO("Finalize failed!");
            return FALSE;
    }
    return TRUE;
}

static uint32_t cst93xx_read_checksum(void)
{
    int ok = FALSE;
    uint8_t i2c_buf[4] = {0};
    uint32_t chip_checksum = 0;
    uint8_t retry = 5;
    
    hyn_93xxdata->boot_is_pass = 0;
    ok =  hyn_wr_reg(hyn_93xxdata, 0xA001A2, 3, i2c_buf, 0);
    if (ok == FALSE) {
        return FALSE;
    }      
    ok =  hyn_wr_reg(hyn_93xxdata, 0xA00355, 3, i2c_buf, 0);
    if (ok == FALSE) {
        return FALSE;
    }      

    mdelay(2);    
    while(retry--){
        mdelay(5);
        ok =  hyn_wr_reg(hyn_93xxdata, 0xA000, 2, i2c_buf, 1);
        if (ok == FALSE) {
            continue;
        }
        if (i2c_buf[0] == 0x88) {
                break;
        }
        if (i2c_buf[0] == 0x00) {
            return FALSE;
        }
    }
    mdelay(1);

    if(i2c_buf[0] == 0x88){
        hyn_93xxdata->boot_is_pass = 1;
        memset(i2c_buf,0,sizeof(i2c_buf));
        ok =  hyn_wr_reg(hyn_93xxdata, 0xA008, 2, i2c_buf, 4);
        if (ok == FALSE) {
            return FALSE;
        }      

        chip_checksum = ((uint32_t)(i2c_buf[0])) |
            (((uint32_t)(i2c_buf[1])) << 8) |
            (((uint32_t)(i2c_buf[2])) << 16) |
            (((uint32_t)(i2c_buf[3])) << 24);
        HYN_INFO("checksum = 0x%x",chip_checksum);
    }
    else{
        hyn_93xxdata->need_updata_fw = 1;
    }
    return chip_checksum;
}


static int cst93xx_updata_fw(u8 *bin_addr, u32 len)
{ 
    #define CHECKSUM_OFFECT  (0x7DFC)       //32k - 512 -4
    int retry = 0;
    int ok_copy = TRUE;
    int ok = FALSE;
    u8 i2c_buf[4];

    u32 fw_checksum=0;
    HYN_ENTER();

    if(len < CST93XX_BIN_SIZE){
        HYN_ERROR("len = %d",len);
        goto UPDATA_END;
    }
    if(len > CST93XX_BIN_SIZE) len = CST93XX_BIN_SIZE;

    if(0 != hyn_93xxdata->fw_file_name[0]){
        //node to update
        ok = copy_for_updata(hyn_93xxdata, i2c_buf, CST93XX_BIN_SIZE - 4, 4);  
        fw_checksum = U8TO32(i2c_buf[3],i2c_buf[2],i2c_buf[1],i2c_buf[0]);
        if(hyn_93xxdata->hw_info.ic_fw_checksum == fw_checksum || ok != 0){ 
             HYN_INFO("no update,fw_checksum is same:0x%04x",fw_checksum);
             goto UPDATA_END;
        }
    }else{
        fw_checksum = U8TO32(bin_addr[CHECKSUM_OFFECT+3],bin_addr[CHECKSUM_OFFECT+2],bin_addr[CHECKSUM_OFFECT+1],bin_addr[CHECKSUM_OFFECT+0]);
    }
    HYN_INFO("updating fw checksum:0x%04x",fw_checksum);

    hyn_irq_set(hyn_93xxdata,DISABLE);
    hyn_esdcheck_switch(hyn_93xxdata,DISABLE);
    hyn_set_i2c_addr(hyn_93xxdata,BOOT_I2C_ADDR);
    
    HYN_INFO("updata_fw start");

    for(retry = 1; retry<5; retry++){
        hyn_93xxdata->fw_updata_process = 0;
        ok = cst93xx_enter_boot();
        if (ok == FALSE){
            continue;
        }
        hyn_93xxdata->fw_updata_process = 20;
        ok = write_code(bin_addr,retry);
        if (ok == FALSE){
            continue;
        }
        hyn_93xxdata->fw_updata_process = 30;
        hyn_93xxdata->hw_info.ic_fw_checksum = cst93xx_read_checksum();
        if(fw_checksum != hyn_93xxdata->hw_info.ic_fw_checksum){
            HYN_INFO("out data fw checksum err:0x%04x",hyn_93xxdata->hw_info.ic_fw_checksum);
            hyn_93xxdata->fw_updata_process |= 0x80;
            continue;
        }
        hyn_93xxdata->fw_updata_process = 100;   
        if(retry>=5){
            ok_copy = FALSE;
            break;
        }
        break;
    }
    hyn_wr_reg(hyn_93xxdata,0xA006EE,3,i2c_buf,0); //exit boot
    mdelay(2);

UPDATA_END:   
    cst93xx_rst();
    mdelay(50);

    hyn_set_i2c_addr(hyn_93xxdata,MAIN_I2C_ADDR);   

    if(ok_copy == TRUE){
        cst93xx_updata_tpinfo();
        HYN_INFO("updata_fw success");
    }
    else{
        HYN_INFO("updata_fw failed");
    }
    hyn_irq_set(hyn_93xxdata,ENABLE);

    return ok_copy;
}   


static int16_t read_word_from_mem(uint8_t type, uint16_t addr, uint32_t *value)
{
    int16_t ret = 0;
    uint8_t i2c_buf[4] = {0},t;

    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x10;
    i2c_buf[2] = type;
    ret = hyn_write_data(hyn_93xxdata,i2c_buf,2,3); 
    if (ret)
    {
        return -1;
    }

    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x0C;
    i2c_buf[2] = addr;
    i2c_buf[3] = addr >> 8;
    ret = hyn_write_data(hyn_93xxdata,i2c_buf,2,4); 
    if (ret)
    {
        return -1;
    }

    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x04;
    i2c_buf[2] = 0xE4;
    ret = hyn_write_data(hyn_93xxdata,i2c_buf,2,3); 
    if (ret)
    {
        return -1;
    }

    for (t = 0;; t++)
    {
        if (t >= 100)
        {
            return -1;
        }
        ret =hyn_wr_reg(hyn_93xxdata,0xA004,2,i2c_buf,1);
        if (ret)
        {
            continue;
        }
        if (i2c_buf[0] == 0x00)
        {
            break;
        }
    }
    ret =hyn_wr_reg(hyn_93xxdata,0xA018,2,i2c_buf,4);
    if (ret)
    {
        return -1;
    }
    *value = ((uint32_t)(i2c_buf[0])) |
             (((uint32_t)(i2c_buf[1])) << 8) |
             (((uint32_t)(i2c_buf[2])) << 16) |
             (((uint32_t)(i2c_buf[3])) << 24);

    return 0;
}


static int cst93xx_read_chip_id(void)
{
    int16_t ret = 0;
    uint8_t retry = 3;
    uint32_t partno_chip_type,module_id;

    ret = cst93xx_enter_boot();
    if (ret == FALSE)
    {
        HYN_ERROR("enter_bootloader error");
        return -1;
    }
    for (; retry > 0; retry--)
    {
        // partno
        ret = read_word_from_mem(1, 0x077C, &partno_chip_type);
        if (ret)
        {
            continue;
        }
        // module id
        ret = read_word_from_mem(0, 0x7FC0, &module_id);
        if (ret)
        {
            continue;
        }
        if ((partno_chip_type >> 16) == 0xCACA)
        {
            partno_chip_type &= 0xffff;
            break;
        }
    }
    cst93xx_rst();
    msleep(30);
    HYN_INFO("partno_chip_type: 0x%04x", partno_chip_type);
    HYN_INFO("module_id: 0x%04x", module_id);
    if ((partno_chip_type != 9317) && (partno_chip_type != 0x9320))
    {
        HYN_ERROR("partno_chip_type error 0x%04x", partno_chip_type);
        //return -1;
    }
    return 0;
}


static int cst93xx_updata_tpinfo(void)
{
    u8 buf[6];
    struct tp_info *ic = &hyn_93xxdata->hw_info;
    int ret = 0;

    cst93xx_set_workmode(0xff,DISABLE);

    ret = hyn_wr_reg(hyn_93xxdata,0xD101,2,buf,0);
    if(ret == FALSE){
        return FALSE;
    }
    mdelay(5);

    //D1FC:BOOT_WINDOW
    ret = hyn_wr_reg(hyn_93xxdata,0xD1FC,2,buf,4);  
    if(ret == FALSE){
        HYN_ERROR("read 0xD1FC error");
        return FALSE;
    }

    memset(buf,0,sizeof(buf));
    //D200:reserved
    ret = hyn_wr_reg(hyn_93xxdata,0xD200,2,buf,4);  
    if(ret == FALSE){
        HYN_ERROR("read 0xD200 error");
        return FALSE;
    }

    memset(buf,0,sizeof(buf));
    //CHIP_INFO   firmware_project_id   firmware_ic_type
    ret = hyn_wr_reg(hyn_93xxdata,0xD204,2,buf,4);  
    if(ret == FALSE){
        HYN_ERROR("read 0xD204 error");
        return FALSE;
    }
    ic->fw_project_id = ((uint16_t)buf[1] << 8) + buf[0];   
    ic->fw_chip_type = ((uint16_t)buf[3] << 8) + buf[2];    

    memset(buf,0,sizeof(buf));

    //firmware_version
    ret = hyn_wr_reg(hyn_93xxdata,0xD208,2,buf,4);  
    if(ret == FALSE){
        HYN_ERROR("read 0xD208 error");
        return FALSE;
    }
    ic->fw_ver = get_u32_from_ptr(buf);  

    memset(buf,0,sizeof(buf));
    //tx_num   rx_num   key_num
    ret = hyn_wr_reg(hyn_93xxdata,0xD1F4,2,buf,4);  
    if(ret == FALSE){
        HYN_ERROR("read 0xD1F4 error");
        return FALSE;
    }
    ic->fw_sensor_txnum = ((uint16_t)buf[1]<<8) + buf[0];   //8
    ic->fw_sensor_rxnum = buf[2];                           //9
    ic->fw_key_num = buf[3];                                

    // ic->fw_res_y = (buf[7]<<8)|buf[6];
    // ic->fw_res_x = (buf[5]<<8)|buf[4];

    //fw_checksum
    memset(buf,0,sizeof(buf));
    ret = hyn_wr_reg(hyn_93xxdata,0xD20C,2,buf,4);  
    if(ret == FALSE){
       	HYN_ERROR("read 0xD20C error");
        return FALSE;
    }
    ic->ic_fw_checksum = ((uint32_t)buf[3] << 24) + ((uint32_t)buf[2] << 16) + ((uint32_t)buf[1] << 8) + buf[0];

    HYN_INFO("IC_info project_id:%04x ictype:%04x fw_ver:%x checksum:%#x",ic->fw_project_id,ic->fw_chip_type,ic->fw_ver,ic->ic_fw_checksum);  //IC_info project_id:5762 ictype:9317 fw_ver:0 checksum:0x0
   
    cst93xx_set_workmode(NOMAL_MODE,ENABLE);
   
    return TRUE;
}

static int cst93xx_updata_judge(u8 *p_fw, u16 len)   
{
    u32 f_checksum,f_fw_ver,f_ictype,f_fw_project_id;
    u8 *p_data = p_fw + len - 4;   //7DFC
    struct tp_info *ic = &hyn_93xxdata->hw_info;
    int ret;

    ret = cst93xx_enter_boot();
    if (ret == FALSE){
        HYN_INFO("cst93xx_enter_boot fail,need update");
        return 1; 
    }
    hyn_93xxdata->hw_info.ic_fw_checksum = cst93xx_read_checksum();
    cst93xx_rst();
    mdelay(40);

    /*调试用  打开可强制更新firmware*/
    //  hyn_93xxdata->boot_is_pass = 0;
    if(hyn_93xxdata->boot_is_pass == 0){
        HYN_INFO("boot_is_pass %d,need force update",hyn_93xxdata->boot_is_pass);
        return 1; //need updata
    }

    f_checksum = U8TO16(p_data[3], p_data[2]);
    f_checksum = (f_checksum << 16)|U8TO16(p_data[1],p_data[0]);

    p_data -= 852;      //7AA8
    f_fw_project_id = U8TO16(p_data[1],p_data[0]);
    f_ictype = U8TO16(p_data[3],p_data[2]);

    f_fw_ver = U8TO16(p_data[7],p_data[6]);
    f_fw_ver = (f_fw_ver<<16)|U8TO16(p_data[5],p_data[4]);


    HYN_INFO("Bin_info project_id:0x%04x ictype:0x%04x fw_ver:0x%x checksum:0x%x",f_fw_project_id,f_ictype,f_fw_ver,f_checksum);
    if(f_ictype != ic->fw_chip_type || f_fw_project_id != ic->fw_project_id){
        HYN_ERROR("not update,please confirm: ic_type 0x%04x,ic_project_id 0x%04x",ic->fw_chip_type,ic->fw_project_id);
        return 0; //not updata
    }
    if(f_checksum != ic->ic_fw_checksum && f_fw_ver > ic->fw_ver){
        HYN_INFO("need update!");
        return 1; //need updata
    }
    HYN_INFO("cst93xx_updata_judge done, no need update");
    return 0;
}

static int cst93xx_set_workmode(enum work_mode mode,u8 enable)
{
    int ok = FALSE;
    uint8_t i2c_buf[4] = {0};
    uint8_t i = 0;
    hyn_93xxdata->work_mode = mode;
    hyn_esdcheck_switch(hyn_93xxdata,enable);
    for(i=0;i<3;i++)
    {
        ok = hyn_wr_reg(hyn_93xxdata,0xD11E,2,i2c_buf,0);
        if (ok == FALSE) {
            msleep(1);
            continue;
        }
        msleep(1);
        ok = hyn_wr_reg(hyn_93xxdata,0x0002,2,i2c_buf,2);
        if (ok == FALSE) {
            msleep(1);
            continue;
        }
        HYN_INFO("buf[0] = 0x%02x buf[1] = 0x%02x", i2c_buf[0],i2c_buf[1]);
        if(i2c_buf[1] == 0x1E){
            break;
        }     
    }    

    switch(mode){
        case NOMAL_MODE:
            hyn_irq_set(hyn_93xxdata,ENABLE);
            // hyn_esdcheck_switch(hyn_93xxdata,enable);
            ok = hyn_wr_reg(hyn_93xxdata,0xD109,2,i2c_buf,0);
            if (ok == FALSE) {
                return FALSE;
            }
            break;
        case XY_MODE:
            // hyn_esdcheck_switch(hyn_93xxdata,enable);
            mode = 0;
            ok = hyn_wr_reg(hyn_93xxdata,0xD11D,2,i2c_buf,0);
            if (ok == FALSE) {
                return FALSE;
            }
            break;
        case GESTURE_MODE:
            // hyn_esdcheck_switch(hyn_93xxdata,enable);
            ok = hyn_wr_reg(hyn_93xxdata,0xD104,2,i2c_buf,0);  //GESTURE_SLEEP
            if (ok == FALSE) {
                return FALSE;
            }
            break;
        case LP_MODE:
            // hyn_esdcheck_switch(hyn_93xxdata,enable);
            ok = hyn_wr_reg(hyn_93xxdata,0xD107,2,i2c_buf,0);   //IDLE_SLEEP
            if (ok == FALSE) {
                return FALSE;
            }
            break;
        case DIFF_MODE:
            ok = hyn_wr_reg(hyn_93xxdata,0xD10D,2,i2c_buf,0);
            if (ok == FALSE) {
                return FALSE;
            }
            break;

        case RAWDATA_MODE:
            ok = hyn_wr_reg(hyn_93xxdata,0xD10A,2,i2c_buf,0);
            if (ok == FALSE) {
                return FALSE;
            }
            break;
        case FAC_TEST_MODE:
            hyn_wr_reg(hyn_93xxdata,0xD114,2,i2c_buf,0);
            break;
        case DEEPSLEEP:
            hyn_wr_reg(hyn_93xxdata,0xD105,2,i2c_buf,0);     //DEEP_SLEEP
            break;
        default :
            hyn_93xxdata->work_mode = NOMAL_MODE;
            break;
    }
    HYN_INFO("cst93xx_set_workmode %d",mode);

    return TRUE;
}

static void cst93xx_rst(void)
{
    HYN_ENTER();
    gpio_set_value(hyn_93xxdata->plat_data.reset_gpio,0);
    msleep(8);
    gpio_set_value(hyn_93xxdata->plat_data.reset_gpio,1);
}



static int cst93xx_supend(void)
{
    HYN_ENTER();
    cst93xx_set_workmode(DEEPSLEEP,0);  
    return 0;
}

static int cst93xx_resum(void)
{
    cst93xx_rst();
    msleep(50);
    cst93xx_set_workmode(NOMAL_MODE,1);
    return 0;
}

static int cst93xx_report(void)
{
    int ok = FALSE;
    uint8_t i2c_buf[MAX_POINTS_REPORT*5+5] = {0};
    uint8_t finger_num = 0;
    uint8_t key_state=0,key_id = 0;
    int i,j;

    hyn_93xxdata->rp_buf.report_need = REPORT_NONE;

    for (i = 0; i < 3; i++) {
        ok = hyn_wr_reg(hyn_93xxdata,0xD000,2,i2c_buf,sizeof(i2c_buf));
        if (ok == FALSE) {
            HYN_INFO("read 0xD000 error retry:%d", i);
            continue;
        }   

        if (i2c_buf[0] == 0xAB || i2c_buf[6] != 0xAB) {
            HYN_INFO("read i2c_buf[0] i2c_buf[6] error 0x%2x 0x%2x retry:%d",i2c_buf[0], i2c_buf[6], i);
            continue;
        }
        finger_num = i2c_buf[5] & 0x7F;
        for (j = 0; j < finger_num; j++) {
            uint8_t shift = (j > 0) ? 2: 0;
            if ((i2c_buf[j*5 + shift + 4] & 0x0F) != \
                ((i2c_buf[j*5 + shift] + \
                  i2c_buf[j*5 + shift + 1] + \
                  i2c_buf[j*5 + shift + 2] + \
                  i2c_buf[j*5 + shift + 3]) & 0x0F)) {
                HYN_INFO("read 0xD000 checksum error retry:%d", i);
                j = -1;
                break;
            }
        }
        if (j == -1) {
            continue;
        }
        
        ok = hyn_wr_reg(hyn_93xxdata,0xD000AB,3,i2c_buf,0);
        if (ok == FALSE) {
            HYN_INFO("read 0xD000AB error retry:%d", i);
            continue;
        }   

        break;
    }
    if (i == 3) {
        HYN_INFO("read 0xD00 error retry");
        return FALSE;
    }

    if (finger_num > MAX_POINTS_REPORT) {
        HYN_INFO("fail finger_num=%d",finger_num);
        return TRUE;
    }
    hyn_93xxdata->rp_buf.rep_num = finger_num;

   
    if ((i2c_buf[5] & 0x80) == 0x80) { // button
        uint8_t *data = i2c_buf + finger_num * 5;
        if (finger_num > 0) {
            data += 2;
        }
        key_state = data[0];//0x83:æŒ‰é”®æœ‰è§¦æ‘?  0x80:æŒ‰é”®æŠ?èµ?
        key_id = data[1]; // data[1]; :0x17  0x27  0x37

        if(key_state&0x80){            
            hyn_93xxdata->rp_buf.report_need |= REPORT_KEY;
            if((key_id == hyn_93xxdata->rp_buf.key_id || 0 == hyn_93xxdata->rp_buf.key_state)&& key_state == 0x83){
                hyn_93xxdata->rp_buf.key_id = key_id;
                hyn_93xxdata->rp_buf.key_state = 1;
            }
            else{
                hyn_93xxdata->rp_buf.key_state = 0;
            }
        }
    }
    else//pos
    {
        uint8_t index = 0,i;
        if((i2c_buf[4]&0xF0) > 0){
            if((i2c_buf[4]&0x80) == 0x80)
                hyn_93xxdata->gesture_id = 14;//KEY_POWER;// palm
            else  if(i2c_buf[4]&0x70)
                hyn_93xxdata->gesture_id = 14;//GESTURE wakeup
            HYN_INFO("i2c_buf[4]=0x%x,gesture_id=0x%x",i2c_buf[4],hyn_93xxdata->gesture_id);
            hyn_93xxdata->rp_buf.report_need |= REPORT_GES;
            return TRUE;
        }
        
        hyn_93xxdata->rp_buf.report_need |= REPORT_POS;
        if (finger_num > 0) {
            uint8_t *data = i2c_buf;
           //uint8_t *data_ges = i2c_buf + finger_num * 5 + 2;
            uint8_t id = data[0] >> 4;
            uint8_t switch_ = data[0] & 0x0F;
            uint16_t x = ((uint16_t)(data[1]) << 4) | (data[3] >> 4);
            uint16_t y = ((uint16_t)(data[2]) << 4) | (data[3] & 0x0F);
            uint16_t z = (data[3] & 0x1F) + 0x03;

            HYN_INFO("finger=%d id=%d x=%d y=%d z=%d",finger_num,id,x,y,z);

            if (id < MAX_POINTS_REPORT) {
                hyn_93xxdata->rp_buf.pos_info[index].pos_id = id;
                hyn_93xxdata->rp_buf.pos_info[index].event = switch_;//(switch_ == 0x06) ? 1 : 0;  
                hyn_93xxdata->rp_buf.pos_info[index].pos_x = x;
                hyn_93xxdata->rp_buf.pos_info[index].pos_y = y;
                hyn_93xxdata->rp_buf.pos_info[index].pres_z = z;
                index++;
            }
            

        }

        for (i = 1; i < finger_num; i++) {
            uint8_t *data = i2c_buf+5*i+2;
            uint8_t id = data[0] >> 4;
            uint8_t switch_ = data[0] & 0x0F;
            uint16_t x = ((uint16_t)(data[1]) << 4) | (data[3] >> 4);
            uint16_t y = ((uint16_t)(data[2]) << 4) | (data[3] & 0x0F);
            uint16_t z = (data[4] & 0x7F);

            if (id < MAX_POINTS_REPORT) {
                hyn_93xxdata->rp_buf.pos_info[index].pos_id = id;
                hyn_93xxdata->rp_buf.pos_info[index].event = switch_;//(switch_ == 0x06) ? 1 : 0;  
                hyn_93xxdata->rp_buf.pos_info[index].pos_x = x;
                hyn_93xxdata->rp_buf.pos_info[index].pos_y = y;
                hyn_93xxdata->rp_buf.pos_info[index].pres_z = z;
                index++;
            }
        }
    }

    return TRUE;
}
#if 0
static u32 cst93xx_check_esd(void)
{
    int ret = 0;
    uint8_t retry=3;
    u8 buf[6];
    HYN_ENTER();
    while (retry--)
    {
        ret = hyn_wr_reg(hyn_93xxdata,0xD040,2,buf,0);
        ret = hyn_wr_reg(hyn_93xxdata,0xD040,2,buf,6);
        if(ret ==0 && hyn_sum16(0xA5,buf,4)==U8TO16(buf[4],buf[5])){
            ret = U8TO32(buf[0],buf[1],buf[2],buf[3]);
            break;
        }
    }
    

    return ret;
}
#endif

static u32 cst93xx_check_esd(void)
{
    int ok = FALSE;
    u8 i2c_buf[6];
    u8 flag = 0,retry = 4;
    u32 esd_value = 0;

    while (retry--) {
        ok = hyn_wr_reg(hyn_93xxdata,0xD048,2,i2c_buf,2);
        if (ok == FALSE){
            HYN_ERROR("ESD read 0xD048 error retry");
            msleep(1);
            continue;
        }
        break;
    }
    HYN_INFO("ESD data:0x%04x,0x%04x", esd_value, hyn_93xxdata->esd_last_value);
    if (retry == 0 || (i2c_buf[0] & 0x0F) > 10) {
        HYN_ERROR("ESD timeout i2c_buf[0]:0x%04x", i2c_buf[0]);
        return 2;
    }

    esd_value = i2c_buf[0] & 0xF0;
    
    if (esd_value == hyn_93xxdata->esd_last_value) {
        return 0;
    }

    switch (hyn_93xxdata->work_mode) {
        case NOMAL_MODE:
        // case XY_MODE:
            flag = esd_value == 0x20 ? 0 : 2;
            break;
        case GESTURE_MODE:
            flag = esd_value == 0xA0 ? 0 : 1;
            break;
        case CHARGE_ENTER:
            flag = esd_value == 0x20 ? 0 : 1;
            break;
        case GLOVE_ENTER:
            flag = esd_value == 0x20 ? 0 : 1;
            break;
        default:
            flag = 2;
            break;
    }
    if (flag) {
        HYN_ERROR("ESD mode error,work_mode:%d esd_value:0x%04x",hyn_93xxdata->work_mode, esd_value);
        return flag;
    }

    hyn_93xxdata->esd_last_value = esd_value;
    return 0;
}

static int cst93xx_prox_handle(u8 cmd)
{
    return TRUE;
}


static int cst93xx_get_dbg_data(u8 *buf, u16 len)
{
    int ret = -1;  

    u16 read_len = (hyn_93xxdata->hw_info.fw_sensor_txnum * hyn_93xxdata->hw_info.fw_sensor_rxnum)*2;
    u16 total_len = read_len + (hyn_93xxdata->hw_info.fw_sensor_txnum + hyn_93xxdata->hw_info.fw_sensor_rxnum)*2;
    HYN_ENTER();
    if(total_len > len){
        HYN_ERROR("buf too small");
        return -1;
    }
    switch(hyn_93xxdata->work_mode){
        case DIFF_MODE:
        case RAWDATA_MODE:
            ret = hyn_wr_reg(hyn_93xxdata,0x1000,2,buf,read_len); //mt 

            buf += read_len;   
            read_len = hyn_93xxdata->hw_info.fw_sensor_rxnum*2;
            ret |= hyn_wr_reg(hyn_93xxdata,0x7000,2,buf,read_len); //rx

            buf += read_len;   
            read_len = hyn_93xxdata->hw_info.fw_sensor_txnum*2;
            ret |= hyn_wr_reg(hyn_93xxdata,0x7200,2,buf,read_len); //tx

            ret |= hyn_wr_reg(hyn_93xxdata,0x000500,3,0,0); //end
            break;
        default:
            HYN_ERROR("work_mode:%d",hyn_93xxdata->work_mode);
            break;
    }
    return ret==0 ? total_len:-1;
}


#ifdef ENABLE_FAC_INI
#define FACTEST_PATH    "/sdcard/hyn_fac_test_cfg.ini"
#define FACTEST_LOG_PATH "/sdcard/hyn_fac_test.log"
#define FACTEST_ITEM      (MULTI_OPEN_TEST|MULTI_SHORT_TEST)

static int cst93xx_get_test_result(u8 *buf, u16 len)
{
    int ret = 0,timeout;
    struct tp_info *ic = NULL;
    u16 scap_len = 0;
    u16 mt_len = 0;
    u16 *raw_s;

    cst93xx_rst();
    msleep(40);
    ret = cst93xx_updata_tpinfo();
    if (ret == FALSE)
    {
        HYN_ERROR("get_firmware_info failed");
        return -1;
    }

    ic = &hyn_93xxdata->hw_info;
    scap_len = (ic->fw_sensor_txnum + ic->fw_sensor_rxnum)*2;
    mt_len = ic->fw_sensor_rxnum*ic->fw_sensor_txnum*2;

    HYN_ENTER();
    if((mt_len*3 + scap_len) > len || mt_len==0){
        HYN_ERROR("buf too small");
        return FAC_GET_DATA_FAIL; 
    }
    HYN_INFO("---open_higdrv---");
    timeout = 500;
    hyn_wr_reg(hyn_93xxdata,0xD110,2,buf,0); ////test open high
    while(--timeout){ //wait rise edge
        if(hyn_wait_irq_timeout(hyn_93xxdata,100) == 0) break;
        msleep(10);
    }
    if(hyn_wr_reg(hyn_93xxdata,0x3000,2,buf,mt_len)){ //open high
        ret = FAC_GET_DATA_FAIL;
        HYN_ERROR("read open high failed");
        goto selftest_end;
    }
    hyn_wr_reg(hyn_93xxdata,0x000005,3,buf,0); 

    HYN_INFO("---open_low---");
    timeout = 500;
    hyn_wr_reg(hyn_93xxdata,0xD111,2,buf,0); ////test open low
    while(--timeout){ //wait rise edge
        if(hyn_wait_irq_timeout(hyn_93xxdata,100) == 0) break;
        msleep(10);
    }
    if(hyn_wr_reg(hyn_93xxdata,0x1000,2,buf + mt_len,mt_len)){ //open low
        ret = FAC_GET_DATA_FAIL;
        HYN_ERROR("read open low failed");
        goto selftest_end;
    }
    hyn_wr_reg(hyn_93xxdata,0x000005,3,buf,0); 

    //short test
    HYN_INFO("---short---");
    timeout = 500;
    hyn_wr_reg(hyn_93xxdata,0xD112,2,buf,0); //// short test
    while(--timeout){ //wait rise edge
        if(hyn_wait_irq_timeout(hyn_93xxdata,100) == 0) break;
        msleep(10);
    }
    if(hyn_wr_reg(hyn_93xxdata,0x5000,2,buf+(mt_len*2),scap_len)){
        ret = FAC_GET_DATA_FAIL;
        HYN_ERROR("read fac short failed");
        goto selftest_end;
    }
    else{
        raw_s = (u16*)(buf + mt_len*2);
        HYN_INFO("raw_s start data =  %d",*raw_s);
        for(i = 0; i< ic->fw_sensor_rxnum+ic->fw_sensor_txnum; i++){
            HYN_INFO("short raw data = %d %d",i,*raw_s);
            if((*raw_s) != 0)  *raw_s = 2000 / (*raw_s);
            else  *raw_s =0;
            HYN_INFO("short reprocess data = %d %d",i,*raw_s);
            raw_s++;
        }
    }

    //read data finlish start test
    ret = factory_multitest(hyn_93xxdata ,FACTEST_PATH, buf,(s16*)(buf+scap_len+mt_len*2),FACTEST_ITEM);

selftest_end:
    if(0 == fac_test_log_save(FACTEST_LOG_PATH,hyn_93xxdata,(s16*)buf,ret,FACTEST_ITEM)){
        HYN_INFO("fac_test log save success");
    } 
    cst93xx_rst();
    msleep(40);
    return ret;
}
#else
// factory test OPEN threshold
#define HYN_CHECK_FACTORY_RATIO_MIN 50
#define HYN_CHECK_FACTORY_RATIO_MAX 150
#define TRX_NUM 8*9
const uint16_t factory_test_threshold[TRX_NUM] = {
    1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
    1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
    1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
    1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
    1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
    1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
    1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
    1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
    1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
};

// factory test white node,no check
#define WHITE_NODE_NUM 1
const uint8_t white_node[WHITE_NODE_NUM] = {0xFF};
// factory test SHORT threshold
#define SHORT_LOW_TH (500)

#define FACTEST_ITEM      (MULTI_OPEN_TEST|MULTI_SHORT_TEST)
#define FACTEST_LOG_PATH "/sdcard/hyn_fac_test.log"
static int check_white_node(u8 i)
{
    uint8_t idx;
    for (idx = 0; idx < WHITE_NODE_NUM; idx++){
        if ((white_node[idx] == i) && (white_node[idx] < 400)){
            return -1;
        }
    }
    return 0;
}

static int cst93xx_get_test_result(u8 *buf, u16 len)
{
    int ret = 0,timeout,i;
    struct tp_info *ic = NULL;
    u16 scap_len = 0;
    u16 mt_len = 0;
    u16 node_num = 0;
    u16 total_sensor = 0;
    u16 *raw_s;

    cst93xx_rst();
    msleep(40);
    ret = cst93xx_updata_tpinfo();
    if (ret == FALSE){
        HYN_ERROR("get_firmware_info failed");
        return -1;
    }

    ic = &hyn_93xxdata->hw_info;
    scap_len = (ic->fw_sensor_txnum + ic->fw_sensor_rxnum)*2;  
    mt_len = ic->fw_sensor_rxnum*ic->fw_sensor_txnum*2;        
    node_num = ic->fw_sensor_txnum*ic->fw_sensor_rxnum;        
    total_sensor = ic->fw_sensor_rxnum + ic->fw_sensor_txnum;  

    HYN_ENTER();
    if((mt_len*3 + scap_len) > len || mt_len==0){
        HYN_ERROR("buf too small");
        return FAC_GET_DATA_FAIL;
    }
    HYN_INFO("---open_higdrv---");
    timeout = 500;
    hyn_wr_reg(hyn_93xxdata,0xD110,2,buf,0); ////test open high
    while(--timeout){ //wait rise edge
        if(hyn_wait_irq_timeout(hyn_93xxdata,100) == 0) break;
        msleep(10);
    }
    if(hyn_wr_reg(hyn_93xxdata,0x3000,2,buf,mt_len)){ //open high
        ret = FAC_GET_DATA_FAIL;
        HYN_ERROR("read open high failed");
        goto selftest_end;
    }
    hyn_wr_reg(hyn_93xxdata,0x000005,3,buf,0); 

    HYN_INFO("---open_low---");
    timeout = 500;
    hyn_wr_reg(hyn_93xxdata,0xD111,2,buf,0); ////test open low
    while(--timeout){ //wait rise edge
        if(hyn_wait_irq_timeout(hyn_93xxdata,100) == 0) break;
        msleep(10);
    }
    if(hyn_wr_reg(hyn_93xxdata,0x1000,2,buf + mt_len,mt_len)){ //open low
        ret = FAC_GET_DATA_FAIL;
        HYN_ERROR("read open low failed");
        goto selftest_end;
    }
    hyn_wr_reg(hyn_93xxdata,0x000005,3,buf,0); 

    //short test
    HYN_INFO("---short---");
    timeout = 500;
    hyn_wr_reg(hyn_93xxdata,0xD112,2,buf,0); //// short test
    while(--timeout){ //wait rise edge
        if(hyn_wait_irq_timeout(hyn_93xxdata,100) == 0) break;
        msleep(10);
    }
    if(hyn_wr_reg(hyn_93xxdata,0x5000,2,buf+mt_len*2,scap_len)){
        ret = FAC_GET_DATA_FAIL;
        HYN_ERROR("read fac short failed");
        goto selftest_end;
    }

    // check open result
    u16 *open_buf = (u16*)buf;
    for (i = 0; i < node_num; i++){
        u16 factory_test_threshold_min;
        u16 factory_test_threshold_max;

        HYN_INFO("open_buf[%d] %d", i, open_buf[i]);

        if (check_white_node(i)){
            HYN_INFO("check_white_node %d continue", i);
            continue;
        }
        factory_test_threshold_min = factory_test_threshold[i] * HYN_CHECK_FACTORY_RATIO_MIN / 100;
        factory_test_threshold_max = factory_test_threshold[i] * HYN_CHECK_FACTORY_RATIO_MAX / 100;
        if (open_buf[i] < factory_test_threshold_min){
            HYN_ERROR("check open_higdrv MIN ERROR:%d-%d-min-%d", i, open_buf[i], factory_test_threshold_max);
            ret = -1;
        }
        if (open_buf[i] > factory_test_threshold_max){
            HYN_ERROR("check open_higdrv MAX ERROR:%d-%d-max-%d", i, open_buf[i], factory_test_threshold_max);
            ret = -1;
        }
    }
    // check short result
    raw_s = (u16*)(buf + mt_len*2); 
    for (i = 0; i < total_sensor; i++){   
        if (raw_s[i] == 0){
            HYN_ERROR("check short ERROR NULL:%d-%d", i, raw_s[i]);
            ret = -1;
        }
        //raw_s[i] = 2000 / raw_s[i];
        HYN_INFO("raw_s[%d] %d", i, raw_s[i]);
        if (raw_s[i] < SHORT_LOW_TH){
            HYN_ERROR("check short ERROR:%d-%d", i, raw_s[i]);
            ret = -1;
        }
    }
selftest_end:
    if(0 == fac_test_log_save(FACTEST_LOG_PATH,hyn_93xxdata,(s16*)buf,ret,FACTEST_ITEM)){
        HYN_INFO("fac_test log save success");
    } 
    cst93xx_rst();
    msleep(40);
    return ret;
}
#endif

const struct hyn_ts_fuc cst93xx_fuc = {
    .tp_rest = cst93xx_rst,
    .tp_report = cst93xx_report,
    .tp_supend = cst93xx_supend,
    .tp_resum = cst93xx_resum,
    .tp_chip_init = cst93xx_init,
    .tp_updata_fw = cst93xx_updata_fw,
    .tp_set_workmode = cst93xx_set_workmode,
    .tp_check_esd = cst93xx_check_esd,
    .tp_prox_handle = cst93xx_prox_handle,
    .tp_get_dbg_data = cst93xx_get_dbg_data,             
    .tp_get_test_result = cst93xx_get_test_result         
};



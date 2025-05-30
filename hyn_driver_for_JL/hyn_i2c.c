#include "hyn_core.h"


int hyn_write_data(struct hyn_ts_data *ts_data, u8 *buf, u8 reg_len, u16 len)
{
    int ret = 0;
    ret = i2c_master_send(ts_data->salve_addr, buf, len);
    return ret < 0 ? -1:0;
}

int hyn_read_data(struct hyn_ts_data *ts_data,u8 *buf, u16 len)
{
    int ret = 0;
    ret = i2c_master_recv(ts_data->salve_addr, buf, len);
    return ret < 0 ? -1:0;
}

int hyn_wr_reg(struct hyn_ts_data *ts_data, u32 reg_addr, u8 reg_len, u8 *rbuf, u16 rlen)
{
    int ret = 0,i=0;
    u8 wbuf[4];
    reg_len = reg_len&0x0F;
    memset(wbuf,0,sizeof(wbuf));
    i = reg_len;
    while(i){
        i--;
        wbuf[i] = reg_addr;
        reg_addr >>= 8;
    }
    ret = i2c_master_send(ts_data->salve_addr, wbuf, reg_len);
    if(rlen){
        ret |= i2c_master_recv(ts_data->salve_addr, rbuf, rlen);
    }
    return ret < 0 ? -1:0;
}


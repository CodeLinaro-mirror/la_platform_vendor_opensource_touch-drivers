
#include "../hyn_core.h"

int hyn_power_source_ctrl(struct hyn_ts_data *ts_data, int enable)
{
    int ret = 0;
    HYN_ENTER();
    if(IS_ERR_OR_NULL(ts_data) || IS_ERR_OR_NULL(ts_data->plat_data.vdd_ana)){
        return ret;
    }
    if(ts_data->power_is_on != enable){
        if(enable){
            if(!IS_ERR_OR_NULL(ts_data->plat_data.vdd_ana)){
                ret |= regulator_enable(ts_data->plat_data.vdd_ana);
            }
            if(!IS_ERR_OR_NULL(ts_data->plat_data.vdd_i2c)){
                ret |= regulator_enable(ts_data->plat_data.vdd_i2c);
            }
        }
        else{
            if(!IS_ERR_OR_NULL(ts_data->plat_data.vdd_ana)){
                ret |= regulator_disable(ts_data->plat_data.vdd_ana);
            }
            if(!IS_ERR_OR_NULL(ts_data->plat_data.vdd_i2c)){
                ret |= regulator_disable(ts_data->plat_data.vdd_i2c);
            }
        }
        ts_data->power_is_on = enable;
    }
    if(ret)
        HYN_ERROR("set vdd %s regulator failed,ret=%d",enable ? "off":"on",ret);
    return ret;
}

void hyn_irq_set(struct hyn_ts_data *ts_data, u8 value)
{
	// HYN_ENTER();
    if(atomic_read(&ts_data->irq_is_disable) != value){
		atomic_set(&ts_data->irq_is_disable,value);
		msleep(1); //wait switch
        if(value ==0){
			disable_irq(ts_data->gpio_irq);
		}
        else{
			enable_irq(ts_data->gpio_irq);
		}
		// HYN_INFO("IRQ %d",value);
    }
}

void hyn_set_i2c_addr(struct hyn_ts_data *ts_data,u8 addr)
{
#ifdef I2C_PORT  
	ts_data->client->addr = addr;
#endif
}

u16 hyn_sum16(int val, u8* buf,u16 len)
{
	u16 sum = val;
	while(len--) sum += *buf++;
	return sum;
}

u32 hyn_sum32(int val, u32* buf,u16 len)
{
	u32 sum = val;
	while(len--) sum += *buf++;
	return sum;
}

void hyn_esdcheck_switch(struct hyn_ts_data *ts_data, u8 enable)
{
#if ESD_CHECK_EN
	if(IS_ERR_OR_NULL(ts_data->hyn_workqueue) || IS_ERR_OR_NULL(&ts_data->esdcheck_work)) return;
	if(enable){
		ts_data->esd_fail_cnt = 0;
		queue_delayed_work(ts_data->hyn_workqueue, &ts_data->esdcheck_work,
						msecs_to_jiffies(1000));
	}
	else{
		cancel_delayed_work_sync(&ts_data->esdcheck_work);
	}
#endif
}


int hyn_dump_fw(struct hyn_ts_data *ts_data,u8 *buf,size_t count)
{
	int ret = 0;
	#define MAX_LEN   (128*1024)
	if(ts_data->fw_dump_state==0){
		return 0;
	}
	if(count){
		if(1==ts_data->fw_dump_state){
			ts_data->fw_dump_addr = vmalloc(MAX_LEN);
			ts_data->fw_dump_len = 0;
			if(IS_ERR_OR_NULL(ts_data->fw_dump_addr)){
				HYN_ERROR("vmalloc memory[%d] failed.\n",MAX_LEN);
				ts_data->fw_dump_state=0;
				ret= -EPERM;
			}
			else{
				ts_data->fw_dump_state = 2;
				HYN_INFO("vmalloc memory[%d] success",MAX_LEN);
			}
		}
		else if(2==ts_data->fw_dump_state){
			// HYN_INFO("total:%d cnt:%d\r\n",ts_data->fw_dump_len,(int)count);
			if((ts_data->fw_dump_len + count) < MAX_LEN){
				memcpy(ts_data->fw_dump_addr+ts_data->fw_dump_len,buf,(int)count);
				ts_data->fw_dump_len += (int)count;
			}
			else{
				HYN_ERROR("fw len full");
			}
		}
	}
	else{
		if(ts_data->fw_dump_state==2){
			const struct hyn_ts_fuc* hyn_fun = ts_data->hyn_fuc_used;
			hyn_fun->tp_updata_fw(ts_data->fw_dump_addr,ts_data->fw_dump_len);
			HYN_INFO("end dump touchFw \r\n");
		}
		if(ts_data->fw_dump_state){
			if(!IS_ERR_OR_NULL(ts_data->fw_dump_addr)){
				vfree(ts_data->fw_dump_addr);
			}
			ts_data->fw_dump_state= 0;
		}
	}
	return -1;
}

int hyn_wait_irq_timeout(struct hyn_ts_data *ts_data,int msec)
{
	atomic_set(&ts_data->hyn_irq_flg,0);
	while(msec--){
		msleep(1);
		if(atomic_read(&ts_data->hyn_irq_flg)==1){
			atomic_set(&ts_data->hyn_irq_flg,0);
			msec = -1;
			break;
		}
	}
	return msec == -1 ? 0:-1;
}

int get_word(u8**sc_str, u8* ds_str, u8 max_len)
{
	u8 ch,cnt = 0,nul_flg = 0;
	while(1){
		ch = **sc_str;
		*sc_str += 1;
		if((ch==' '&& nul_flg) || ch=='\t' || ch=='\0' || (ch=='\r' && **sc_str == '\n') || ch=='\n'|| ch==','|| ch=='=' || cnt>=max_len){
			*ds_str++ = '\0';
			break;
		}
		if(ch >= 'A' && ch <= 'Z') ch = ch + 'a' - 'A';
		if(ch!=' '){
			*ds_str++ = ch;
			nul_flg = 1;
		}
		cnt++;
	}
	return cnt;
}

int hyn_str_2_num(char *str,int*result,int type)
{
	int step = 0,flg = 0,cnt = 15;
	char ch;
	*result = 0;
	while(*str != '\0' && --cnt){
		ch = *str++;
		if(ch==' ') continue;
		else if(ch=='-' && step==0){
			flg = 1;
			continue;
		}
		if(type == 10){
			if(ch <= '9' && ch >= '0'){
				step++;
				*result *= 10;
				*result += (ch - '0');
			}
			else{
				cnt = 0;
				break;
			}
		}
		else{
			if(ch <= '9' && ch >= '0'){
				step++;
				*result *= 16;
				*result += (ch - '0');
			}
			else if(ch <= 'f' && ch >= 'a'){
				step++;
				*result *= 16;
				*result += (10 + ch - 'a');
			}
			else{
				cnt = 0;
				break;
			}
		}
	}
	if(flg){
		*result = -*result;
	}
	return (cnt == 0 || step==0) ? -1:0;
}

int exchange_byte(uint8_t *src, uint16_t len) 
{
	u16 i = 0;
    if (src == NULL || len == 0) {
        return -1;  // 2?��y?TD��
    }
    for (i = 0; i < len; i+=2) {
        u8 *p = src + i;  
		if(p[0] != 0 || p[1] != 0){
			u8 temp = p[0];
			p[0] = p[1];
			p[1] = temp;  
		}         
    }
    return 0;
}

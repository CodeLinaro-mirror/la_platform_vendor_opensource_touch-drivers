#include ".\..\hynitron_core.h"

#if HYN_ENABLE_PLUG_STATE
extern void hynitron_plug_cb(uint8_t status);
extern bool plugin;
#endif
#if HYN_ENABLE_WEAR_STATE
extern void hynitron_wear_cb(uint8_t status);
extern bool wear_state_by_hyn;
#endif

// factory test OPEN threshold
#define HYN_CHECK_FACTORY_RATIO_MIN 50
#define HYN_CHECK_FACTORY_RATIO_MAX 150
const uint16_t factory_test_threshold[TRX_NUM] = {
    943,1216,1232,1242,1210,1165,951,
    1204,1256,1254,1267,1234,1253,1205,
    1225,1263,1253,1265,1233,1214,1375,
    1231,1395,1253,1403,1233,1214,1232,
	1201,1280,1253,1285,1263,1291,1317,
    1199,1253,1262,1277,1262,1241,1294,
    1186,1254,1263,1272,1250,1232,1261,
    853,1174,1205,1230,1211,1166,977,
};
// factory test white node,no check
#define WHITE_NODE_NUM 0
#if WHITE_NODE_NUM
const uint8_t white_node[WHITE_NODE_NUM] = {0, 1, 6, 7, 8, 15, 48, 55, 56, 57, 62, 63};
#endif 
// factory test SHORT threshold
#define SHORT_LOW_TH (500000)

static uint32_t get_u32_from_ptr(const void *ptr)
{
    uint32_t temp = 0;
    uint8_t *data = (uint8_t *)ptr;

    HYN_FUNC_ENTER;
    temp |= *data;
    data++;
    temp |= (uint32_t)(*data) << 8;
    data++;
    temp |= (uint32_t)(*data) << 16;
    data++;
    temp |= (uint32_t)(*data) << 24;
    data++;
    HYN_FUNC_EXIT;
    return temp;
}

static uint32_t firmware_verify_128bytes_bin_data(uint8_t *pdata, uint16_t order)
{
    uint32_t sum = 0;
    uint16_t data_len = 0;
    uint16_t i;

    HYN_FUNC_ENTER;
    if (!pdata)
    {
        HYNITRON_ERROR("pdata error return");
        return 0;
    }
    data_len = 128;
    if (order == 254)
        data_len = (128 - 20);
    for (i = 0; i < data_len; i += 4)
    {
        sum += get_u32_from_ptr(pdata + i);
    }
    HYN_FUNC_EXIT;
    return sum;
}
int16_t enter_boot(void)
{
    int16_t ret = 0;
    uint8_t i2c_buf[4] = {0};
    uint8_t check_cnt = 0;

    HYN_FUNC_ENTER;
    for (uint8_t i = HYN_ENTER_BOOT_STARTTIME;; i++)
    {
        if (i > HYN_ENTER_BOOT_TIMEOUT)
        {
            HYNITRON_ERROR("enter_boot:try timeout");
            return -1;
        }
        p_g_chip_obj->reset_ic();
        DELAY_MS(i);
        for (check_cnt = 0; check_cnt < 5; check_cnt++)
        {
            i2c_buf[0] = 0xA0;
            i2c_buf[1] = 0x01;
            i2c_buf[2] = 0xAA;
            ret = p_g_chip_obj->i2c_write(HYN_BOOT_I2C_ADDR, i2c_buf, 3);
            if (ret)
            {
                DELAY_MS(2);
                continue;
            }
            DELAY_MS(2);
            i2c_buf[0] = 0xA0;
            i2c_buf[1] = 0x02;
            ret = p_g_chip_obj->i2c_write_read(HYN_BOOT_I2C_ADDR, i2c_buf, 2, i2c_buf, 2);
            if (ret)
            {
                DELAY_MS(2);
                continue;
            }
            if ((i2c_buf[0] == 0x55) && (i2c_buf[1] == 0xB0))
            {
                break;
            }
        }
        if ((i2c_buf[0] == 0x55) && (i2c_buf[1] == 0xB0))
        {
            break;
        }
    }

    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x01;
    i2c_buf[2] = 0x00;
    ret = p_g_chip_obj->i2c_write(HYN_BOOT_I2C_ADDR, i2c_buf, 3);
    if (ret)
    {
        HYNITRON_ERROR("enter_boot exit error");
        return -1;
    }
    HYN_FUNC_EXIT;
    return 0;
}

static int16_t read_word_from_mem(uint8_t type, uint16_t addr, uint32_t *value)
{
    int16_t ret = 0;
    uint8_t i2c_buf[4] = {0};

    HYN_FUNC_ENTER;
    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x10;
    i2c_buf[2] = type;
    ret = p_g_chip_obj->i2c_write(HYN_BOOT_I2C_ADDR, i2c_buf, 3);
    if (ret)
    {
        return -1;
    }
    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x0C;
    i2c_buf[2] = addr;
    i2c_buf[3] = addr >> 8;
    ret = p_g_chip_obj->i2c_write(HYN_BOOT_I2C_ADDR, i2c_buf, 4);
    if (ret)
    {
        return -1;
    }
    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x04;
    i2c_buf[2] = 0xE4;
    ret = p_g_chip_obj->i2c_write(HYN_BOOT_I2C_ADDR, i2c_buf, 3);
    if (ret)
    {
        return -1;
    }
    for (uint8_t t = 0;; t++)
    {
        if (t >= 100)
        {
            return -1;
        }
        i2c_buf[0] = 0xA0;
        i2c_buf[1] = 0x04;
        ret = p_g_chip_obj->i2c_write_read(HYN_BOOT_I2C_ADDR, i2c_buf, 2, i2c_buf, 1);
        if (ret)
        {
            DELAY_MS(2);
            continue;
        }

        if (i2c_buf[0] == 0x00)
        {
            break;
        }
    }
    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x18;
    ret = p_g_chip_obj->i2c_write_read(HYN_BOOT_I2C_ADDR, i2c_buf, 2, i2c_buf, 4);
    if (ret)
    {
        return -1;
    }

    *value = ((uint32_t)(i2c_buf[0])) |
             (((uint32_t)(i2c_buf[1])) << 8) |
             (((uint32_t)(i2c_buf[2])) << 16) |
             (((uint32_t)(i2c_buf[3])) << 24);

    HYN_FUNC_EXIT;
    return 0;
}

static int16_t erase_all_mem(void)
{
    int16_t ret = 0;
    uint8_t i2c_buf[4] = {0};

    HYN_FUNC_ENTER;
    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x14;
    i2c_buf[2] = 0x00;
    i2c_buf[3] = 0x00;
    ret = p_g_chip_obj->i2c_write(HYN_BOOT_I2C_ADDR, i2c_buf, 4);
    if (ret)
    {
        return -1;
    }
    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x0C;
    i2c_buf[2] = 0x80;
    i2c_buf[3] = 0x7F;
    ret = p_g_chip_obj->i2c_write(HYN_BOOT_I2C_ADDR, i2c_buf, 4);
    if (ret)
    {
        return -1;
    }
    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x04;
    i2c_buf[2] = 0xEC;
    ret = p_g_chip_obj->i2c_write(HYN_BOOT_I2C_ADDR, i2c_buf, 3);
    if (ret)
    {
        return -1;
    }
    DELAY_MS(800);
    for (uint16_t t = 0;; t += 10)
    {
        if (t >= 1000)
        {
            return -1;
        }
        DELAY_MS(10);
        i2c_buf[0] = 0xA0;
        i2c_buf[1] = 0x05;
        ret = p_g_chip_obj->i2c_write_read(HYN_BOOT_I2C_ADDR, i2c_buf, 2, i2c_buf, 1);
        if (ret)
        {
            DELAY_MS(10);
            continue;
        }
        if (i2c_buf[0] == 0x88)
        {
            break;
        }
    }
    HYN_FUNC_EXIT;
    return 0;
}

static int16_t write_sram(uint8_t *buf, uint16_t len)
{
#if (HYNITRON_I2C_TRAN_PER_SIZE < HYNITRON_PROGRAM_PAGE_SIZE + 2)
    uint8_t i2c_buf[HYNITRON_I2C_TRAN_PER_SIZE] = {0};
#else
    uint8_t i2c_buf[HYNITRON_PROGRAM_PAGE_SIZE + 2] = {0};
#endif

    int16_t ret = 0;
    uint16_t reg = 0xA018;
    uint16_t per_len = sizeof(i2c_buf) - 2;

    HYN_FUNC_ENTER;
    while ((len > 0) && (len <= MEM_SIZE))
    {
        uint16_t cur_len = len;
        if (cur_len > per_len)
        {
            cur_len = per_len;
        }
        i2c_buf[0] = reg >> 8;
        i2c_buf[1] = reg;
        memcpy(i2c_buf + 2, buf, cur_len);
        ret = p_g_chip_obj->i2c_write(HYN_BOOT_I2C_ADDR, i2c_buf, cur_len + 2);
        if (ret)
        {
            DELAY_MS(2);
            continue;
        }
        reg += cur_len;
        buf += cur_len;
        len -= cur_len;
    }
    HYN_FUNC_EXIT;
    return 0;
}

static int16_t write_mem_page(uint16_t addr, uint8_t *buf, uint16_t len)
{
    int16_t ret = 0;
    uint8_t i2c_buf[4] = {0};

    HYN_FUNC_ENTER;
    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x0C;
    i2c_buf[2] = len;
    i2c_buf[3] = len >> 8;
    ret = p_g_chip_obj->i2c_write(HYN_BOOT_I2C_ADDR, i2c_buf, 4);
    if (ret)
    {
        return -1;
    }
    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x14;
    i2c_buf[2] = addr;
    i2c_buf[3] = addr >> 8;
    ret = p_g_chip_obj->i2c_write(HYN_BOOT_I2C_ADDR, i2c_buf, 4);
    if (ret)
    {
        return -1;
    }
    ret = write_sram(buf, len);
    if (ret)
    {
        return -1;
    }
    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x04;
    i2c_buf[2] = 0xEE;
    ret = p_g_chip_obj->i2c_write(HYN_BOOT_I2C_ADDR, i2c_buf, 3);
    if (ret)
    {
        return -1;
    }
    for (uint16_t t = 0;; t += 10)
    {
        if (t >= 1000)
        {
            return -1;
        }
        DELAY_MS(HYNITRON_PROGRAM_PAGE_TIMEOUT);
        i2c_buf[0] = 0xA0;
        i2c_buf[1] = 0x05;
        ret = p_g_chip_obj->i2c_write_read(HYN_BOOT_I2C_ADDR, i2c_buf, 2, i2c_buf, 1);
        if (ret)
        {
            DELAY_MS(10);
            continue;
        }
        if (i2c_buf[0] == 0x55)
        {
            break;
        }
    }
    HYN_FUNC_EXIT;
    return 0;
}

static int16_t write_mem_all(void)
{
    uint8_t *data;
    uint16_t addr = 0;
    uint16_t remain_len = MEM_SIZE;

    HYN_FUNC_ENTER;
    while ((remain_len > 0) && (remain_len <= MEM_SIZE))
    {
        uint16_t cur_len = remain_len;
        if (cur_len > HYNITRON_PROGRAM_PAGE_SIZE)
        {
            cur_len = HYNITRON_PROGRAM_PAGE_SIZE;
        }
        // if write fw 128 bytes every time,need update point
        if (p_g_chip_obj->get_bin_addr(((MEM_SIZE - remain_len) / HYNITRON_PROGRAM_PAGE_SIZE), HYNITRON_PROGRAM_PAGE_SIZE) < 0)
        {
            HYNITRON_ERROR("get_bin_addr fail");
            return -1;
        }
        data = p_g_chip_obj->bin_data.data;
        if (write_mem_page(addr, data, cur_len) < 0)
        {
            return -1;
        }
        data += cur_len; // update bin point every 128 bytes
        addr += cur_len; // update bin point every 128 bytes
        remain_len -= cur_len;
    }
    HYN_FUNC_EXIT;
    return 0;
}

static int16_t calculate_verify_checksum(void)
{
    int16_t ret = 0;
    uint8_t i2c_buf[4] = {0};
    uint32_t checksum = 0;

    HYN_FUNC_ENTER;
    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x03;
    i2c_buf[2] = 0x00;
    ret = p_g_chip_obj->i2c_write(HYN_BOOT_I2C_ADDR, i2c_buf, 3);
    if (ret)
    {
        return -1;
    }
    for (uint16_t t = 0;; t += 10)
    {
        if (t >= 1000)
        {
            return -1;
        }
        DELAY_MS(10);
        i2c_buf[0] = 0xA0;
        i2c_buf[1] = 0x00;
        ret = p_g_chip_obj->i2c_write_read(HYN_BOOT_I2C_ADDR, i2c_buf, 2, i2c_buf, 1);
        if (ret)
        {
            DELAY_MS(2);
            continue;
        }
        if (i2c_buf[0] == 0x01)
        {
            break;
        }
        if (i2c_buf[0] == 0x02)
        {
            return -1;
        }
    }

    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x08;
    ret = p_g_chip_obj->i2c_write_read(HYN_BOOT_I2C_ADDR, i2c_buf, 2, i2c_buf, 4);
    if (ret)
    {
        return -1;
    }
    checksum = ((uint32_t)(i2c_buf[0])) |
               (((uint32_t)(i2c_buf[1])) << 8) |
               (((uint32_t)(i2c_buf[2])) << 16) |
               (((uint32_t)(i2c_buf[3])) << 24);

    if (checksum != p_g_chip_obj->bin_data.checksum)
    {
        return -1;
    }
    HYN_FUNC_EXIT;
    return 0;
}

int16_t upgrade_firmware(void)
{
    int16_t ret = 0;
    uint8_t retry = 3;

    HYN_FUNC_ENTER;
    while (retry--)
    {
        ret = enter_boot();
        if (ret)
        {
            HYNITRON_ERROR("enter_boot fail.%d", retry);
            continue;
        }
        p_g_chip_obj->chip_ic_workmode = ENUM_MODE_UPDATE_FW;
        ret = erase_all_mem();
        if (ret)
        {
            HYNITRON_ERROR("erase_all_mem fail.%d", retry);
            continue;
        }
        ret = write_mem_all();
        if (ret)
        {
            HYNITRON_ERROR("write_mem_all fail.%d", retry);
            continue;
        }
        ret = calculate_verify_checksum();
        if (ret)
        {
            HYNITRON_ERROR("calculate_verify_checksum fail.%d", retry);
            continue;
        }
        else
        {
            break;
        }
    }

    p_g_chip_obj->chip_ic_workmode = ENUM_MODE_NORMAL;
    p_g_chip_obj->reset_ic();
    DELAY_MS(HYN_RESET_DELAY_TIME);
    if (ret)
    {
        HYNITRON_ERROR("upgrade_firmware fail exit.%d", retry);
        return -1;
    }
    else
    {
        HYNITRON_DEBUG("upgrade_firmware success exit");
    }
    HYN_FUNC_EXIT;
    return 0;
}

int16_t bin_firmware_parse(void)
{
    uint16_t i;
    // int16_t ret;
    uint32_t sum;
    uint8_t *pdata;

    HYN_FUNC_ENTER;

    p_g_chip_obj->bin_data.checksum = 0;
    // 0x7F6C-32620=32k-128-20=checksum
    // 0x7F80-32640=32k-128
    sum = 0x55;
    for (i = 0; i < 255; i++)
    {
        if (p_g_chip_obj->get_bin_addr(i, 128) < 0)
        {
            HYNITRON_ERROR("get_bin_addr fail.");
            return -1;
        }
        pdata = p_g_chip_obj->bin_data.data;
        sum += firmware_verify_128bytes_bin_data(pdata, i);
        // HYNITRON_DEBUG("sum checksum data=0x%04x 0x%04x 0x%04x", sum, i,*pdata);
        pdata += 128; // update bin point every 128 bytes
        if (i == 254)
        {
            pdata -= 20; // 0x7F6C
            if (sum != get_u32_from_ptr(pdata))
            {
                HYNITRON_ERROR("main checksum data error 0x%04x 0x%04x", sum, get_u32_from_ptr(pdata));
                return -1;
            }
            sum = 0;
            sum += get_u32_from_ptr(pdata - 4);      // 0x7F68
            sum += get_u32_from_ptr(pdata - 8);      // 0x7F64
            sum += get_u32_from_ptr(pdata - 12);     // 0x7F60
            sum += get_u32_from_ptr(pdata - 16);     // 0x7F5C
            if (sum != get_u32_from_ptr(pdata + 16)) // 0x7F7C
            {
                HYNITRON_ERROR("info checksum data error 0x%04x 0x%04x", sum, get_u32_from_ptr(pdata + 16));
                return -1;
            }
            p_g_chip_obj->bin_data.ok = TRUE;
            p_g_chip_obj->bin_data.checksum = get_u32_from_ptr(pdata + 0x7F6C - 0x7F6C);
            p_g_chip_obj->bin_data.chip_type = (get_u32_from_ptr(pdata + 0x7F64 - 0x7F6C) >> 16);
            p_g_chip_obj->bin_data.version = get_u32_from_ptr(pdata + 0x7F68 - 0x7F6C);
            p_g_chip_obj->bin_data.project_id = (get_u32_from_ptr(pdata + 0x7F64 - 0x7F6C) & 0x0000FFFF);
        }
    }
    HYNITRON_DEBUG("bin_data.ok: 0x%x", p_g_chip_obj->bin_data.ok);
    HYNITRON_DEBUG("bin_data.checksum: 0x%04x", p_g_chip_obj->bin_data.checksum);
    HYNITRON_DEBUG("bin_data.version: 0x%04x", p_g_chip_obj->bin_data.version);
    HYNITRON_DEBUG("bin_data.project_id: 0x%04x", p_g_chip_obj->bin_data.project_id);
    HYNITRON_DEBUG("bin_data.chip_type: 0x%04x", p_g_chip_obj->bin_data.chip_type);
    HYN_FUNC_EXIT;
    return 0;
}

int16_t upgrade_firmware_judge(void)
{
    HYN_FUNC_ENTER;
    if (!p_g_chip_obj->bin_data.ok)
    {
        HYNITRON_ERROR("p_g_chip_obj->bin_data.ok %d is not ok.", p_g_chip_obj->bin_data.ok);
        return -1;
    }
    if (p_g_chip_obj->partno_chip_type != p_g_chip_obj->bin_data.chip_type)
    {
        HYNITRON_ERROR("partno chip type != bin data chip type");
        return -1;
    }
    if (p_g_chip_obj->IC_firmware.firmware_info_ok == 0)
    {
        HYNITRON_DEBUG("IC_firmware.firmware_info_ok error,need force update.");
        return 0;
    }
    else
    {
        if (p_g_chip_obj->IC_firmware.firmware_project_id != p_g_chip_obj->bin_data.project_id)
        {
            HYNITRON_NOTICE("firmware_project_id != bin_data.firmware_project_id,no need update");
            return -1;
        }

        if (p_g_chip_obj->bin_data.version == 0xFFFFFFFF) {
            HYNITRON_DEBUG("bin_data.version is highest test version,need force update");
            return 0;
        }

        if (p_g_chip_obj->IC_firmware.firmware_checksum == p_g_chip_obj->bin_data.checksum)
        {
            HYNITRON_NOTICE("firmware_checksum == bin_data.checksum,no need update.");
            return -1;
        }
        else
        {

            if (p_g_chip_obj->IC_firmware.firmware_version == 0) {
                HYNITRON_DEBUG("firmware_version is lowest test version,no need update");
                return -1;
            } 
                       
            if (p_g_chip_obj->IC_firmware.firmware_version <= p_g_chip_obj->bin_data.version)
            {
                HYNITRON_DEBUG("firmware_version is lower than bin_data.version,need update");
                return 0;
            }
            else
            {
                HYNITRON_NOTICE("firmware_version is higher,no need update");
            }
        }
    }
    HYN_FUNC_EXIT;
    return -1;
}

int16_t read_point(void)
{
    int16_t ret = 0;
    uint8_t data_buf[HYNITRON_FINGER_NUM * 5 + 5] = {0};
    uint8_t finger_num = 0;
    uint8_t reg_buf[4] = {0};
    uint8_t len = sizeof(data_buf);
    uint8_t *data = data_buf;
    uint16_t reg = 0xD000;

    HYN_FUNC_ENTER;
    memset(&(p_g_chip_obj->touch_info), 0, sizeof(p_g_chip_obj->touch_info));
    memset(p_g_chip_obj->point_info, 0, sizeof(p_g_chip_obj->point_info));

    p_g_chip_obj->esd_lock = 0;
    while ((len > 0) && (len <= sizeof(data_buf)))
    {
        uint8_t cur_len = len;
        reg_buf[0] = reg >> 8;
        reg_buf[1] = reg;
        ret = p_g_chip_obj->i2c_write(HYNITRON_I2C_ADDR, reg_buf, 2);
        ret |= p_g_chip_obj->i2c_write_read(HYNITRON_I2C_ADDR, reg_buf, 2, data, cur_len);
        if (ret)
        {
            //HYNITRON_ALERT("write 0xD000 failed");
            return -1;
        }
        len -= cur_len;
        data += cur_len;
        reg += cur_len;
    }
    // w 0x00 05 00 ----newer sdk
    // w 0xd0 00 ab ----newer sdk
    reg_buf[0] = 0xD0;
    reg_buf[1] = 0x00; // 00
    reg_buf[2] = 0xAB;
    ret = p_g_chip_obj->i2c_write(HYNITRON_I2C_ADDR, reg_buf, 3);
    if (ret)
    {
        //HYNITRON_ALERT("write 0xD024AB failed\n");
        return -1;
    }
    p_g_chip_obj->esd_lock = 0xff;

    if (data_buf[6] != 0xAB)
    {
        //HYNITRON_ALERT("read data_buf failed 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", data_buf[0], data_buf[1], data_buf[2], data_buf[3], data_buf[4], data_buf[5], data_buf[6]);
        return -1;
    }

    // palm + gesture
    if (data_buf[4] & 0xF0)
    {
        //HYNITRON_DEBUG("read point data_buf[4]=0x%x.\n", data_buf[4]);
        if (data_buf[4] & 0xF0)
        {
            if ((data_buf[4] >> 7) == 0X01)
            {
                p_g_chip_obj->touch_info.valid = TRUE;
                p_g_chip_obj->touch_info.palm = 0x01;
            }
            else if (data_buf[4] >> 4)
            {
                p_g_chip_obj->touch_info.valid = TRUE;
                p_g_chip_obj->touch_info.gesture = data_buf[4] & 0xF0;
            }
        }
    }

    finger_num = data_buf[5] & 0x7F;
    if (finger_num > HYNITRON_FINGER_NUM)
    {
        //HYNITRON_ERROR("read finger_num error %d", finger_num);
        return -1;
    }

    if (finger_num > 0)
        p_g_chip_obj->status.reproting_flag = 1;
    else
        p_g_chip_obj->status.reproting_flag = 0;

    p_g_chip_obj->touch_info.valid = TRUE;
    p_g_chip_obj->touch_info.finger_num = finger_num;
    // button
    if ((data_buf[5] & 0x80) == 0x80)
    {
        uint8_t *data_key = data_buf + finger_num * 5;
        if (finger_num > 0)
        {
            data_key += 2;
        }
        p_g_chip_obj->touch_info.valid = TRUE;
        p_g_chip_obj->touch_info.key_id = data_key[0];
        p_g_chip_obj->touch_info.key_status = data_key[1];
    }

    if (finger_num > 0)
    {
        uint8_t *data1 = data_buf;
        uint8_t id = data1[0] >> 4;
        uint8_t switch_ = data1[0] & 0x0F;
        uint16_t x = ((uint16_t)(data1[1]) << 4) | (data1[3] >> 4);
        uint16_t y = ((uint16_t)(data1[2]) << 4) | (data1[3] & 0x0F);

#if (HYN_REPORT_TOUCH_CHECKSUM_EN)
        uint8_t checksum;
        checksum = 0;
        checksum = *(data1 + 0);
        checksum += *(data1 + 1);
        checksum += *(data1 + 2);
        checksum += *(data1 + 3);
        if ((checksum & 0x0f) != (*(data1 + 4) & 0x0f))
        {
            //HYNITRON_ERROR("get point checksum error");
            return -1;
        }
#endif
        // HYNITRON_DEBUG("get raw point x %d,y %d,id %d,switch_ %d.", x, y, id, switch_);
        if (id < HYNITRON_FINGER_NUM)
        {
            p_g_chip_obj->point_info[id].evt = switch_;
            p_g_chip_obj->point_info[id].x = x;
            p_g_chip_obj->point_info[id].y = y;
            p_g_chip_obj->point_info[id].valid = TRUE;
        }
    }

#if (HYNITRON_FINGER_NUM == 2)
    for (uint8_t i = 1; i < finger_num; i++)
    {
        uint8_t *data2 = data_buf + 5 * i + 2;
        uint8_t id = data2[0] >> 4;
        uint8_t switch_ = data2[0] & 0x0F;
        uint16_t x = ((uint16_t)(data2[1]) << 4) | (data2[3] >> 4);
        uint16_t y = ((uint16_t)(data2[2]) << 4) | (data2[3] & 0x0F);
#if (HYN_REPORT_TOUCH_CHECKSUM_EN)
        uint8_t checksum;
        checksum = 0;
        checksum = *(data2 + 0);
        checksum += *(data2 + 1);
        checksum += *(data2 + 2);
        checksum += *(data2 + 3);
        if ((checksum & 0x0f) != (*(data2 + 4) & 0x0f))
        {
            //HYNITRON_ERROR("get point checksum error");
            return -1;
        }
#endif
        if (id < HYNITRON_FINGER_NUM)
        {
            p_g_chip_obj->point_info[id].evt = switch_;
            p_g_chip_obj->point_info[id].x = x;
            p_g_chip_obj->point_info[id].y = y;
            p_g_chip_obj->point_info[id].valid = TRUE;
        }
    }
#endif
    HYN_FUNC_EXIT;
    return 0;
}

int16_t enter_sleep(uint8_t sleep_type)
{
    int16_t ret = -1;
    uint8_t i2c_buf[4] = {0};
    uint8_t read_buf[4] = {0};
    uint8_t retry = 0;

    HYN_FUNC_ENTER;
    if (p_g_chip_obj->chip_ic_workmode)
    {
        HYNITRON_ERROR("enter_sleep chip_ic_workmode %d return", p_g_chip_obj->chip_ic_workmode);
        return -1;
    }
    p_g_chip_obj->status.esd_enable = FALSE;
    p_g_chip_obj->status.sleep_done = sleep_type;
    p_g_chip_obj->set_work_mode(0XFF);
    switch (sleep_type)
    {
    case DEEP_SLEEP:
        i2c_buf[0] = 0xD1;
        i2c_buf[1] = 0x05;
        HYNITRON_DEBUG("enter DEEP_SLEEP");
        break;
    case IDLE_SLEEP:
        i2c_buf[0] = 0xD1;
        i2c_buf[1] = 0x07;
        HYNITRON_DEBUG("enter IDLE_SLEEP");
        break;
    case GESTURE_SLEEP:
        i2c_buf[0] = 0xD1;
        i2c_buf[1] = 0x04;
        HYNITRON_DEBUG("enter GESTURE_SLEEP");
        break;
    default:
        HYNITRON_DEBUG("sleep_type NA return");
        p_g_chip_obj->status.esd_enable = TRUE;
        return 0;
    }
    for (retry = 4; retry > 0; retry--)
    {
        ret = p_g_chip_obj->i2c_write(HYNITRON_I2C_ADDR, i2c_buf, 2);
        if (ret)
        {
            HYNITRON_ERROR("enter_sleep write sleep cmd error");
            DELAY_MS(1);
            continue;
        }
        read_buf[0] = 0x00;
        read_buf[1] = 0x02;
        ret = p_g_chip_obj->i2c_write_read(HYNITRON_I2C_ADDR, read_buf, 2, read_buf, 2);
        if (ret)
        {
            HYNITRON_ERROR("enter_sleep read 0x0002 error");
            DELAY_MS(1);
            continue;
        }
        if (read_buf[1] != i2c_buf[1])
        {
            HYNITRON_ERROR("enter_sleep check sleep cmd error");
            DELAY_MS(1);
            continue;
        }
        else
        {
            HYNITRON_ERROR("enter_sleep check sleep cmd success");
            break;
        }
    }
    if (sleep_type != DEEP_SLEEP)
        p_g_chip_obj->status.esd_enable = TRUE;
    HYN_FUNC_EXIT;
    return 0;
}
int16_t wake_up(void)
{
    HYN_FUNC_ENTER;
    HYNITRON_DEBUG("wake_up chip_ic_workmode %d", p_g_chip_obj->chip_ic_workmode);
    p_g_chip_obj->status.esd_enable = FALSE;
    p_g_chip_obj->reset_ic();
    DELAY_MS(HYN_RESET_DELAY_TIME);
#if 0
if (p_g_chip_obj->get_firmware_info())
{
HYNITRON_ERROR("wake_up get_firmware_info error");
p_g_chip_obj->reset_ic();
}
#endif
    p_g_chip_obj->status.sleep_done = 0;
    p_g_chip_obj->status.esd_enable = TRUE;
    p_g_chip_obj->esd_value = 0;
    p_g_chip_obj->esd_value_pre = 0;
    HYN_FUNC_EXIT;
    return 0;
}

int16_t read_chip_id(void)
{
    int16_t ret = 0;
    uint8_t retry = 10;
    // uint8_t i2c_buf[4] = {0};
    HYN_FUNC_ENTER;

    for (; retry > 0; retry--)
    {
        // partno
        ret = read_word_from_mem(1, 0x077C, &p_g_chip_obj->partno_chip_type);
        if (ret)
        {
            DELAY_MS(1);
            continue;
        }
        // module id
        ret = read_word_from_mem(0, 0x7FC0, &p_g_chip_obj->module_id);
        if (ret)
        {
            DELAY_MS(1);
            continue;
        }
        if ((p_g_chip_obj->partno_chip_type >> 16) == 0xCACA)
        {
            p_g_chip_obj->partno_chip_type &= 0xffff;
            break;
        }
        DELAY_MS(2);
    }
    HYNITRON_DEBUG("partno_chip_type: 0x%04x", p_g_chip_obj->partno_chip_type);
    HYNITRON_DEBUG("module_id: 0x%04x", p_g_chip_obj->module_id);
    if ((p_g_chip_obj->partno_chip_type != 0x9217) && (p_g_chip_obj->partno_chip_type != 0x9220))
    {
        HYNITRON_ERROR("partno_chip_type error 0x%04x", p_g_chip_obj->partno_chip_type);
        return -1;
    }
    HYN_FUNC_EXIT;
    return 0;
}

int16_t read_info(void)
{
    int16_t ret = 0;
    uint8_t i2c_buf[4] = {0};
    uint8_t retry = 3;

    HYN_FUNC_ENTER;
    while (retry--)
    {
        ret = enter_boot();
        if (ret)
        {
            DELAY_MS(1);
            continue;
        }

        // partno
        ret = read_word_from_mem(1, 0x077C, &p_g_chip_obj->partno_chip_type);
        if (ret)
        {
            DELAY_MS(1);
            continue;
        }
        // module id
        ret = read_word_from_mem(0, 0x7FC0, &p_g_chip_obj->module_id);
        if (ret)
        {
            DELAY_MS(1);
            continue;
        }

        // version
        ret = read_word_from_mem(0, 0x7F68, &p_g_chip_obj->IC_firmware.firmware_version);
        if (ret)
        {
            DELAY_MS(1);
            continue;
        }

        // out boot mode
        i2c_buf[0] = 0xA0;
        i2c_buf[1] = 0x04;
        i2c_buf[2] = 0xE8;
        ret = p_g_chip_obj->i2c_write(HYN_BOOT_I2C_ADDR, i2c_buf, 3);
        if (ret)
        {
            DELAY_MS(1);
            continue;
        }

        if ((p_g_chip_obj->partno_chip_type >> 16) == 0xCACA)
        {
            p_g_chip_obj->partno_chip_type &= 0xffff;
            break;
        }
    }
    p_g_chip_obj->reset_ic();
    DELAY_MS(HYN_RESET_DELAY_TIME);
#if HYN_ENABLE_PLUG_STATE
    if (plugin)
    {
        hynitron_plug_cb(1);
    }
#endif

#if HYN_ENABLE_WEAR_STATE
    if (wear_state_by_hyn)
    {
        hynitron_wear_cb(1);
    }
#endif
    if ((p_g_chip_obj->status.sleep_done == GESTURE_SLEEP))
    {
        ret = p_g_chip_obj->enter_sleep(GESTURE_SLEEP);
        if (ret)
        {
            HYNITRON_ERROR("enter_sleep failed");
        }
    }
    p_g_chip_obj->chip_ic_workmode = ENUM_MODE_NORMAL;

    if ((p_g_chip_obj->partno_chip_type != 0x9217) && (p_g_chip_obj->partno_chip_type != 0x9220))
    {
        HYNITRON_ERROR("partno_chip_type error 0x%04x", p_g_chip_obj->partno_chip_type);
        return -1;
    }
    HYN_FUNC_EXIT;
    return 0;
}

int16_t judge_module(void) {
    HYN_FUNC_ENTER;
    uint8_t i2c_buf[6] = {0};
    int16_t ret = -1;
    int8_t retry;
    p_g_chip_obj->reset_ic();
    DELAY_MS(40);
    p_g_chip_obj->set_work_mode(ENUM_MODE_DEBUG_INFO);
    //module id
    for (retry = 0; retry < 3; retry++) {
        i2c_buf[0] = 0xD2;
        i2c_buf[1] = 0x20;
        ret = p_g_chip_obj->i2c_write_read(HYNITRON_I2C_ADDR, i2c_buf, 2, i2c_buf, 4);
        if (ret) {
            HYNITRON_ERROR("read 0xD220 error");
            DELAY_US(200);
            continue;
        }
        p_g_chip_obj->module_id = ((uint32_t)i2c_buf[3] << 24) + ((uint32_t)i2c_buf[2] << 16) + ((uint32_t)i2c_buf[1] << 8) + i2c_buf[0];

        //partno id
        i2c_buf[0] = 0xD2;
        i2c_buf[1] = 0x24;
        ret = p_g_chip_obj->i2c_write_read(HYNITRON_I2C_ADDR, i2c_buf, 2, i2c_buf, 4);
        if (ret) {
            HYNITRON_ERROR("read 0xD221 error");
            DELAY_US(200);
            continue;
        }
        p_g_chip_obj->partno_chip_type = ((uint32_t)i2c_buf[3] << 24) + ((uint32_t)i2c_buf[2] << 16) + ((uint32_t)i2c_buf[1] << 8) + i2c_buf[0];
        if ((p_g_chip_obj->partno_chip_type >> 16) == 0xCACA) {
            p_g_chip_obj->partno_chip_type &= 0xffff;
            break;
        }
        
        break;
    }

    if (retry == 3) {
        HYNITRON_ERROR("judge_module error");
        return -1;
    }

    HYNITRON_DEBUG("partno_chip_type: 0x%04x", p_g_chip_obj->partno_chip_type);
    HYNITRON_DEBUG("module_id: 0x%04x", p_g_chip_obj->module_id);
    if ((p_g_chip_obj->partno_chip_type != 0x9217) && (p_g_chip_obj->partno_chip_type != 0x9220)) {
        HYNITRON_ERROR("partno_chip_type error 0x%04x", p_g_chip_obj->partno_chip_type);
        return -1;
    }
    p_g_chip_obj->set_work_mode(ENUM_MODE_NORMAL);
    return 0;
}

int16_t get_firmware_info(void)
{
    uint8_t i2c_buf[6] = {0};
    uint32_t version = 0;
    int16_t ret = -1;
    int32_t info_checksum = 0;

    HYN_FUNC_ENTER;
    p_g_chip_obj->IC_firmware.firmware_info_ok = 0;
    p_g_chip_obj->set_work_mode(ENUM_MODE_DEBUG_INFO);
    DELAY_MS(1);
    // BOOT TIME + CBCB
    i2c_buf[0] = 0xD1;
    i2c_buf[1] = 0xFC;
    ret = p_g_chip_obj->i2c_write_read(HYNITRON_I2C_ADDR, i2c_buf, 2, i2c_buf, 4);
    if (ret)
    {
        HYNITRON_ERROR("read 0xD1FC error");
        return -1;
    }
    info_checksum += get_u32_from_ptr(i2c_buf);
    // d200: 0x55AA55AA
    // info_checksum += 0x55AA55AA;
    // d200: 0x55AA55AA
    i2c_buf[0] = 0xD2;
    i2c_buf[1] = 0x00;
    ret = p_g_chip_obj->i2c_write_read(HYNITRON_I2C_ADDR, i2c_buf, 2, i2c_buf, 4);
    if (ret)
    {
        HYNITRON_ERROR("read 0xD200 error");
        return -1;
    }
    info_checksum += get_u32_from_ptr(i2c_buf);

    // firmware_project_id firmware_ic_type
    i2c_buf[0] = 0xD2;
    i2c_buf[1] = 0x04;
    ret = p_g_chip_obj->i2c_write_read(HYNITRON_I2C_ADDR, i2c_buf, 2, i2c_buf, 4);
    if (ret)
    {
        HYNITRON_ERROR("read 0xD204 error");
        return -1;
    }
    info_checksum += get_u32_from_ptr(i2c_buf);
    p_g_chip_obj->IC_firmware.firmware_project_id = ((uint16_t)i2c_buf[1] << 8) + i2c_buf[0];
    p_g_chip_obj->IC_firmware.firmware_ic_type = ((uint16_t)i2c_buf[3] << 8) + i2c_buf[2];
    // firmware_version
    i2c_buf[0] = 0xD2;
    i2c_buf[1] = 0x08;
    ret = p_g_chip_obj->i2c_write_read(HYNITRON_I2C_ADDR, i2c_buf, 2, i2c_buf, 4);
    if (ret)
    {
        HYNITRON_ERROR("read 0xD208 error");
        return -1;
    }
    version = get_u32_from_ptr(i2c_buf);
    info_checksum += version;
    p_g_chip_obj->IC_firmware.firmware_version = version;
    // firmware_info_checksum
    i2c_buf[0] = 0xD2;
    i2c_buf[1] = 0x1c;
    ret = p_g_chip_obj->i2c_write_read(HYNITRON_I2C_ADDR, i2c_buf, 2, i2c_buf, 4);
    if (ret)
    {
        HYNITRON_ERROR("read 0xD21c error");
        return -1;
    }
    // HYNITRON_DEBUG("sum info_checksum 0x%x ,read info_checksum 0x%x ",
    // info_checksum, get_u32_from_ptr(i2c_buf))
    if (get_u32_from_ptr(i2c_buf) != info_checksum)
    {
        HYNITRON_ERROR("info_checksum error");
        return -1;
    }
    p_g_chip_obj->IC_firmware.firmware_info_ok = 1;
    // firmware_checksum
    i2c_buf[0] = 0xD2;
    i2c_buf[1] = 0x0C;
    ret = p_g_chip_obj->i2c_write_read(HYNITRON_I2C_ADDR, i2c_buf, 2, i2c_buf, 4);
    if (ret)
    {
        HYNITRON_ERROR("read 0xD20c error");
        return -1;
    }
    p_g_chip_obj->IC_firmware.firmware_checksum = ((uint32_t)i2c_buf[3] << 24) + ((uint32_t)i2c_buf[2] << 16) + ((uint32_t)i2c_buf[1] << 8) + i2c_buf[0];
    // tx_num rx_num key_num
    i2c_buf[0] = 0xD1;
    i2c_buf[1] = 0xF4;
    ret = p_g_chip_obj->i2c_write_read(HYNITRON_I2C_ADDR, i2c_buf, 2, i2c_buf, 4);
    if (ret)
    {
        HYNITRON_ERROR("read 0xD1F4 error");
        return -1;
    }
    p_g_chip_obj->IC_firmware.tx_num = ((uint16_t)i2c_buf[1] << 8) + i2c_buf[0];
    p_g_chip_obj->IC_firmware.rx_num = i2c_buf[2];
    p_g_chip_obj->IC_firmware.key_num = i2c_buf[3];
    // go back normal mode
    p_g_chip_obj->set_work_mode(ENUM_MODE_NORMAL);
    HYNITRON_DEBUG("chip firmware_info_ok: 0x%04x", p_g_chip_obj->IC_firmware.firmware_info_ok);
    HYNITRON_DEBUG("chip firmware_ic_type: 0x%04x", p_g_chip_obj->IC_firmware.firmware_ic_type);
    HYNITRON_DEBUG("chip firmware_version: 0x%04x", p_g_chip_obj->IC_firmware.firmware_version);
    HYNITRON_DEBUG("chip firmware_project_id: 0x%04x", p_g_chip_obj->IC_firmware.firmware_project_id);
    HYNITRON_DEBUG("chip checksum: 0x%04x", p_g_chip_obj->IC_firmware.firmware_checksum);
    HYNITRON_DEBUG("chip tx_num: %d", p_g_chip_obj->IC_firmware.tx_num);
    HYNITRON_DEBUG("chip rx_num: %d", p_g_chip_obj->IC_firmware.rx_num);
    HYNITRON_DEBUG("chip key_num: %d", p_g_chip_obj->IC_firmware.key_num);
    HYN_FUNC_EXIT;
    return 0;
}

int16_t set_work_mode(uint8_t mode)
{
    uint8_t i2c_buf[4] = {0};
    uint8_t temp_buf[4] = {0};
    uint8_t i = 0;
    int16_t ret = -1;
    // uint8_t mode_cmd = 0;

    HYN_FUNC_ENTER;
    for (i = 0; i < 3; i++)
    {
        i2c_buf[0] = 0xD1;
        i2c_buf[1] = 0x1e;
        ret = p_g_chip_obj->i2c_write(HYNITRON_I2C_ADDR, i2c_buf, 2);
        if (ret)
        {
            DELAY_US(200);
            continue;
        }
        DELAY_US(200);
        i2c_buf[0] = 0xD1;
        i2c_buf[1] = 0x1E;
        ret = p_g_chip_obj->i2c_write(HYNITRON_I2C_ADDR, i2c_buf, 2);
        if (ret)
        {
            DELAY_US(200);
            continue;
        }
        i2c_buf[0] = 0x00;
        i2c_buf[1] = 0x02;
        ret = p_g_chip_obj->i2c_write_read(HYNITRON_I2C_ADDR, i2c_buf, 2, i2c_buf, 2);
        if (ret)
        {
            DELAY_US(200);
            continue;
        }
        if (i2c_buf[1] == 0x1E)
            break;
    }

    switch (mode)
    {
    case ENUM_MODE_NORMAL:
    {
        HYNITRON_DEBUG("set_work_mode: ENUM_MODE_NORMAL");
        p_g_chip_obj->chip_ic_workmode = mode;
        i2c_buf[0] = 0xD1;
        i2c_buf[1] = 0x09;
        break;
    }
    case ENUM_MODE_DEBUG_DIFF:
    {
        HYNITRON_DEBUG("set_work_mode: ENUM_MODE_DEBUG_DIFF");
        p_g_chip_obj->chip_ic_workmode = mode;
        i2c_buf[0] = 0xD1;
        i2c_buf[1] = 0x0D;
        break;
    }
    case ENUM_MODE_DEBUG_RAWDATA:
    {
        HYNITRON_DEBUG("set_work_mode: ENUM_MODE_DEBUG_RAWDATA");
        p_g_chip_obj->chip_ic_workmode = mode;
        i2c_buf[0] = 0xD1;
        i2c_buf[1] = 0x0A;
        break;
    }
    case ENUM_MODE_DEBUG_INFO:
    {
        HYNITRON_DEBUG("set_work_mode: ENUM_MODE_DEBUG_INFO");
        p_g_chip_obj->chip_ic_workmode = mode;
        i2c_buf[0] = 0xD1;
        i2c_buf[1] = 0x01;
        break;
    }
    case ENUM_MODE_FACTORY:
    {
        HYNITRON_DEBUG("set_work_mode: ENUM_MODE_FACTORY");
        p_g_chip_obj->chip_ic_workmode = mode;
        for (i = 0; i < 10; i++)
        {
            i2c_buf[0] = 0xD1;
            i2c_buf[1] = 0x14;
            ret = p_g_chip_obj->i2c_write(HYNITRON_I2C_ADDR, i2c_buf, 2);
            if (ret)
            {
                DELAY_MS(1);
                continue;
            }
            DELAY_MS(10);
            i2c_buf[0] = 0x00;
            i2c_buf[1] = 0x09;
            ret = p_g_chip_obj->i2c_write_read(HYNITRON_I2C_ADDR, i2c_buf, 2, i2c_buf, 1);
            if (ret)
            {
                DELAY_MS(1);
                continue;
            }
            if (i2c_buf[0] == 0x14)
                break;
        }
        i2c_buf[0] = 0xD1;
        i2c_buf[1] = 0x19;
        ret = p_g_chip_obj->i2c_write(HYNITRON_I2C_ADDR, i2c_buf, 2);
        if (ret)
        {
            HYNITRON_ERROR("set_work_mode 0xD119 error");
            return -1;
        }
        break;
    }
    case ENUM_MODE_FACTORY_LOWDRV:
    {
        HYNITRON_DEBUG("set_work_mode: ENUM_MODE_FACTORY_LOWDRV");
        p_g_chip_obj->chip_ic_workmode = mode;
        i2c_buf[0] = 0xD1;
        i2c_buf[1] = 0x11;
        break;
    }
    case ENUM_MODE_FACTORY_HIGHDRV:
    {
        HYNITRON_DEBUG("set_work_mode: ENUM_MODE_FACTORY_HIGHDRV");
        p_g_chip_obj->chip_ic_workmode = mode;
        i2c_buf[0] = 0xD1;
        i2c_buf[1] = 0x10;
        break;
    }
    case ENUM_MODE_FACTORY_SHORT:
    {
        HYNITRON_DEBUG("set_work_mode: ENUM_MODE_FACTORY_SHORT");
        p_g_chip_obj->chip_ic_workmode = mode;
        i2c_buf[0] = 0xD1;
        i2c_buf[1] = 0x12;
        break;
    }
    case ENUM_MODE_PLUG_IN:
    {
        HYNITRON_DEBUG("set_work_mode: ENUM_MODE_PLUG_IN");
        i2c_buf[0] = 0xD1;
        i2c_buf[1] = 0x1F;
        break;
    }
    case ENUM_MODE_PLUG_OUT:
    {
        HYNITRON_DEBUG("set_work_mode: ENUM_MODE_PLUG_OUT");
        i2c_buf[0] = 0xD1;
        i2c_buf[1] = 0x20;
        break;
    }
    case ENUM_MODE_WEAR:
    {
        HYNITRON_DEBUG("set_work_mode: ENUM_MODE_WEAR");
        i2c_buf[0] = 0xD1;
        i2c_buf[1] = 0x23;
        break;
    }
    case ENUM_MODE_UNWEAR:
    {
        HYNITRON_DEBUG("set_work_mode: ENUM_MODE_UNWEAR");
        i2c_buf[0] = 0xD1;
        i2c_buf[1] = 0x24;
        break;
    }
    default:
    {
        HYNITRON_DEBUG("set_work_mode: NA return");
        return 0;
    }
    }
    for (i = 0; i < 3; i++)
    {
        ret = p_g_chip_obj->i2c_write(HYNITRON_I2C_ADDR, i2c_buf, 2);
        if (ret)
        {
            HYNITRON_ERROR("set_work_mode 0x%x 0x%x error", i2c_buf[0], i2c_buf[1]);
            DELAY_MS(1);
            continue;
        }
        temp_buf[0] = 0x00;
        temp_buf[1] = 0x02;
        ret = p_g_chip_obj->i2c_write_read(HYNITRON_I2C_ADDR, temp_buf, 2, temp_buf, 2);
        if (ret)
        {
            HYNITRON_ERROR("set_work_mode read 0x0002 FAIL");
            DELAY_MS(1);
            continue;
        }
        if (i2c_buf[1] != temp_buf[1])
        {
            HYNITRON_ERROR("set_work_mode read 0x0002=0x%x FAIL", temp_buf[1]);
            DELAY_MS(1);
            continue;
        }
        break;
    }
    DELAY_MS(10);
    HYN_FUNC_EXIT;
    return 0;
}

int16_t get_irq_signal(void)
{
    HYN_FUNC_ENTER;
    if (p_g_chip_obj->status.int_trig)
    {
        p_g_chip_obj->status.int_trig = 0;
        HYNITRON_DEBUG("get_irq_signal get irq event");
        return 0;
    }
    else
    {
        HYNITRON_DEBUG("get_irq_signal no irq event");
        return -1;
    }
    HYN_FUNC_EXIT;
}

#define LOG_SIZE 5 * 1024 * 1024

int32_t hyn_get_file_size(FILE *stream)
{
    int32_t file_size = 0;
    int32_t cur_offset = ftell(stream);
    if (cur_offset == -1)
    {
        HYNITRON_ERROR("ftell failed");
        return -1;
    }
    if (fseek(stream, 0, SEEK_END) != 0)
    {
        HYNITRON_ERROR("fseek failed");
        return -1;
    }
    file_size = ftell(stream);
    if (file_size == -1)
    {
        HYNITRON_ERROR("ftell failed");
    }
    if (fseek(stream, cur_offset, SEEK_SET) != 0)
    {
        HYNITRON_ERROR("fseek failed");
        return -1;
    }
    return file_size;
}

// int write_rawdata_log(int16_t *debugdata, int16_t *tsdebugdata, int16_t *rsdebugdata)
// {
//     // write debug data to file
//     // HYNITRON_DEBUG("-----Start record hybrid data!!!\r\n");
//     const char *path = "/log/offlinelog/cst92xx_data.txt";
//     FILE *file = fopen(path, "a");
//     int32_t file_length;
//     int ret;

//     if (file == NULL)
//     {
//         //HYNITRON_ERROR(" failed to open %s\n", path);
//         return -1;
//     }
//     file_length = hyn_get_file_size(file);
//     if (file_length > LOG_SIZE)
//     {
//         //HYNITRON_ERROR("get file size oversize = %u\n", file_length);
//         ret = unlink(path);
//         if (ret != 0)
//         {
//             //HYNITRON_ERROR(" failed to remove %s\n", path);
//         }
//     }
//     else if (file_length == 0)
//     {
//         //HYNITRON_ERROR("start save data,get file size = %u\n", file_length);
//     }

//     fprintf(file, "diff_data=\n");
//     for (int i = 0; i < p_g_chip_obj->IC_firmware.tx_num; i++)
//     {
//         fprintf(file, "[HYN]%d,%d,%d,%d,%d,%d,%d,%d,%d\n", debugdata[i * 8], debugdata[i * 8 + 1], debugdata[i * 8 + 2], debugdata[i * 8 + 3], debugdata[i * 8 + 4], debugdata[i * 8 + 5], debugdata[i * 8 + 6], debugdata[i * 8 + 7], tsdebugdata[i]);
//     }
//     fprintf(file, "[HYN]%d,%d,%d,%d,%d,%d,%d,%d\n", rsdebugdata[0], rsdebugdata[1], rsdebugdata[2], rsdebugdata[3], rsdebugdata[4], rsdebugdata[5], rsdebugdata[6], rsdebugdata[7]);

//     fclose(file);
//     // HYNITRON_DEBUG("-----Finished record hybrid data!!!\r\n");
//     p_g_chip_obj->status.int_trig = 0;
//     return 0;
// }

int16_t get_debug_data(void)
{
    int16_t ret = -1;
    uint8_t i2c_buf[4];
    uint16_t node_num = TRX_NUM;
    union
    {
        uint8_t buff_u8[TRX_NUM * 2];
        int16_t buff_s16[TRX_NUM];
    } debugdata = {.buff_s16 = 0};

    union
    {
        uint8_t buff_u8[TX_NUM * 2];
        int16_t buff_s16[TX_NUM];
    } tsdebugdata = {.buff_s16 = 0};

    union
    {
        uint8_t buff_u8[RX_NUM * 2];
        int16_t buff_s16[RX_NUM];
    } rsdebugdata = {.buff_s16 = 0};

    HYN_FUNC_ENTER;
    i2c_buf[0] = 0x10;
    i2c_buf[1] = 0x00;
    ret = p_g_chip_obj->i2c_write_read(HYNITRON_I2C_ADDR, i2c_buf, 2, debugdata.buff_u8, node_num * 2);
    if (ret)
    {
        //HYNITRON_ERROR("read 0x1000 error");
        return -1;
    }
    i2c_buf[0] = 0x70;
    i2c_buf[1] = 0x00;
    ret = p_g_chip_obj->i2c_write_read(HYNITRON_I2C_ADDR, i2c_buf, 2, rsdebugdata.buff_u8, p_g_chip_obj->IC_firmware.rx_num * 2);
    if (ret)
    {
        //HYNITRON_ERROR("read 0x7000 error");
        return -1;
    }
    i2c_buf[0] = 0x72;
    i2c_buf[1] = 0x00;
    ret = p_g_chip_obj->i2c_write_read(HYNITRON_I2C_ADDR, i2c_buf, 2, tsdebugdata.buff_u8, p_g_chip_obj->IC_firmware.tx_num * 2);
    if (ret)
    {
        //HYNITRON_ERROR("read 0x7200 error");
        return -1;
    }
    i2c_buf[0] = 0x00;
    i2c_buf[1] = 0x05;
    i2c_buf[2] = 0x00;
    ret = p_g_chip_obj->i2c_write(HYNITRON_I2C_ADDR, i2c_buf, 3);
    if (ret)
    {
        //HYNITRON_ERROR("write 0x000500 error");
        return -1;
    }
#if 0
    write_rawdata_log(debugdata.buff_s16, tsdebugdata.buff_s16, rsdebugdata.buff_s16);
#else
    // HYNITRON_ERROR("timestamp=, diff_data=");
    // for(uint8_t i = 0; i < p_g_chip_obj->IC_firmware.tx_num;i++)
    // {
    //     for(uint8_t j = 0; j < p_g_chip_obj->IC_firmware.rx_num; j++)
    //     {
    //         rt_kprintf("%d ",debugdata.buff_s16[(i*7)+j]);
    //     }
    //     rt_kprintf("%d\n",tsdebugdata.buff_s16[i]);
    // }
    // rt_kprintf("%d,%d,%d,%d,%d,%d,%d\n",rsdebugdata.buff_s16[0],rsdebugdata.buff_s16[1],rsdebugdata.buff_s16[2],rsdebugdata.buff_s16[3],rsdebugdata.buff_s16[4],rsdebugdata.buff_s16[5],rsdebugdata.buff_s16[6]);
#endif

    HYN_FUNC_EXIT;
    return 0;
}

int16_t get_diff_data(void)
{
    int16_t ret = -1;
    // uint16_t node_num = 0;
    uint16_t time_out;
    HYN_FUNC_ENTER;
    if ((p_g_chip_obj->status.sleep_done > 0) || (p_g_chip_obj->status.ic_init_done == 0))
    {
        HYNITRON_DEBUG("get_diff_data status NA return");
        return -1;
    }
    ret = p_g_chip_obj->get_firmware_info();
    if (ret)
    {
        HYNITRON_ERROR("get_firmware_info failed");
        return -1;
    }
    p_g_chip_obj->set_work_mode(ENUM_MODE_DEBUG_DIFF);
    DELAY_MS(20);
    // if (p_g_chip_obj->IC_firmware.key_num == 0)
    // {
    // node_num = p_g_chip_obj->IC_firmware.tx_num * p_g_chip_obj->IC_firmware.rx_num;
    // }
    // else
    // {
    // node_num = (p_g_chip_obj->IC_firmware.tx_num - 1) * p_g_chip_obj->IC_firmware.rx_num + p_g_chip_obj->IC_firmware.key_num;
    // }
    time_out = 40;
    while (time_out--)
    {
        if (!get_irq_signal())
            break;
        HAL_Delay_us(100 * 1000);
    }
    get_debug_data();
    p_g_chip_obj->set_work_mode(ENUM_MODE_NORMAL);
    HYN_FUNC_EXIT;
    return 0;
}

int16_t get_rawdata_data(void)
{
    int16_t ret = -1;
    // uint16_t node_num = 0;
    uint16_t time_out;
    HYN_FUNC_ENTER;
    if ((p_g_chip_obj->status.sleep_done > 0) || (p_g_chip_obj->status.ic_init_done == 0))
    {
        HYNITRON_DEBUG("get_diff_data status NA return");
        return -1;
    }
    ret = p_g_chip_obj->get_firmware_info();
    if (ret)
    {
        HYNITRON_ERROR("get_firmware_info failed");
        return -1;
    }
    p_g_chip_obj->set_work_mode(ENUM_MODE_DEBUG_RAWDATA);
    DELAY_MS(20);
    // if (p_g_chip_obj->IC_firmware.key_num == 0)
    // {
    // node_num = p_g_chip_obj->IC_firmware.tx_num * p_g_chip_obj->IC_firmware.rx_num;
    // }
    // else
    // {
    // node_num = (p_g_chip_obj->IC_firmware.tx_num - 1) * p_g_chip_obj->IC_firmware.rx_num + p_g_chip_obj->IC_firmware.key_num;
    // }
    time_out = 40;
    while (time_out--)
    {
        if (!get_irq_signal())
            break;
        HAL_Delay_us(100 * 1000);
    }
    get_debug_data();
    p_g_chip_obj->set_work_mode(ENUM_MODE_NORMAL);
    HYN_FUNC_EXIT;
    return 0;
}

int get_factory_test_data(uint8_t mode, uint8_t *buff, uint16_t data_len)
{
    uint8_t i2c_buf[4];
    int16_t ret = -1;

    HYN_FUNC_ENTER;

    switch (mode)
    {
    case ENUM_MODE_FACTORY_LOWDRV:
        i2c_buf[0] = 0x10;
        i2c_buf[1] = 0x00;
        // p_g_chip_obj->status.esd_enable = FALSE;
        HYNITRON_DEBUG("enter ENUM_MODE_FACTORY_LOWDRV");
        break;
    case ENUM_MODE_FACTORY_HIGHDRV:
        i2c_buf[0] = 0x30;
        i2c_buf[1] = 0x00;
        HYNITRON_DEBUG("enter ENUM_MODE_FACTORY_HIGHDRV");
        break;
    case ENUM_MODE_FACTORY_SHORT:
        i2c_buf[0] = 0x50;
        i2c_buf[1] = 0x00;
        HYNITRON_DEBUG("enter ENUM_MODE_FACTORY_SHORT");
        break;
    default:
        HYNITRON_DEBUG("factory mode NA return");
        return 0;
    }
    ret = p_g_chip_obj->i2c_write_read(HYNITRON_I2C_ADDR, i2c_buf, 2, buff, data_len);
    if (ret)
    {
        HYNITRON_ERROR("read 0x1000 error");
        return -1;
    }
    i2c_buf[0] = 0x00;
    i2c_buf[1] = 0x05;
    i2c_buf[2] = 0x00;
    ret = p_g_chip_obj->i2c_write(HYNITRON_I2C_ADDR, i2c_buf, 3);
    if (ret)
    {
        HYNITRON_ERROR("write 0x000500 error");
        return -1;
    }
    HYN_FUNC_EXIT;
    return 0;
}

int16_t check_white_node(uint8_t i)
{
#if WHITE_NODE_NUM
    uint8_t idx;

    HYN_FUNC_ENTER;
    for (idx = 0; idx < WHITE_NODE_NUM; idx++)
    {
        if ((white_node[idx] == i) && (white_node[idx] < 100))
        {
            return -1;
        }
    }
    HYN_FUNC_EXIT;
#endif
    return 0;
}

int16_t get_factory_test_result(uint16_t *cval_buf)
{
    uint16_t node_num = 0;
    uint16_t time_out = 1000;
    uint16_t i = 0;
    // uint16_t j = 0;
    uint8_t total_sensor;
    int16_t ret;

    union
    {
        uint8_t buff_u8[TRX_NUM * 2];
        int16_t buff_u16[TRX_NUM];
    } open_higdrv = {.buff_u16 = 0};

    union
    {
        uint8_t buff_u8[TRX_NUM * 2];
        int16_t buff_u16[TRX_NUM];
    } open_lowdrv = {.buff_u16 = 0};

    union
    {
        uint8_t buff_u8[(TX_NUM + RX_NUM) * 2];
        int16_t buff_u16[(TX_NUM + RX_NUM)];
    } sns_short = {.buff_u16 = 0};

    HYN_FUNC_ENTER;
    if ((p_g_chip_obj->status.sleep_done > 0) || (p_g_chip_obj->status.ic_init_done == 0))
    {
        HYNITRON_DEBUG("get_factory_test_result status NA return");
        return -1;
    }
    p_g_chip_obj->reset_ic();
    DELAY_MS(HYN_RESET_DELAY_TIME);
    ret = p_g_chip_obj->get_firmware_info();
    if (ret)
    {
        HYNITRON_ERROR("get_firmware_info failed");
        return -1;
    }
    total_sensor = (p_g_chip_obj->IC_firmware.tx_num + p_g_chip_obj->IC_firmware.rx_num + p_g_chip_obj->IC_firmware.key_num);
    if (p_g_chip_obj->IC_firmware.key_num == 0)
    {
        node_num = p_g_chip_obj->IC_firmware.tx_num * p_g_chip_obj->IC_firmware.rx_num;
    }
    else
    {
        node_num = (p_g_chip_obj->IC_firmware.tx_num - 1) * p_g_chip_obj->IC_firmware.rx_num + p_g_chip_obj->IC_firmware.key_num;
    }
    if ((p_g_chip_obj->IC_firmware.tx_num > 10) || (p_g_chip_obj->IC_firmware.rx_num > 10))
    {
        HYNITRON_ERROR("IC_firmware.tx_num/rx_num ERROR");
        return -1;
    }
    HYNITRON_INFO("TX_NUM = %d; RX_NUM = %d",p_g_chip_obj->IC_firmware.tx_num,p_g_chip_obj->IC_firmware.rx_num);
    // get open hight result
    set_work_mode(ENUM_MODE_FACTORY_HIGHDRV);
    time_out = 40;
    while (time_out--)
    {
        if (!get_irq_signal())
            break;
        HAL_Delay_us(100 * 1000);
    }
    get_factory_test_data(ENUM_MODE_FACTORY_HIGHDRV, open_higdrv.buff_u8, node_num * 2);
    // HYNITRON_DEBUG("---open_higdrv---");
    // for (i = 0, j = 0; i < node_num; i++)
    // {
    // HYNITRON_DEBUG("%d ", open_higdrv.buff_u16[i]);
    // j++;
    // if (j >= p_g_chip_obj->IC_firmware.rx_num)
    // {
    // j -= p_g_chip_obj->IC_firmware.rx_num;
    // }
    // }
    // get open low result
    set_work_mode(ENUM_MODE_FACTORY_LOWDRV);
    time_out = 40;
    while (time_out--)
    {
        if (!get_irq_signal())
            break;
        HAL_Delay_us(100 * 1000);
    }
    get_factory_test_data(ENUM_MODE_FACTORY_LOWDRV, open_lowdrv.buff_u8, node_num * 2);
    // HYNITRON_DEBUG("---open_lowdrv---");
    // for (i = 0, j = 0; i < node_num; i++)
    // {
    // HYNITRON_DEBUG("%d ", open_lowdrv.buff_u16[i]);
    // j++;
    // if (j >= p_g_chip_obj->IC_firmware.rx_num)
    // {
    // j -= p_g_chip_obj->IC_firmware.rx_num;
    // }
    // }

    // get short result
    set_work_mode(ENUM_MODE_FACTORY_SHORT);
    time_out = 40;
    while (time_out--)
    {
        if (!get_irq_signal())
            break;
        HAL_Delay_us(100 * 1000);
    }
    get_factory_test_data(ENUM_MODE_FACTORY_SHORT, sns_short.buff_u8, (total_sensor * 2));
    // HYNITRON_DEBUG("---sns_short---");
    // for (i = 0, j = 0; i < (total_sensor); i++)
    // {
    // HYNITRON_DEBUG("%d ", sns_short.buff_u16[i]);
    // j++;
    // if ((j == p_g_chip_obj->IC_firmware.rx_num) ||
    // (j == (p_g_chip_obj->IC_firmware.rx_num + p_g_chip_obj->IC_firmware.tx_num)) ||
    // (j == total_sensor))
    // {
    // HYNITRON_DEBUG("");
    // }
    // }

    for (i = 0; i < node_num; i++)
    {
        open_higdrv.buff_u16[i] -= open_lowdrv.buff_u16[i];
        open_higdrv.buff_u16[i] = open_higdrv.buff_u16[i] * 3;
    }

    for (i = 0; i < sizeof(open_higdrv.buff_u16) / sizeof(uint16_t); i++)
    {
        cval_buf[i] = open_higdrv.buff_u16[i];
    }

    // check open result
    for (i = 0; i < node_num; i++)
    {
        uint16_t factory_test_threshold_min;
        uint16_t factory_test_threshold_max;
        if (check_white_node(i))
        {
            HYNITRON_DEBUG("check_white_node %d continue", i);
            continue;
        }
        factory_test_threshold_min = factory_test_threshold[i] * HYN_CHECK_FACTORY_RATIO_MIN / 100;
        factory_test_threshold_max = factory_test_threshold[i] * HYN_CHECK_FACTORY_RATIO_MAX / 100;
        if (open_higdrv.buff_u16[i] < factory_test_threshold_min) //
        {
            HYNITRON_ERROR("check open_higdrv MIN ERROR:%d-%d-min-%d", i, open_higdrv.buff_u16[i], factory_test_threshold_max);
            ret = -1; // return -1;
        }
        if (open_higdrv.buff_u16[i] > factory_test_threshold_max) //
        {
            HYNITRON_ERROR("check open_higdrv MAX ERROR:%d-%d-max-%d", i, open_higdrv.buff_u16[i], factory_test_threshold_max);
            ret = -1; // return -1;
        }
    }
    // check short result
    for (i = 0; i < total_sensor; i++)
    {
        double resis = 0;
        if (sns_short.buff_u16[i] == 0)
        {
            HYNITRON_ERROR("check short ERROR NULL:%d-%d", i, sns_short.buff_u16[i]);
            ret = -1; // return -1;
        }
        resis = 1.0 * 2000000 / sns_short.buff_u16[i];
        if (resis < SHORT_LOW_TH)
        {
            HYNITRON_ERROR("check short ERROR:%d-%d", i, sns_short.buff_u16[i]);
            ret = -1; // return -1;
        }
    }
    if (ret)
    {
        HYNITRON_ERROR("get_factory_test_result FAIL,ret=%d", ret);
    }
    else
    {
        HYNITRON_DEBUG("get_factory_test_result PASS,ret=%d", ret);
    }

    p_g_chip_obj->reset_ic();
    DELAY_MS(40);
    HYN_FUNC_EXIT;
    return ret;
}

void esd_check(void)
{
    int16_t ret = -1;
    uint8_t i2c_buf[6], retry;
    uint8_t flag = 0;
    uint8_t sleep_status_last;

    HYN_FUNC_ENTER;
    if (p_g_chip_obj->chip_ic_workmode)
    {
        HYNITRON_ERROR("esd_check chip_ic_workmode %d return ", p_g_chip_obj->chip_ic_workmode);
        return;
    }
    if ((p_g_chip_obj->status.esd_enable == 0) || (p_g_chip_obj->status.ic_init_done == 0))
    {
        HYNITRON_DEBUG("esd_check status NA return");
        return;
    }
    retry = 4;
    flag = 0;
    while (retry--)
    {
        p_g_chip_obj->esd_lock = 1;
        if (p_g_chip_obj->status.reproting_flag)
        {
            uint16_t time_out = 16;
            while (time_out--)
            {
                if (p_g_chip_obj->esd_lock == 0xff)
                    break;
                DELAY_MS(1);
            }
        }
        i2c_buf[0] = 0xD0;
        i2c_buf[1] = 0x48;
        ret = p_g_chip_obj->i2c_write(HYNITRON_I2C_ADDR, i2c_buf, 2);
        DELAY_US(500);
        ret = p_g_chip_obj->i2c_write_read(HYNITRON_I2C_ADDR, i2c_buf, 2, i2c_buf, 1);
        if (ret)
        {
            DELAY_MS(1);
            continue;
        }
        else
        {
            if (((i2c_buf[0] & 0xF0) == 0x20) || (((i2c_buf[0] & 0xF0) == 0xA0) && ((i2c_buf[0] & 0x0F) < 10)))
            {
                flag = 1;
                p_g_chip_obj->esd_value = i2c_buf[0];
                break;
            }
            else
            {
                HYNITRON_ERROR("ESD data NA,need retry: 0x%x %d", i2c_buf[0], p_g_chip_obj->esd_lock);
                DELAY_MS(2);
                continue;
            }
        }
    }
    // HYNITRON_DEBUG("ESD data:%d,0x%04x", flag, p_g_chip_obj->esd_value);
    if (flag == 0)
    {
#ifdef CONFIG_I2C_RESET
        extern int hyn_reset_i2c(void);
        hyn_reset_i2c();
#endif
        p_g_chip_obj->poweron_ic(FALSE);
        DELAY_MS(2);
        p_g_chip_obj->poweron_ic(TRUE);
        DELAY_MS(2);
        p_g_chip_obj->reset_ic();
        DELAY_MS(HYN_RESET_DELAY_TIME);
        p_g_chip_obj->esd_value = 0;
        p_g_chip_obj->esd_value_pre = 0;
#if HYN_ENABLE_PLUG_STATE
        if (plugin)
        {
            hynitron_plug_cb(1);
        }
#endif

#if HYN_ENABLE_WEAR_STATE
        if (wear_state_by_hyn)
        {
            hynitron_wear_cb(1);
        }
#endif
        HYNITRON_DEBUG("esd_check power reset ic");
    }
    sleep_status_last = p_g_chip_obj->status.sleep_done;
    if ((sleep_status_last == GESTURE_SLEEP) && (p_g_chip_obj->esd_value != 0xA0))
    {
        ret = p_g_chip_obj->enter_sleep(sleep_status_last);
        if (ret)
        {
            HYNITRON_ERROR("enter_sleep failed");
        }
    }
    p_g_chip_obj->esd_value_pre = p_g_chip_obj->esd_value;
    HYN_FUNC_EXIT;
}

void hyn_cst92xx_init_obj(struct hyn_chip *chip)
{
    chip->chip_type = HYN_CHIP_TYPE;
    chip->enter_boot = enter_boot;
    chip->read_chip_id = read_chip_id;
    chip->read_info = read_info;
    chip->bin_firmware_parse = bin_firmware_parse;
    chip->upgrade_firmware_judge = upgrade_firmware_judge;
    chip->upgrade_firmware = upgrade_firmware;
    chip->read_point = read_point;
    chip->enter_sleep = enter_sleep;
    chip->wake_up = wake_up;
    chip->esd_check = esd_check;
    chip->judge_module      = judge_module;
    chip->get_firmware_info = get_firmware_info;
    chip->set_work_mode = set_work_mode;
    chip->get_diff_data = get_diff_data;
    chip->get_rawdata_data = get_rawdata_data;
    chip->get_debug_data = get_debug_data;
    chip->get_factory_test_result = get_factory_test_result;
}

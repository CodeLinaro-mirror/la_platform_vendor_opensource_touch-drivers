#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define INCLUDE_SELF_TEST   (0)

#define LOG(fmt, ...)   printf(fmt, ##__VA_ARGS__)

#define B8_TO_U32(a, b, c, d)   ((uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24))

static int g_debug_file = -1;
static int g_ini_file = -1;

static struct {
    uint8_t tx_num;
    uint8_t rx_num;
    uint32_t checksum;
    uint32_t version;
} g_tp_inf;

static struct {
    uint8_t *fac_buf;
    int16_t *cp_data;
} g_fac_data;

static struct {
    uint8_t *ini_buf;
    int16_t *cp_min;
    int16_t *cp_max;
} g_ini_data;

#define FAC_TEST_DEV   "/proc/hyn_apk_tool/fops_nod"

int read_tp_inf(void)
{
    LOG("read tp information ... ");

    g_debug_file = open(FAC_TEST_DEV, O_RDWR);
    if (g_debug_file < 0) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    uint8_t file_buf[32] = {0};
    file_buf[0] = 0x0A;
    size_t file_size = write(g_debug_file, file_buf, 1);
    if (file_size != 1) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    file_size = read(g_debug_file, file_buf, sizeof(file_buf));
    if (file_size != sizeof(file_buf)) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    LOG("\r\n");

    g_tp_inf.tx_num = file_buf[0];
    g_tp_inf.rx_num = file_buf[1];
    g_tp_inf.checksum = B8_TO_U32(file_buf[24], file_buf[25], file_buf[26], file_buf[27]);
    g_tp_inf.version = B8_TO_U32(file_buf[20], file_buf[21], file_buf[22], file_buf[23]);
    LOG(">> tx number : %d\r\n", g_tp_inf.tx_num);
    LOG(">> rx number : %d\r\n", g_tp_inf.rx_num);
    LOG(">> firmware checksum : 0x%08X\r\n", g_tp_inf.checksum);
    LOG(">> firmware version  : 0x%08X\r\n", g_tp_inf.version);

    return 0;
}

int read_fac_data(void)
{
    LOG("read factory data ... ");

    int fac_data_size = g_tp_inf.tx_num * g_tp_inf.rx_num*2;

    // alloc memory
    g_fac_data.fac_buf = malloc(fac_data_size+4);
    if (g_fac_data.fac_buf == NULL) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    g_fac_data.cp_data = malloc(fac_data_size * sizeof(*g_fac_data.cp_data));
    if (g_fac_data.cp_data == NULL) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    uint8_t file_buf[32] = {0};
    file_buf[0] = 0x7A;
    file_buf[1] = 0x08;
    size_t file_size = write(g_debug_file, file_buf, 2);
    if (file_size != 2) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    file_size = read(g_debug_file, g_fac_data.fac_buf, fac_data_size+4);
    if (file_size != (fac_data_size+4)) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    LOG(">> cp_data\r\n");
    int16_t *fac_buf = (int16_t*)(g_fac_data.fac_buf + 4);
    for (int i = 0; i < fac_data_size/2; i++) {
        g_fac_data.cp_data[i] = *fac_buf;
        LOG("%5d ", *fac_buf);
        fac_buf++;
    }
    LOG("\r\n");
    return 0;
}

int read_ini_data(void)
{
    LOG("read ini data ... \r\n");

    g_ini_file = open("./factory.txt", O_RDONLY);
    if (g_ini_file < 0) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    // get file size
    struct stat file_stat;
    int code = fstat(g_ini_file, &file_stat);
    if (code < 0) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }
    if (file_stat.st_size > 1*1024*1024) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    // alloc memory
    g_ini_data.ini_buf = malloc(file_stat.st_size + 1);
    if (g_ini_data.ini_buf == NULL) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }
    memset(g_ini_data.ini_buf, 0x00, file_stat.st_size + 1);

    g_ini_data.cp_min = malloc(g_tp_inf.tx_num * g_tp_inf.rx_num * sizeof(*g_ini_data.cp_min));
    if (g_ini_data.cp_min == NULL) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    g_ini_data.cp_max = malloc(g_tp_inf.tx_num * g_tp_inf.rx_num * sizeof(*g_ini_data.cp_max));
    if (g_ini_data.cp_max == NULL) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    // read ini data
    ssize_t file_size = read(g_ini_file, g_ini_data.ini_buf, file_stat.st_size);
    if (file_size != file_stat.st_size) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    //find cp_min
    LOG("cp_min \r\n");
    int16_t *tmp_ptr = g_ini_data.cp_min;
    char key_buf[32] = {0};
    snprintf(key_buf, sizeof(key_buf),"advanced_cp_min = ");
    const char *dest_str = strstr(g_ini_data.ini_buf, key_buf);
    if (dest_str == NULL) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }
    dest_str += strlen(key_buf);
    for (int i = 0; i < g_tp_inf.rx_num*g_tp_inf.tx_num; i++) {
        if (i != 0) {
            dest_str = strstr(dest_str, " ");
            if (dest_str == NULL) {
                LOG("error(%d)\r\n", __LINE__);
                return -1;
            }
            dest_str += strlen(" ");
        }
        int num = 0;
        code = sscanf(dest_str, "%d", &num);
        if (code != 1) {
            LOG("error(%d)\r\n", __LINE__);
            return -1;
        }
        *tmp_ptr = num;
        tmp_ptr++;
        LOG("\t%d",num);
    }
    LOG("\r\ncp_max\r\n");
    //find cp_max
    tmp_ptr = g_ini_data.cp_max;
    memset(key_buf,0,sizeof(key_buf));
    snprintf(key_buf, sizeof(key_buf),"advanced_cp_max = ");
    dest_str = strstr(dest_str, key_buf);
    dest_str += strlen(key_buf);
    for (int i = 0; i < g_tp_inf.rx_num*g_tp_inf.tx_num; i++) {
        if (i != 0) {
            dest_str = strstr(dest_str, " ");
            if (dest_str == NULL) {
                LOG("error(%d)\r\n", __LINE__);
                return -1;
            }
            dest_str += strlen(" ");
        }
        int num = 0;
        code = sscanf(dest_str, "%d", &num);
        if (code != 1) {
            LOG("error(%d)\r\n", __LINE__);
            return -1;
        }
        *tmp_ptr = num;
        tmp_ptr++;
        LOG("\t%d",num);
    }
    LOG("\r\n");

    return 0;
}

void log_fac_result(void)
{
    int error = 0;

    error = 0;
    LOG("log test ...\r\n");
    int16_t index = 0;
    for (int i = 0; i < g_tp_inf.tx_num*g_tp_inf.rx_num; i++) {
        if(g_fac_data.cp_data[i] < g_ini_data.cp_min[i] || g_fac_data.cp_data[i] > g_ini_data.cp_max[i]){
            LOG("node_faild%i:%d<%d>%d\r\n", i,g_ini_data.cp_min[i], g_fac_data.cp_data[i],g_ini_data.cp_max[i]);
            error = -1;
        }
    }
    LOG("%s\r\n", error ? "NG" : "PASS");
}

int main(int argc, char *argv[])
{
    int error = 0;

    if (error == 0) {
        error = read_tp_inf();
    }

    if (error == 0) {
        error = read_fac_data();
    }

    if (error == 0) {
        error = read_ini_data();
    }

    if (error == 0) {
        log_fac_result();
    }

    return 0;
}

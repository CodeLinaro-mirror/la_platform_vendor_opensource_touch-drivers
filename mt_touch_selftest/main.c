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
    int16_t *open_high;
    int16_t *open_low;
    int16_t *rx_short;
    int16_t *tx_short;
    int16_t *rx_self;
    int16_t *tx_self;
} g_fac_data;

static struct {
    uint8_t *ini_buf;
    int16_t *open_min;
    int16_t *open_max;
    int16_t rx_short_min;
    int16_t tx_short_min;
    int16_t *rx_self_min;
    int16_t *rx_self_max;
    int16_t *tx_self_min;
    int16_t *tx_self_max;
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

    int open_high_num = g_tp_inf.tx_num * g_tp_inf.rx_num;
    int open_low_num = open_high_num;
    int rx_short_num = g_tp_inf.rx_num;
    int tx_short_num = g_tp_inf.tx_num;
    int rx_self_num = g_tp_inf.rx_num;
    int tx_self_num = g_tp_inf.tx_num;
    int fac_data_size = (open_high_num + open_low_num + rx_short_num + tx_short_num + rx_self_num + tx_self_num) * 2 + 32;

    // alloc memory
    g_fac_data.fac_buf = malloc(fac_data_size);
    if (g_fac_data.fac_buf == NULL) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    g_fac_data.open_high = malloc(open_high_num * sizeof(*g_fac_data.open_high));
    if (g_fac_data.open_high == NULL) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    g_fac_data.open_low = malloc(open_low_num * sizeof(*g_fac_data.open_low));
    if (g_fac_data.open_low == NULL) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    g_fac_data.rx_short = malloc(rx_short_num * sizeof(*g_fac_data.rx_short));
    if (g_fac_data.rx_short == NULL) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    g_fac_data.tx_short = malloc(tx_short_num * sizeof(*g_fac_data.tx_short));
    if (g_fac_data.tx_short == NULL) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    g_fac_data.rx_self = malloc(rx_self_num * sizeof(*g_fac_data.rx_self));
    if (g_fac_data.rx_self == NULL) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    g_fac_data.tx_self = malloc(tx_self_num * sizeof(*g_fac_data.tx_self));
    if (g_fac_data.tx_self == NULL) {
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

    file_size = read(g_debug_file, g_fac_data.fac_buf, fac_data_size);
    if (file_size != fac_data_size) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    LOG("\r\n");

    int16_t *fac_buf = (int16_t*)(g_fac_data.fac_buf + 4);
    for (int i = 0; i < open_high_num; i++) {
        g_fac_data.open_high[i] = *fac_buf;
        fac_buf++;
    }
    for (int i = 0; i < open_low_num; i++) {
        g_fac_data.open_low[i] = *fac_buf;
        fac_buf++;
    }
    for (int i = 0; i < rx_short_num; i++) {
        g_fac_data.rx_short[i] = *fac_buf;
        fac_buf++;
    }
    for (int i = 0; i < tx_short_num; i++) {
        g_fac_data.tx_short[i] = *fac_buf;
        fac_buf++;
    }
    for (int i = 0; i < rx_self_num; i++) {
        g_fac_data.rx_self[i] = *fac_buf;
        fac_buf++;
    }
    for (int i = 0; i < tx_self_num; i++) {
        g_fac_data.tx_self[i] = *fac_buf;
        fac_buf++;
    }

    LOG(">> open high\r\n");
    fac_buf = g_fac_data.open_high;
    for (int tx = 0; tx < g_tp_inf.tx_num; tx++) {
        LOG(">> TX%02d ", tx);
        for (int rx = 0; rx < g_tp_inf.rx_num; rx++) {
            LOG("%5d ", *fac_buf);
            fac_buf++;
        }
        LOG("\r\n");
    }

    LOG(">> open low\r\n");
    fac_buf = g_fac_data.open_low;
    for (int tx = 0; tx < g_tp_inf.tx_num; tx++) {
        LOG(">> TX%02d ", tx);
        for (int rx = 0; rx < g_tp_inf.rx_num; rx++) {
            LOG("%5d ", *fac_buf);
            fac_buf++;
        }
        LOG("\r\n");
    }

    LOG(">> rx short\r\n");
    fac_buf = g_fac_data.rx_short;
    LOG(">> ");
    for (int rx = 0; rx < g_tp_inf.rx_num; rx++) {
        LOG("%5d ", *fac_buf);
        fac_buf++;
    }
    LOG("\r\n");

    LOG(">> tx short\r\n");
    fac_buf = g_fac_data.tx_short;
    LOG(">> ");
    for (int tx = 0; tx < g_tp_inf.tx_num; tx++) {
        LOG("%5d ", *fac_buf);
        fac_buf++;
    }
    LOG("\r\n");

#if INCLUDE_SELF_TEST
    LOG(">> rx self\r\n");
    fac_buf = g_fac_data.rx_self;
    LOG(">> ");
    for (int rx = 0; rx < g_tp_inf.rx_num; rx++) {
        LOG("%5d ", *fac_buf);
        fac_buf++;
    }
    LOG("\r\n");

    LOG(">> tx self\r\n");
    fac_buf = g_fac_data.tx_self;
    LOG(">> ");
    for (int tx = 0; tx < g_tp_inf.tx_num; tx++) {
        LOG("%5d ", *fac_buf);
        fac_buf++;
    }
    LOG("\r\n");
#endif

    return 0;
}

int read_ini_data(void)
{
    LOG("read ini data ... ");

    g_ini_file = open("./factory.ini", O_RDONLY);
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

    g_ini_data.open_min = malloc(g_tp_inf.tx_num * g_tp_inf.rx_num * sizeof(*g_ini_data.open_min));
    if (g_ini_data.open_min == NULL) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    g_ini_data.open_max = malloc(g_tp_inf.tx_num * g_tp_inf.rx_num * sizeof(*g_ini_data.open_max));
    if (g_ini_data.open_max == NULL) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    g_ini_data.rx_self_min = malloc(g_tp_inf.rx_num * sizeof(*g_ini_data.rx_self_min));
    if (g_ini_data.rx_self_min == NULL) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    g_ini_data.rx_self_max = malloc(g_tp_inf.rx_num * sizeof(*g_ini_data.rx_self_max));
    if (g_ini_data.rx_self_max == NULL) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    g_ini_data.tx_self_min = malloc(g_tp_inf.tx_num * sizeof(*g_ini_data.tx_self_min));
    if (g_ini_data.tx_self_min == NULL) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    g_ini_data.tx_self_max = malloc(g_tp_inf.tx_num * sizeof(*g_ini_data.tx_self_max));
    if (g_ini_data.tx_self_max == NULL) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    // read ini data
    ssize_t file_size = read(g_ini_file, g_ini_data.ini_buf, file_stat.st_size);
    if (file_size != file_stat.st_size) {
        LOG("error(%d)\r\n", __LINE__);
        return -1;
    }

    // find open min
    int16_t *open_min = g_ini_data.open_min;
    for (int tx = 0; tx < g_tp_inf.tx_num; tx++) {
        char key_buf[32] = {0};
        snprintf(key_buf, sizeof(key_buf), "TX%dOpenMin=", tx);
        const char *dest_str = strstr(g_ini_data.ini_buf, key_buf);
        if (dest_str == NULL) {
            LOG("error(%d)\r\n", __LINE__);
            return -1;
        }
        dest_str += strlen(key_buf);
        for (int rx = 0; rx < g_tp_inf.rx_num; rx++) {
            if (rx != 0) {
                dest_str = strstr(dest_str, ",");
                if (dest_str == NULL) {
                    LOG("error(%d)\r\n", __LINE__);
                    return -1;
                }
                dest_str += strlen(",");
            }
            int num = 0;
            code = sscanf(dest_str, "%d", &num);
            if (code != 1) {
                LOG("error(%d)\r\n", __LINE__);
                return -1;
            }
            *open_min = num;
            open_min++;
        }
    }

    // find open max
    int16_t *open_max = g_ini_data.open_max;
    for (int tx = 0; tx < g_tp_inf.tx_num; tx++) {
        char key_buf[32] = {0};
        snprintf(key_buf, sizeof(key_buf), "TX%dOpenMax=", tx);
        const char *dest_str = strstr(g_ini_data.ini_buf, key_buf);
        if (dest_str == NULL) {
            LOG("error(%d)\r\n", __LINE__);
            return -1;
        }
        dest_str += strlen(key_buf);
        for (int rx = 0; rx < g_tp_inf.rx_num; rx++) {
            if (rx != 0) {
                dest_str = strstr(dest_str, ",");
                if (dest_str == NULL) {
                    LOG("error(%d)\r\n", __LINE__);
                    return -1;
                }
                dest_str += strlen(",");
            }
            int num = 0;
            code = sscanf(dest_str, "%d", &num);
            if (code != 1) {
                LOG("error(%d)\r\n", __LINE__);
                return -1;
            }
            *open_max = num;
            open_max++;
        }
    }

    // find short min
    if (1) {
        char key_buf[32] = {0};
        snprintf(key_buf, sizeof(key_buf), "FactoryRxShortTh=");
        const char *dest_str = strstr(g_ini_data.ini_buf, key_buf);
        if (dest_str == NULL) {
            LOG("error(%d)\r\n", __LINE__);
            return -1;
        }
        dest_str += strlen(key_buf);
        int num = 0;
        code = sscanf(dest_str, "%d", &num);
        if (code != 1) {
            LOG("error(%d)\r\n", __LINE__);
            return -1;
        }
        g_ini_data.rx_short_min = num;
    }
    if (1) {
        char key_buf[32] = {0};
        snprintf(key_buf, sizeof(key_buf), "FactoryTxShortTh=");
        const char *dest_str = strstr(g_ini_data.ini_buf, key_buf);
        if (dest_str == NULL) {
            LOG("error(%d)\r\n", __LINE__);
            return -1;
        }
        dest_str += strlen(key_buf);
        int num = 0;
        code = sscanf(dest_str, "%d", &num);
        if (code != 1) {
            LOG("error(%d)\r\n", __LINE__);
            return -1;
        }
        g_ini_data.tx_short_min = num;
    }

    // find rx self min
    int16_t *rx_self_min = g_ini_data.rx_self_min;
    if (1) {
        char key_buf[32] = {0};
        snprintf(key_buf, sizeof(key_buf), "RxSCapScanMin=");
        const char *dest_str = strstr(g_ini_data.ini_buf, key_buf);
        if (dest_str == NULL) {
            LOG("error(%d)\r\n", __LINE__);
            return -1;
        }
        dest_str += strlen(key_buf);
        for (int rx = 0; rx < g_tp_inf.rx_num; rx++) {
            if (rx != 0) {
                dest_str = strstr(dest_str, ",");
                if (dest_str == NULL) {
                    LOG("error(%d)\r\n", __LINE__);
                    return -1;
                }
                dest_str += strlen(",");
            }
            int num = 0;
            code = sscanf(dest_str, "%d", &num);
            if (code != 1) {
                LOG("error(%d)\r\n", __LINE__);
                return -1;
            }
            *rx_self_min = num;
            rx_self_min++;
        }
    }

    // find rx self max
    int16_t *rx_self_max = g_ini_data.rx_self_max;
    if (1) {
        char key_buf[32] = {0};
        snprintf(key_buf, sizeof(key_buf), "RxSCapScanMax=");
        const char *dest_str = strstr(g_ini_data.ini_buf, key_buf);
        if (dest_str == NULL) {
            LOG("error(%d)\r\n", __LINE__);
            return -1;
        }
        dest_str += strlen(key_buf);
        for (int rx = 0; rx < g_tp_inf.rx_num; rx++) {
            if (rx != 0) {
                dest_str = strstr(dest_str, ",");
                if (dest_str == NULL) {
                    LOG("error(%d)\r\n", __LINE__);
                    return -1;
                }
                dest_str += strlen(",");
            }
            int num = 0;
            code = sscanf(dest_str, "%d", &num);
            if (code != 1) {
                LOG("error(%d)\r\n", __LINE__);
                return -1;
            }
            *rx_self_max = num;
            rx_self_max++;
        }
    }

    // find tx self min
    int16_t *tx_self_min = g_ini_data.tx_self_min;
    if (1) {
        char key_buf[32] = {0};
        snprintf(key_buf, sizeof(key_buf), "TxSCapScanMin=");
        const char *dest_str = strstr(g_ini_data.ini_buf, key_buf);
        if (dest_str == NULL) {
            LOG("error(%d)\r\n", __LINE__);
            return -1;
        }
        dest_str += strlen(key_buf);
        for (int tx = 0; tx < g_tp_inf.tx_num; tx++) {
            if (tx != 0) {
                dest_str = strstr(dest_str, ",");
                if (dest_str == NULL) {
                    LOG("error(%d)\r\n", __LINE__);
                    return -1;
                }
                dest_str += strlen(",");
            }
            int num = 0;
            code = sscanf(dest_str, "%d", &num);
            if (code != 1) {
                LOG("error(%d)\r\n", __LINE__);
                return -1;
            }
            *tx_self_min = num;
            tx_self_min++;
        }
    }

    // find tx self max
    int16_t *tx_self_max = g_ini_data.tx_self_max;
    if (1) {
        char key_buf[32] = {0};
        snprintf(key_buf, sizeof(key_buf), "TxSCapScanMax=");
        const char *dest_str = strstr(g_ini_data.ini_buf, key_buf);
        if (dest_str == NULL) {
            LOG("error(%d)\r\n", __LINE__);
            return -1;
        }
        dest_str += strlen(key_buf);
        for (int tx = 0; tx < g_tp_inf.tx_num; tx++) {
            if (tx != 0) {
                dest_str = strstr(dest_str, ",");
                if (dest_str == NULL) {
                    LOG("error(%d)\r\n", __LINE__);
                    return -1;
                }
                dest_str += strlen(",");
            }
            int num = 0;
            code = sscanf(dest_str, "%d", &num);
            if (code != 1) {
                LOG("error(%d)\r\n", __LINE__);
                return -1;
            }
            *tx_self_max = num;
            tx_self_max++;
        }
    }

    LOG("\r\n");

    LOG(">> open minimum\r\n");
    open_min = g_ini_data.open_min;
    for (int tx = 0; tx < g_tp_inf.tx_num; tx++) {
        LOG(">> TX%2d ", tx);
        for (int rx = 0; rx < g_tp_inf.rx_num; rx++) {
            LOG("%5d ", *open_min);
            open_min++;
        }
        LOG("\r\n");
    }

    LOG(">> open maximum\r\n");
    open_max = g_ini_data.open_max;
    for (int tx = 0; tx < g_tp_inf.tx_num; tx++) {
        LOG(">> TX%2d ", tx);
        for (int rx = 0; rx < g_tp_inf.rx_num; rx++) {
            LOG("%5d ", *open_max);
            open_max++;
        }
        LOG("\r\n");
    }

    LOG(">> rx short minimum : %d\r\n", g_ini_data.rx_short_min);
    LOG(">> tx short minimum : %d\r\n", g_ini_data.tx_short_min);

    LOG(">> rx self minimum\r\n");
    rx_self_min = g_ini_data.rx_self_min;
    LOG(">> ");
    for (int rx = 0; rx < g_tp_inf.rx_num; rx++) {
        LOG("%5d ", *rx_self_min);
        rx_self_min++;
    }
    LOG("\r\n");

    LOG(">> rx self maximum\r\n");
    rx_self_max = g_ini_data.rx_self_max;
    LOG(">> ");
    for (int rx = 0; rx < g_tp_inf.rx_num; rx++) {
        LOG("%5d ", *rx_self_max);
        rx_self_max++;
    }
    LOG("\r\n");

    LOG(">> tx self minimum\r\n");
    tx_self_min = g_ini_data.tx_self_min;
    LOG(">> ");
    for (int tx = 0; tx < g_tp_inf.tx_num; tx++) {
        LOG("%5d ", *tx_self_min);
        tx_self_min++;
    }
    LOG("\r\n");

    LOG(">> tx self maximum\r\n");
    tx_self_max = g_ini_data.tx_self_max;
    LOG(">> ");
    for (int tx = 0; tx < g_tp_inf.tx_num; tx++) {
        LOG("%5d ", *tx_self_max);
        tx_self_max++;
    }
    LOG("\r\n");

    return 0;
}

void log_fac_result(void)
{
    int error = 0;

    error = 0;
    LOG("log open minimum ...\r\n");
    LOG(">> ");
    int16_t index = 0;
    for (int tx = 0; tx < g_tp_inf.tx_num; tx++) {
        for (int rx = 0; rx < g_tp_inf.rx_num; rx++) {
            if (g_fac_data.open_high[index] - g_fac_data.open_low[index] < g_ini_data.open_min[index]) {
                LOG("(%d,%d) ", tx, rx);
                error = -1;
            }
            index++;
        }
    }
    LOG("%s\r\n", error ? "NG" : "PASS");

    error = 0;
    index = 0;
    LOG("log open maximum ... \r\n");
    LOG(">> ");
    for (int tx = 0; tx < g_tp_inf.tx_num; tx++) {
        for (int rx = 0; rx < g_tp_inf.rx_num; rx++) {
            if (g_fac_data.open_high[index] - g_fac_data.open_low[index] > g_ini_data.open_max[index]) {
                LOG("(%d,%d) ", tx, rx);
                error = -1;
            }
            index++;
        }
    }
    LOG("%s\r\n", error ? "NG" : "PASS");

    error = 0;
    LOG("log rx short minimum ...\r\n");
    LOG(">> ");
    for (int rx = 0; rx < g_tp_inf.rx_num; rx++) {
        if (g_fac_data.rx_short[rx] < g_ini_data.rx_short_min) {
            LOG("%2d ", rx);
            error = -1;
        }
    }
    LOG("%s\r\n", error ? "NG" : "PASS");

    error = 0;
    LOG("log tx short minimum ...\r\n");
    LOG(">> ");
    for (int tx = 0; tx < g_tp_inf.tx_num; tx++) {
        if (g_fac_data.tx_short[tx] < g_ini_data.tx_short_min) {
            LOG("%2d ", tx);
            error = -1;
        }
    }
    LOG("%s\r\n", error ? "NG" : "PASS");

#if INCLUDE_SELF_TEST
    error = 0;
    LOG("log rx self minimum ...\r\n");
    LOG(">> ");
    for (int rx = 0; rx < g_tp_inf.rx_num; rx++) {
        if (g_fac_data.rx_self[rx] < g_ini_data.rx_self_min[rx]) {
            LOG("%2d ", rx);
            error = -1;
        }
    }
    LOG("%s\r\n", error ? "NG" : "PASS");

    error = 0;
    LOG("log rx self maximum ...\r\n");
    LOG(">> ");
    for (int rx = 0; rx < g_tp_inf.rx_num; rx++) {
        if (g_fac_data.rx_self[rx] > g_ini_data.rx_self_max[rx]) {
            LOG("%2d ", rx);
            error = -1;
        }
    }
    LOG("%s\r\n", error ? "NG" : "PASS");

    error = 0;
    LOG("log tx self minimum ...\r\n");
    LOG(">> ");
    for (int tx = 0; tx < g_tp_inf.tx_num; tx++) {
        if (g_fac_data.tx_self[tx] > g_ini_data.tx_self_min[tx]) {
            LOG("%2d ", tx);
            error = -1;
        }
    }
    LOG("%s\r\n", error ? "NG" : "PASS");

    error = 0;
    LOG("log tx self maximum ...\r\n");
    LOG(">> ");
    for (int tx = 0; tx < g_tp_inf.tx_num; tx++) {
        if (g_fac_data.tx_self[tx] > g_ini_data.tx_self_max[tx]) {
            LOG("%2d ", tx);
            error = -1;
        }
    }
    LOG("%s\r\n", error ? "NG" : "PASS");
#endif
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

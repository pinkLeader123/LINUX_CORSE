#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/kthread.h>

// Tên driver này PHẢI KHỚP với tên trong id_table
#define DRIVER_NAME "combined-i2c-driver"
#define OLED_COMPATIBLE "solomon,ssd1306"
#define DS1307_COMPATIBLE "dallas,ds1307"

// Định nghĩa các API của OLED (Giả định nằm trong oled.h)
#include "oled.h"

// Globals để share clients giữa DS1307 và OLED cho kthread
static struct i2c_client *oled_client_global;
static struct i2c_client *rtc_client_global;
static struct task_struct *display_kthread;

int oled_write_cmd(struct i2c_client *client, u8 cmd)
{
    u8 buf[2] = {0x00, cmd};
    int ret = i2c_master_send(client, buf, 2);
    if (ret != 2) {
        dev_err(&client->dev, "Failed to send command 0x%02x: %d\n", cmd, ret);
        return (ret < 0) ? ret : -EIO;
    }
    return 0;
}

int oled_write_2byte_cmd(struct i2c_client *client, u8 *cmd)
{
    u8 buf[3] = {0x00, cmd[0], cmd[1]};
    int ret = i2c_master_send(client, buf, 3);
    if (ret != 3) {
        dev_err(&client->dev, "Failed to send 2-byte command 0x%02x 0x%02x: %d\n",
                cmd[0], cmd[1], ret);
        return (ret < 0) ? ret : -EIO;
    }
    return 0;
}

int oled_write_data(struct i2c_client *client, u8 data)
{
    u8 buf[2] = {0x40, data};
    int ret = i2c_master_send(client, buf, 2);
    if (ret != 2) {
        dev_err(&client->dev, "Failed to send data 0x%02x: %d\n", data, ret);
        return (ret < 0) ? ret : -EIO;
    }
    return 0;
}

int oled_hw_init(struct i2c_client *client)
{
    int ret, i;
    u8 cmd[][2] = {
        {0xA8, 0x3F}, // Multiplex ratio: 64
        {0xD3, 0x00}, // Display offset: 0
        {0xDA, 0x12}, // COM pins configuration
        {0x81, 0x7F}, // Contrast control
        {0xD5, 0x80}, // Display clock divide
        {0x8D, 0x14}, // Charge pump
        {0x20, 0x00}, // Memory addressing mode: horizontal
    };
    u8 single_cmds[] = {0x40, 0xA1, 0xC8, 0xA4, 0xA6, 0xAF};

    for (i = 0; i < ARRAY_SIZE(cmd); i++) {
        ret = oled_write_2byte_cmd(client, cmd[i]);
        if (ret < 0)
            return ret;
    }
    for (i = 0; i < ARRAY_SIZE(single_cmds); i++) {
        ret = oled_write_cmd(client, single_cmds[i]);
        if (ret < 0)
            return ret;
    }
    msleep(100); // Delay sau init để ổn định
    return 0;
}

void oled_blank(struct i2c_client *client)
{
    int i, ret;
    u8 buf[OLED_WIDTH + 1];
    buf[0] = 0x40;
    for (i = 1; i <= OLED_WIDTH; i++)
        buf[i] = 0x00;

    for (i = 0; i < OLED_PAGES; i++) {
        oled_write_cmd(client, 0xB0 + i);
        oled_write_cmd(client, 0x00);
        oled_write_cmd(client, 0x10);
        ret = i2c_master_send(client, buf, OLED_WIDTH + 1);
        if (ret != OLED_WIDTH + 1) {
            dev_err(&client->dev, "Failed to blank page %d: %d\n", i, ret);
        }
    }
}

void oled_clear_page(struct i2c_client *client, u8 page)
{
    int ret;
    int i; 
    u8 buf[OLED_WIDTH + 1];
    buf[0] = 0x40;
    for (i = 1; i <= OLED_WIDTH; i++)
        buf[i] = 0x00;

    oled_write_cmd(client, 0xB0 + page);
    oled_write_cmd(client, 0x00);
    oled_write_cmd(client, 0x10);
    ret = i2c_master_send(client, buf, OLED_WIDTH + 1);
    if (ret != OLED_WIDTH + 1) {
        dev_err(&client->dev, "Failed to clear page %d: %d\n", page, ret);
    }
}

void oled_print(struct i2c_client *client, u8 *str)
{
    int i, j;
    for (i = 0; str[i] && i < (OLED_WIDTH / 6); i++) {
        for (j = 0; j < 5; j++) {
            oled_write_data(client, ASCII[str[i] - 32][j]);
        }
        oled_write_data(client, 0x00); // Space between chars
    }
}

void oled_msg(struct i2c_client *client, u8 Ypos, u8 Xpos, u8 *str)
{
    if (Ypos >= OLED_PAGES || Xpos >= OLED_WIDTH)
        return;
    oled_write_cmd(client, 0x00 + (0x0F & Xpos));
    oled_write_cmd(client, 0x10 + (0x0F & (Xpos >> 4)));
    oled_write_cmd(client, 0xB0 + Ypos);
    oled_print(client, str);
}

int DS1307_tx(struct i2c_client *client, u8 reg, u8 *data, int data_len)
{
    int ret;
    u8 *buf;
    if (data_len <= 0)
        return -EINVAL;
    buf = kmalloc(1 + data_len, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;
    buf[0] = reg;
    memcpy(&buf[1], data, data_len);
    ret = i2c_master_send(client, buf, 1 + data_len);
    kfree(buf);
    if (ret != 1 + data_len) {
        dev_err(&client->dev, "Failed to write reg 0x%02x (len %d): %d\n", reg, data_len, ret);
        return (ret < 0) ? ret : -EIO;
    }
    return 0;
}

int DS1307_rx(struct i2c_client *client, u8 reg, u8 *str, int data_len)
{
    int ret;
    if (data_len <= 0)
        return -EINVAL;
    // Bước 1: Write reg address
    ret = i2c_master_send(client, &reg, 1);
    if (ret != 1) {
        dev_err(&client->dev, "Failed to write reg 0x%02x: %d\n", reg, ret);
        return (ret < 0) ? ret : -EIO;
    }
    // Bước 2: Read data_len bytes
    ret = i2c_master_recv(client, str, data_len);
    if (ret != data_len) {
        dev_err(&client->dev, "Failed to read %d bytes from reg 0x%02x: %d\n", data_len, reg, ret);
        return (ret < 0) ? ret : -EIO;
    }
    return 0;
}

u8 DS1307_converter(u8 date)
{
    u8 tens = date / 10;
    u8 units = date % 10;
    return (tens << 4) | units;
}

int DS1307_reverter(u8 time)
{
    u8 tens = time >> 4;
    u8 units = time & 0x0F;
    return (tens * 10) + units;
}

int DS1307_update_sec(struct i2c_client *client, u8 sec)
{
    u8 bcd_sec = DS1307_converter(sec);
    return DS1307_tx(client, 0x00, &bcd_sec, 1);
}

int DS1307_update_min(struct i2c_client *client, u8 min)
{
    u8 bcd_min = DS1307_converter(min);
    return DS1307_tx(client, 0x01, &bcd_min, 1);
}

int DS1307_update_hrs(struct i2c_client *client, u8 hrs)
{
    u8 bcd_hrs = DS1307_converter(hrs);
    return DS1307_tx(client, 0x02, &bcd_hrs, 1);
}

int DS1307_update_time(struct i2c_client *client, u8 hrs, u8 min, u8 sec)
{
    int ret_sec = DS1307_update_sec(client, sec);
    int ret_min = DS1307_update_min(client, min);
    int ret_hrs = DS1307_update_hrs(client, hrs);
    if (ret_sec < 0 || ret_min < 0 || ret_hrs < 0)
        return -EIO;
    return 0;
}

int DS1307_get_time(struct i2c_client *client, u8 *str)
{
    return DS1307_rx(client, 0x00, str, 7); // Đọc 7 bytes
}

void int2str(int val, char str[])
{
    if (val < 0) val = 0; // Clamp nếu negative
    if (val > 99) val = 99; // Clamp cho BCD
    if (val < 10) {
        str[0] = '0';
        str[1] = '0' + val;
    } else {
        str[0] = '0' + (val / 10);
        str[1] = '0' + (val % 10);
    }
    str[2] = '\0';
}

static void example_usage(struct i2c_client *client)
{
    u8 raw_time[7];
    char sec_str[3], min_str[3], hrs_str[3];
    int ret;
    int sec, min, hrs;
    ret = DS1307_get_time(client, raw_time);
    if (ret < 0)
        return;
    // Revert BCD to decimal
    sec = DS1307_reverter(raw_time[0] & 0x7F); // Mask CH bit
    min = DS1307_reverter(raw_time[1]);
    hrs = DS1307_reverter(raw_time[2]);
    // Format each
    int2str(hrs, hrs_str);
    int2str(min, min_str);
    int2str(sec, sec_str);
    dev_info(&client->dev, "Time: %s:%s:%s\n", hrs_str, min_str, sec_str);
}

// Kthread function để display time trên OLED
static int background_task(void *data)
{
    dev_info(&oled_client_global->dev, "%s: Kthread started successfully.\n", DRIVER_NAME);
    u8 raw_time[7];
    char sec_str[3], min_str[3], hrs_str[3];
    char time_buf[9]; // "HH:MM:SS\0"
    int ret;
    int sec, min, hrs;

    while (!kthread_should_stop()) {
        if (!rtc_client_global || !oled_client_global) {
            dev_err(&oled_client_global->dev, "%s: Missing client (OLED or RTC), retrying...\n", DRIVER_NAME);
            msleep(1000);
            continue;
        }

        ret = DS1307_get_time(rtc_client_global, raw_time);
        if (ret < 0) {
            dev_err(&rtc_client_global->dev, "%s: Error getting time: %d, retrying...\n", DRIVER_NAME, ret);
            msleep(1000);
            continue;
        }

        // Revert BCD to decimal
        sec = DS1307_reverter(raw_time[0] & 0x7F); // Mask CH bit
        min = DS1307_reverter(raw_time[1]);
        hrs = DS1307_reverter(raw_time[2]);

        // Format full time string
        int2str(hrs, hrs_str);
        int2str(min, min_str);
        int2str(sec, sec_str);
        snprintf(time_buf, sizeof(time_buf), "%s:%s:%s", hrs_str, min_str, sec_str);

        // Clear page and display
        oled_clear_page(oled_client_global, 4);
        oled_msg(oled_client_global, 4, 0, (u8 *)time_buf);

        msleep(700);
    }
    dev_info(&oled_client_global->dev, "%s: Kthread terminated.\n", DRIVER_NAME);
    return 0;
}

static int combined_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret;
    const char str[] = "Dang van phuc";

    // 1. Kiểm tra NULL client (Thao tác chuẩn)
    if (!client) {
        pr_err("I2C: Client structure is NULL.\n");
        return -ENODEV;
    }
    dev_info(&client->dev, "I2C: Probe started for %s at address 0x%02x.\n", client->name, client->addr);

    // 2. PHÂN LOẠI THIẾT BỊ DỰA TRÊN DEVICE TREE (DT)
    // --- KHỐI A: Xử lý SSD1306 OLED ---
    if (of_device_is_compatible(client->dev.of_node, OLED_COMPATIBLE)) {
        dev_info(&client->dev, "OLED: Initialization sequence started.\n");
        ret = oled_hw_init(client);
        if (ret) {
            dev_err(&client->dev, "OLED init failed (Error: %d)\n", ret);
            return ret;
        }
        oled_blank(client);
        oled_msg(client, 2, 3, (u8 *)str);
        oled_client_global = client;
        dev_info(&client->dev, "SSD1306: Probe successful. Display initialized.\n");
    }
    // --- KHỐI B: Xử lý DS1307 RTC ---
    else if (of_device_is_compatible(client->dev.of_node, DS1307_COMPATIBLE)) {
        dev_info(&client->dev, "DS1307: Initialization sequence started. Setting time to 10:30:00.\n");
        ret = DS1307_update_time(client, 10, 30, 0);
        if (ret) {
            dev_err(&client->dev, "DS1307 set time failed (Error: %d)\n", ret);
            return ret;
        }
        // Thử đọc lại thời gian để xác minh
        example_usage(client);
        rtc_client_global = client;
        dev_info(&client->dev, "DS1307: Probe successful. RTC is ready.\n");

        // Start kthread chỉ khi cả OLED và RTC đã probe
        if (oled_client_global && rtc_client_global) {
            display_kthread = kthread_run(background_task, NULL, DRIVER_NAME "_display");
            if (IS_ERR(display_kthread)) {
                ret = PTR_ERR(display_kthread);
                dev_err(&client->dev, "%s: Failed to create kernel thread: %d\n", DRIVER_NAME, ret);
                return ret;
            }
            dev_info(&client->dev, "%s: Display thread started successfully.\n", DRIVER_NAME);
        } else {
            dev_warn(&client->dev, "%s: Waiting for OLED to start display thread.\n", DRIVER_NAME);
        }
    }
    // --- Xử lý Lỗi ---
    else {
        dev_warn(&client->dev, "I2C: Unknown device bound to this driver: %s\n", client->name);
        return -ENODEV;
    }
    return 0;
}

static int combined_i2c_remove(struct i2c_client *client)
{
    dev_info(&client->dev, "%s: Remove called for 0x%02x.\n", DRIVER_NAME, client->addr);

    // Dọn dẹp kthread nếu là DS1307
    if (of_device_is_compatible(client->dev.of_node, DS1307_COMPATIBLE) && display_kthread) {
        kthread_stop(display_kthread);
        display_kthread = NULL;
        dev_info(&client->dev, "%s: Kthread stopped.\n", DRIVER_NAME);
    }

    // Dọn dẹp OLED
    if (of_device_is_compatible(client->dev.of_node, OLED_COMPATIBLE)) {
        oled_blank(client);
        oled_client_global = NULL;
        dev_info(&client->dev, "OLED: Display blanked.\n");
    }

    // Clear RTC global
    if (of_device_is_compatible(client->dev.of_node, DS1307_COMPATIBLE)) {
        rtc_client_global = NULL;
    }

    dev_info(&client->dev, "%s: Resources released.\n", DRIVER_NAME);
    return 0;
}

static const struct of_device_id combined_of_match[] = {
    // PHẢI KHỚP VỚI CHUỖI compatible TRONG DTS!
    { .compatible = OLED_COMPATIBLE },
    { .compatible = DS1307_COMPATIBLE },
    { }
};
MODULE_DEVICE_TABLE(of, combined_of_match); // Macro xuất bảng này ra kernel

static const struct i2c_device_id combined_id[] = {
    { "ssd1306", 0 },
    { "ds1307", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, combined_id); // Macro xuất bảng này ra kernel

static struct i2c_driver combined_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = combined_of_match,
    },
    .probe = combined_i2c_probe,
    .remove = combined_i2c_remove,
    .id_table = combined_id,
};

module_i2c_driver(combined_driver);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dang Van Phuc");
MODULE_DESCRIPTION("Driver for SSD1306 OLED and DS1307 RTC using I2C.");
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/time.h>

// Tên driver này PHẢI KHỚP với tên trong id_table
#define DRIVER_NAME "combined-i2c-driver"
#define OLED_COMPATIBLE "solomon,ssd1306"
#define DS1307_COMPATIBLE "dallas,ds1307"
#define MISC_DEVICE_NAME "rtc_oled_time"

// Định nghĩa các API của OLED (Giả định nằm trong oled.h)
#include "oled.h"

// Globals để share clients giữa DS1307 và OLED cho kthread
static struct i2c_client *oled_client_global;
static struct i2c_client *rtc_client_global;
static struct task_struct *display_kthread;
static int time; //gio phut giay -> giay; 

#define LED_IOC_MAGIC 'k'
#define GET_TIME_CMD _IOR(LED_IOC_MAGIC, 1, int)

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


int time2sec(int h, int m, int s){
    return h*60*60 + m * 60 + s; 
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
        
        time = time2sec(hrs,min,sec); 

        // Format full time string
        int2str(hrs, hrs_str);
        int2str(min, min_str);
        int2str(sec, sec_str);
        snprintf(time_buf, sizeof(time_buf), "%s:%s:%s", hrs_str, min_str, sec_str);

        // Clear page and display
        oled_clear_page(oled_client_global, 4);
        oled_msg(oled_client_global, 4, 0, (u8 *)time_buf);

        msleep(10);
    }
    dev_info(&oled_client_global->dev, "%s: Kthread terminated.\n", DRIVER_NAME);
    return 0;
}

static long time_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret; 
    switch (cmd) {

    case GET_TIME_CMD:
        
        // Copy số nguyên 4 byte (current_led_status) lên userspace
        if (copy_to_user((int __user *)arg, &time, sizeof(time))) {
            return -EFAULT; // Lỗi truy cập bộ nhớ
        }
        pr_info("IOCTL: Returning status %d\n", time);
        ret = 0;
        break;


    default:
        pr_warn("IOCTL: Unknown command 0x%x\n", cmd);
        ret = -ENOTTY; // Command không được hỗ trợ
    }
    return ret; 
}


// --- KHỐI FILE OPERATIONS VÀ MISC DEVICE ---

static int misc_open(struct inode *node, struct file *filep) { return 0; }
static int misc_release(struct inode *node, struct file *filep) { return 0; }

static const struct file_operations combined_misc_fops = {
    .owner = THIS_MODULE,
    .open = misc_open,
    .release = misc_release,
    .unlocked_ioctl = time_ioctl, // <-- ĐĂNG KÝ HÀM IOCTL
};

static struct miscdevice combined_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = MISC_DEVICE_NAME, 
    .fops = &combined_misc_fops,
};

static int combined_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0;
    const char str[] = "Dang van phuc";
    
    // Kiểm tra cấu trúc client
    if (!client) {
        dev_err(&client->dev, "I2C: Client structure is NULL.\n");
        return -ENODEV;
    }
    dev_info(&client->dev, "I2C: Probe started for %s at address 0x%02x.\n", client->name, client->addr);

    // --- KHỐI A: Xử lý SSD1306 OLED ---
    if (of_device_is_compatible(client->dev.of_node, OLED_COMPATIBLE)) {
        dev_info(&client->dev, "OLED: Initialization sequence started.\n");
        
        // Khởi tạo phần cứng OLED
        ret = oled_hw_init(client);
        if (ret) return ret; 
        
        oled_blank(client);
        oled_msg(client, 2, 3, (u8 *)str);
        oled_client_global = client; // Lưu client toàn cục
        
        dev_info(&client->dev, "SSD1306: Probe successful. Display initialized.\n");
    }
    
    // --- KHỐI B: Xử lý DS1307 RTC ---
    else if (of_device_is_compatible(client->dev.of_node, DS1307_COMPATIBLE)) {
        dev_info(&client->dev, "DS1307: Initialization sequence started. Setting time to 00:00:00.\n");
        
        // Khởi tạo và đặt thời gian
        ret = DS1307_update_time(client, 0, 0, 0); 
        if (ret) {
            dev_err(&client->dev, "DS1307 set time failed (Error: %d)\n", ret);
            return ret;
        }
        rtc_client_global = client; // Lưu client toàn cục
        
        // 1. ĐĂNG KÝ MISC DEVICE (Chỉ khi RTC probe thành công)
        ret = misc_register(&combined_misc_device);
        if (ret) {
            dev_err(&client->dev, "%s: Failed to register misc device: %d\n", DRIVER_NAME, ret);
            rtc_client_global = NULL; // Cleanup global
            return ret;
        }
        dev_info(&client->dev, "%s: Misc device registered at /dev/%s\n", DRIVER_NAME, MISC_DEVICE_NAME);
    }

    // 2. Start kthread (KHÔNG TRONG ELSE-IF KHÁC)
    // Sau khi xử lý xong client hiện tại, kiểm tra nếu cả hai đã sẵn sàng.
    if (oled_client_global && rtc_client_global && !display_kthread) {
        display_kthread = kthread_run(background_task, NULL, DRIVER_NAME "_display");
        if (IS_ERR(display_kthread)) {
            ret = PTR_ERR(display_kthread);
            dev_err(&client->dev, "%s: Failed to create kernel thread: %d\n", DRIVER_NAME, ret);
            // Cleanup: Đăng ký Misc thất bại, cần hủy đăng ký (Chỉ xảy ra nếu RTC probe sau OLED)
            if (of_device_is_compatible(client->dev.of_node, DS1307_COMPATIBLE)) {
                misc_deregister(&combined_misc_device);
                rtc_client_global = NULL; 
            }
            return ret;
        }
        dev_info(&client->dev, "%s: Display thread started successfully.\n", DRIVER_NAME);
    } else if (of_device_is_compatible(client->dev.of_node, DS1307_COMPATIBLE)) {
        // Chỉ hiện cảnh báo nếu là DS1307 (vì DS1307 chịu trách nhiệm khởi động thread)
        dev_warn(&client->dev, "%s: Waiting for OLED to start display thread.\n", DRIVER_NAME);
    }
    
    return 0;
}

static int combined_i2c_remove(struct i2c_client *client)
{
    dev_info(&client->dev, "%s: Remove called for 0x%02x.\n", DRIVER_NAME, client->addr);

    // Dọn dẹp kthread và Misc Device (Chỉ xảy ra khi client DS1307 bị remove)
    if (of_device_is_compatible(client->dev.of_node, DS1307_COMPATIBLE)) {
        
        // 1. Dừng Kthread (Quan trọng: Phải dừng trước khi dọn dẹp các clients)
        if (display_kthread) {
            kthread_stop(display_kthread);
            display_kthread = NULL;
            dev_info(&client->dev, "%s: Kthread stopped.\n", DRIVER_NAME);
        }
        
        // 2. Dọn dẹp Misc Device
        misc_deregister(&combined_misc_device);
        dev_info(&client->dev, "%s: Misc device deregistered.\n", DRIVER_NAME);
        
        // 3. Dọn dẹp Global RTC client
        rtc_client_global = NULL;
    }

    // Dọn dẹp OLED (Khi client OLED bị remove)
    if (of_device_is_compatible(client->dev.of_node, OLED_COMPATIBLE)) {
        oled_blank(client);
        oled_client_global = NULL;
        dev_info(&client->dev, "OLED: Display blanked and global client cleared.\n");
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/delay.h>

// Tên driver này PHẢI KHỚP với tên trong id_table
#define DRIVER_NAME "ssd1306" 

// Định nghĩa các API của OLED (Giả định nằm trong oled.h)
#include "oled.h"


static int oled_write_cmd(struct i2c_client *client, u8 cmd); 
static int oled_write_2byte_cmd(struct i2c_client *client, u8 *cmd); 

static int oled_write_data(struct i2c_client *client, u8 data); 
static int oled_hw_init(struct i2c_client *client);
static void oled_blank(struct i2c_client *client);
static void oled_print(struct i2c_client *client, u8 *str); 
static void oled_msg(struct i2c_client *client, u8 Ypos, u8 Xpos, u8 *str); 

static int oled_write_cmd(struct i2c_client *client, u8 cmd)
{
    u8 buf[2] = {0x00, cmd};
    int ret = i2c_master_send(client, buf, 2);
    if (ret < 0)
        dev_err(&client->dev, "Failed to send command 0x%02x: %d\n", cmd, ret);
    return ret;
}

static int oled_write_2byte_cmd(struct i2c_client *client, u8 *cmd)
{
    u8 buf[3] = {0x00, cmd[0], cmd[1]};
    int ret = i2c_master_send(client, buf, 3);
    if (ret < 0)
        dev_err(&client->dev, "Failed to send 2-byte command 0x%02x 0x%02x: %d\n",
                cmd[0], cmd[1], ret);
    return ret;
}

static int oled_write_data(struct i2c_client *client, u8 data)
{
    u8 buf[2] = {0x40, data};
    int ret = i2c_master_send(client, buf, 2);
    if (ret < 0)
        dev_err(&client->dev, "Failed to send data 0x%02x: %d\n", data, ret);
    return ret;
}

static int oled_hw_init(struct i2c_client *client)
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
    return 0;
}

static void oled_blank(struct i2c_client *client)
{
    int i;
    u8 buf[OLED_WIDTH + 1];

    buf[0] = 0x40;
    for (i = 1; i <= OLED_WIDTH; i++)
        buf[i] = 0x00;

    for (i = 0; i < OLED_PAGES; i++) {
        oled_write_cmd(client, 0xB0 + i);
        oled_write_cmd(client, 0x00);
        oled_write_cmd(client, 0x10);
        i2c_master_send(client, buf, OLED_WIDTH + 1);
    }
}

static void oled_print(struct i2c_client *client, u8 *str)
{
    int i, j;
    for (i = 0; str[i] && i < (OLED_WIDTH / 6); i++) {
        for (j = 0; j < 5; j++) {
            oled_write_data(client, ASCII[str[i] - 32][j]);
        }
        oled_write_data(client, 0x00);
    }
}

static void oled_msg(struct i2c_client *client, u8 Ypos, u8 Xpos, u8 *str)
{
    if (Ypos >= OLED_PAGES || Xpos >= OLED_WIDTH)
        return;
    oled_write_cmd(client, 0x00 + (0x0F & Xpos));
    oled_write_cmd(client, 0x10 + (0x0F & (Xpos >> 4)));
    oled_write_cmd(client, 0xB0 + Ypos);
    oled_print(client, str);
}


// -------------------------------------------------------------
// Hàm Probe: Chạy khi Kernel thấy node DT phù hợp
// -------------------------------------------------------------
static int oled_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret; 
    char str[] = "Dang van phuc"; // Chuỗi test

    // 1. Kiểm tra NULL client và địa chỉ
    if (!client) {
        pr_err("OLED: Client structure is NULL.\n");
        return -ENODEV;
    }
    
    // In thông tin về thiết bị mà driver đang bind
    pr_info("OLED: Probe started for %s at address 0x%02x.\n", client->name, client->addr);

    // 2. Khởi tạo Phần cứng OLED
    ret = oled_hw_init(client);
    if (ret) {
        dev_err(&client->dev, "OLED init failed (Error: %d)\n", ret); 
        return ret;
    }

    // 3. Logic Hiển thị
    oled_blank(client); 
    oled_msg(client, 2, 3, str); 

    // 4. Lưu trữ dữ liệu driver (nếu cần)
    // i2c_set_clientdata(client, data_struct_ptr);

    pr_info("SSD1306: Probe successful. Display initialized.\n"); 
    return 0; 
}

// -------------------------------------------------------------
// Hàm Remove: Chạy khi module bị gỡ bỏ hoặc thiết bị bị ngắt kết nối
// -------------------------------------------------------------
static int oled_remove(struct i2c_client *client)
{
    // Dọn dẹp: Tắt màn hình
    pr_info("SSD1306: Remove called. Blanking display.\n"); 
    // Giả định oled_blank có thể gọi an toàn
    oled_blank(client); 
    
    // Giải phóng tài nguyên (nếu có devm_ không dọn dẹp)
    
    return 0;
}

// -------------------------------------------------------------
// Bảng Match (Bắt buộc cho DT Binding và ID Table)
// -------------------------------------------------------------

// 1. Match cho Device Tree (OF - Open Firmware)
static const struct of_device_id oled_of_match[] = {
    // PHẢI KHỚP VỚI CHUỖI compatible TRONG DTS!
    { .compatible = "solomon,ssd1306" }, 
    { } // Dấu hiệu kết thúc
};
MODULE_DEVICE_TABLE(of, oled_of_match); // Macro xuất bảng này ra kernel

// 2. Match cho I2C truyền thống (ID Table)
static const struct i2c_device_id oled_id[] = {
    { DRIVER_NAME, 0 }, // Tên này KHỚP VỚI DRIVER_NAME
    { }
};
MODULE_DEVICE_TABLE(i2c, oled_id); // Macro xuất bảng này ra kernel

// -------------------------------------------------------------
// Cấu trúc I2C Driver
// -------------------------------------------------------------
static struct i2c_driver oled_driver = {
    .driver = {
        .name = DRIVER_NAME, // "ssd1306"
        .of_match_table = oled_of_match,
    },
    .probe = oled_probe, // Hàm probe chính
    .remove = oled_remove,
    .id_table = oled_id,
};

module_i2c_driver(oled_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dang Van Phuc");
MODULE_DESCRIPTION("Driver for SSD1306 OLED display using I2C.");
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/slab.h>

// Định nghĩa cấu trúc riêng để lưu trữ GPIO và timer
struct p8_08_led_data {
    struct gpio_desc *gpiod; // GPIO Descriptor
    struct timer_list blink_timer; // Timer cho nhấp nháy
    int state; // Trạng thái LED (0/1)
};


static void led_blink_function(struct timer_list *t)
{
    struct p8_08_led_data *data = from_timer(data, t, blink_timer);
    
    // Đảo trạng thái (toggle)
    data->state = !data->state;
    
    // Sử dụng GPIO Descriptor API để điều khiển chân
    gpiod_set_value(data->gpiod, data->state);
    
    // In log và sắp xếp lại timer (1 giây)
    pr_info("P8_08_LED: Toggled to %d\n", data->state);
    mod_timer(&data->blink_timer, jiffies + HZ); // HZ = 1 giây
}

static int p8_08_led_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct p8_08_led_data *data;
    int ret;

    pr_info("P8_08_LED: Probe started.\n");

    // 1. Phân bổ bộ nhớ cho dữ liệu driver
    data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    // 2. Lấy GPIO Descriptor từ Device Tree
    // Hàm này tự động gọi gpio_request và gpio_direction_output
    data->gpiod = devm_gpiod_get(dev,NULL, GPIOD_OUT_LOW);
    if (IS_ERR(data->gpiod)) {
        ret = PTR_ERR(data->gpiod);
        pr_err("P8_08_LED: Failed to get GPIO, error %d\n", ret);
        return ret;
    }
    
    // Đặt tên cho GPIO trong sysfs
    gpiod_set_consumer_name(data->gpiod, "P8_08_BLINK");
    
    // 3. Khởi tạo và kích hoạt Timer (nhấp nháy 1 giây)
    timer_setup(&data->blink_timer, led_blink_function, 0);
    data->state = 0; // Bắt đầu ở trạng thái OFF
    
    // Kích hoạt timer lần đầu (sau 1 giây)
    mod_timer(&data->blink_timer, jiffies + HZ); 
    
    // 4. Lưu trữ dữ liệu driver
    platform_set_drvdata(pdev, data);
    
    pr_info("P8_08_LED: Module loaded successfully. Blink started.\n");
    return 0;
}

static int p8_08_led_remove(struct platform_device *pdev)
{
    struct p8_08_led_data *data = platform_get_drvdata(pdev);

    pr_info("P8_08_LED: Remove called. Stopping blink.\n");

    // 1. Dừng timer (rất quan trọng)
    del_timer_sync(&data->blink_timer);

    // 2. Tắt LED trước khi gỡ module
    gpiod_set_value(data->gpiod, 0); 
    
    // devm_gpiod_get tự động gọi gpio_free

    return 0;
}

// Bảng match với thuộc tính compatible trong DTS
static const struct of_device_id p8_08_led_of_match[] = {
    { .compatible = "myvendor,p8-08-blink" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, p8_08_led_of_match);


static struct platform_driver p8_08_led_driver = {
    .probe = p8_08_led_probe,
    .remove = p8_08_led_remove,
    .driver = {
        .name = "p8-08-blink-driver",
        .of_match_table = p8_08_led_of_match,
    },
};

module_platform_driver(p8_08_led_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dang Van PHUC");
MODULE_DESCRIPTION("Simple GPIO LED P8.08 blinker based on Device Tree.");
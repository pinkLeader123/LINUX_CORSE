#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/ioctl.h>


#define LED_IOC_MAGIC 'k' 
#define LED_ON  _IOW(LED_IOC_MAGIC, 1, int) 
#define LED_OFF _IOW(LED_IOC_MAGIC, 2, int)
#define LED_GET_STATUS _IOR(LED_IOC_MAGIC, 3, int) 

#define GPIO_CTRL         0x130
#define GPIO_OE           0x134
#define GPIO_DATAIN       0x138
#define GPIO_DATAOUT      0x13C
#define GPIO_CLEARDATAOUT 0x190
#define GPIO_SETDATAOUT   0x194
#define GPIO1_BASE_ADDR   0x4804C000
#define GPIO_SIZE         0x1000
#define LED_PIN           (1<<28)

static void __iomem *gpio1_base = NULL; 
static int led_status = 0; 

char local_data[128];

int misc_open(struct inode *node, struct file *filep)
{
    pr_info("%s, %d\n", __func__, __LINE__);
    return 0;
}

int misc_release(struct inode *node, struct file *filep)
{
    pr_info("%s, %d\n", __func__, __LINE__);
    return 0;
}

static long led_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    int value = 0;

    pr_info("LED_Driver: IOCTL command 0x%x received.\n", cmd);

    switch (cmd) {
    case LED_ON:
        writel(LED_PIN, gpio1_base + GPIO_SETDATAOUT);  // Sửa: dùng SETDATAOUT
        if (copy_from_user(&value, (int __user *)arg, sizeof(value))) {
            return -EFAULT;
        }
        led_status = 1;
        pr_info("LED_Driver: LED ON, data received: %d\n", value);
        break;
    case LED_OFF:
        writel(LED_PIN, gpio1_base + GPIO_CLEARDATAOUT);
        led_status = 0;
        pr_info("LED_Driver: LED OFF.\n");
        break;
    case LED_GET_STATUS:
        value = led_status;
        if (copy_to_user((int __user *)arg, &value, sizeof(value))) {
            return -EFAULT;
        }
        pr_info("LED_Driver: GET STATUS, returning %d.\n", led_status);
        break;
    default:
        ret = -ENOTTY;
        pr_warn("LED_Driver: Unknown IOCTL command 0x%x\n", cmd);
    }
    return ret;
}

static ssize_t misc_read(struct file *filp, char __user *buf, size_t count,
                         loff_t *f_pos)
{
    pr_info("%s, %d\n", __func__, __LINE__);
    return 0;
}

static ssize_t misc_write(struct file *filp, const char __user *buf,
                          size_t count, loff_t *f_pos)
{
    pr_info("%s, %d\n", __func__, __LINE__);
    return count;
}

struct file_operations misc_fops = {
    .owner = THIS_MODULE,
    .open = misc_open, //Enable hardware
    .release = misc_release, //disable hardware, synchronize data xuong hardware
    .read = misc_read, //Doc du lieu tu hardware, luu vao buffer cua kernel
    .write = misc_write, //Ghi du lieu tu buffer cua kernel xuong hardware
    .unlocked_ioctl = led_ioctl,
};

static struct miscdevice misc_example = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "misc_example",
    .fops = &misc_fops,
};

static int misc_init(void)
{
    u32 reg;
    u32 i;
    int ret;

    pr_info("misc module init\n");
    gpio1_base = ioremap(GPIO1_BASE_ADDR, GPIO_SIZE);
    if (!gpio1_base) {
        pr_err("Raw GPIO Driver: Failed to ioremap GPIO1 base address.\n");
        return -ENOMEM;
    }
    pr_info("Raw GPIO Driver: GPIO1 remapped to %p\n", gpio1_base);

    reg = readl(gpio1_base + GPIO_OE);
    reg &= ~LED_PIN;
    writel(reg, gpio1_base + GPIO_OE);

    // Blink 3 times to test (sửa logic)
    for (i = 0; i < 3; i++) {
        writel(LED_PIN, gpio1_base + GPIO_SETDATAOUT);
        mdelay(100);
        writel(LED_PIN, gpio1_base + GPIO_CLEARDATAOUT);
        mdelay(100);
    }
    writel(LED_PIN, gpio1_base + GPIO_CLEARDATAOUT);  // Tắt cuối

    reg = readl(gpio1_base + GPIO_OE);  // Đọc lại để confirm
    pr_info("Raw GPIO Driver: Configured P9.12 as Output. OE = 0x%x\n", reg);

    ret = misc_register(&misc_example);
    if (ret) {
        pr_err("Failed to register misc device: %d\n", ret);
        iounmap(gpio1_base);
        return ret;
    }

    return 0;
}

static void misc_exit(void)
{
    pr_info("misc module exit\n");
    if (led_status) {
        writel(LED_PIN, gpio1_base + GPIO_CLEARDATAOUT);  // Tắt LED
    }
    if (gpio1_base) {
        iounmap(gpio1_base);
    }
    pr_info("Raw GPIO Driver: Module unloaded and memory unmapped.\n");
    misc_deregister(&misc_example);
}

module_init(misc_init);
module_exit(misc_exit);

MODULE_AUTHOR("Dang Van Phuc");
MODULE_DESCRIPTION("simple led misc driver.");
MODULE_LICENSE("GPL");
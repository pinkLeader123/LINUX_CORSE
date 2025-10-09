#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/jiffies.h>


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
#define GPIO0_BASE_ADDR   0x44E07000
#define GPIO_SIZE         0x1000
#define LED_PIN           (1<<28) //Gpio1_28
#define BUTTON_GPIO       30 //Gpio0_30
#define BUTTON_NAME       "button_irq"
#define DEBOUNCE_DELAY    msecs_to_jiffies(50)

static void __iomem *gpio1_base = NULL; 
static void __iomem *gpio0_base = NULL; 
static int led_status = 0; 
static struct delayed_work debounce_work;
static struct workqueue_struct *workqueue;

static void debounce_handler(struct work_struct *work) {
    if (led_status) {
        writel(LED_PIN, gpio1_base + GPIO_CLEARDATAOUT);
        led_status = 0;
    } else {
        writel(LED_PIN, gpio1_base + GPIO_SETDATAOUT);
        led_status = 1;
    }
    pr_info("LED_Driver: Button toggled LED to %d\n", led_status);
}

static irqreturn_t button_isr(int irq, void *dev_id)
{
    // Schedule debounce work để tránh spam ISR
    queue_delayed_work(workqueue, &debounce_work, DEBOUNCE_DELAY);
    return IRQ_HANDLED;
}


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

static int misc_init(void) {
    u32 reg;
    u32 i;
    int irq, ret = 0;

    pr_info("misc module init\n");

    // ioremap GPIO1
    gpio1_base = ioremap(GPIO1_BASE_ADDR, GPIO_SIZE);
    if (!gpio1_base) {
        pr_err("Failed to ioremap GPIO1\n");
        return -ENOMEM;
    }
    // Config LED output
    reg = readl(gpio1_base + GPIO_OE);
    reg &= ~LED_PIN;
    writel(reg, gpio1_base + GPIO_OE);

    // ioremap GPIO0 - SỬA
    gpio0_base = ioremap(GPIO0_BASE_ADDR, GPIO_SIZE);
    if (!gpio0_base) {
        pr_err("Failed to ioremap GPIO0\n");
        ret = -ENOMEM;
        goto err_iounmap_gpio1;
    }

    // Config button input - SỬA bit
    reg = readl(gpio0_base + GPIO_OE);
    reg |= (1 << 30);  // Bit 30 cho input
    writel(reg, gpio0_base + GPIO_OE);

    // GPIO request - THÊM
    ret = gpio_request(BUTTON_GPIO, BUTTON_NAME);
    if (ret) {
        pr_err("Failed to request GPIO %d: %d\n", BUTTON_GPIO, ret);
        goto err_iounmap_gpio0;
    }

    irq = gpio_to_irq(BUTTON_GPIO);
    if (irq < 0) {
        pr_err("Failed to get IRQ: %d\n", irq);
        ret = irq;
        goto err_gpio_free;
    }

    ret = request_irq(irq, button_isr, IRQF_TRIGGER_RISING | IRQF_ONESHOT, BUTTON_NAME, NULL);
    if (ret) {
        pr_err("Failed to request IRQ: %d\n", ret);
        goto err_gpio_free;
    }

    workqueue = create_singlethread_workqueue("button_work");
    if (!workqueue) {
        pr_err("Failed to create workqueue\n");
        ret = -ENOMEM;
        goto err_free_irq;
    }
    INIT_DELAYED_WORK(&debounce_work, debounce_handler);  // Bây giờ OK

    // Blink test
    for (i = 0; i < 3; i++) {
        writel(LED_PIN, gpio1_base + GPIO_SETDATAOUT);
        mdelay(100);
        writel(LED_PIN, gpio1_base + GPIO_CLEARDATAOUT);
        mdelay(100);
    }
    writel(LED_PIN, gpio1_base + GPIO_CLEARDATAOUT);

    ret = misc_register(&misc_example);
    if (ret) {
        pr_err("Failed to register misc: %d\n", ret);
        goto err_destroy_wq;
    }

    pr_info("Module loaded. IRQ: %d\n", irq);
    return 0;

err_destroy_wq:
    destroy_workqueue(workqueue);
err_free_irq:
    free_irq(irq, NULL);
err_gpio_free:
    gpio_free(BUTTON_GPIO);
err_iounmap_gpio0:
    iounmap(gpio0_base);
err_iounmap_gpio1:
    iounmap(gpio1_base);
    return ret;
}

static void misc_exit(void) {
    int irq = gpio_to_irq(BUTTON_GPIO);
    pr_info("misc module exit\n");

    if (led_status) writel(LED_PIN, gpio1_base + GPIO_CLEARDATAOUT);

    if (irq > 0) free_irq(irq, NULL);
    if (workqueue) destroy_workqueue(workqueue);
    gpio_free(BUTTON_GPIO);
    if (gpio0_base) iounmap(gpio0_base);
    if (gpio1_base) iounmap(gpio1_base);
    misc_deregister(&misc_example);

    pr_info("Module unloaded.\n");
}

module_init(misc_init);
module_exit(misc_exit);

MODULE_AUTHOR("Dang Van Phuc");
MODULE_DESCRIPTION("simple led misc driver.");
MODULE_LICENSE("GPL");
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/init.h>

#define GPIO_BUTTON   49   // P9_23 trên BBB (GPIO1_17 = (1*32)+17 = 49)
#define GPIO_LED      60   // P9_12 trên BBB (GPIO1_28 = (1*32)+28 = 60)

static int irq_number;
static bool led_on = false;

static irqreturn_t button_isr(int irq, void *data)
{
    led_on = !led_on;
    gpio_set_value(GPIO_LED, led_on);
    pr_info("BBB: Button pressed! LED = %d\n", led_on);
    return IRQ_HANDLED;
}

static int __init my_init(void)
{
    int ret;

    pr_info("BBB: Init button-irq-led module\n");

    // Request GPIO
    if (!gpio_is_valid(GPIO_LED) || !gpio_is_valid(GPIO_BUTTON)) {
        pr_err("BBB: Invalid GPIO\n");
        return -ENODEV;
    }

    gpio_request(GPIO_LED, "LED");
    gpio_direction_output(GPIO_LED, 0);

    gpio_request(GPIO_BUTTON, "Button");
    gpio_direction_input(GPIO_BUTTON);

    // Map GPIO to IRQ
    irq_number = gpio_to_irq(GPIO_BUTTON);
    pr_info("BBB: Button mapped to IRQ %d\n", irq_number);

    // Request IRQ
    ret = request_irq(irq_number,
                      button_isr,
                      IRQF_TRIGGER_FALLING,
                      "bbb_button_irq",
                      NULL);

    if (ret) {
        pr_err("BBB: Cannot request IRQ\n");
        return ret;
    }

    return 0;
}

static void __exit my_exit(void)
{
    free_irq(irq_number, NULL);
    gpio_free(GPIO_BUTTON);
    gpio_set_value(GPIO_LED, 0);
    gpio_free(GPIO_LED);

    pr_info("BBB: Exit button-irq-led module\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("You");
MODULE_DESCRIPTION("Button interrupt toggles LED on BeagleBone Black");

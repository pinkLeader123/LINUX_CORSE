#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(.gnu.linkonce.this_module) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section(__versions) = {
	{ 0x63021283, "module_layout" },
	{ 0x7d3e0d60, "misc_deregister" },
	{ 0xedc03953, "iounmap" },
	{ 0xfe990052, "gpio_free" },
	{ 0xc1514a3b, "free_irq" },
	{ 0x8c03d20c, "destroy_workqueue" },
	{ 0x8c511068, "misc_register" },
	{ 0xc6f46339, "init_timer_key" },
	{ 0x8e865d3c, "arm_delay_ops" },
	{ 0xffeedf6a, "delayed_work_timer_fn" },
	{ 0xdf9208c0, "alloc_workqueue" },
	{ 0x92d5838e, "request_threaded_irq" },
	{ 0x60fed729, "gpiod_to_irq" },
	{ 0x1f143876, "gpio_to_desc" },
	{ 0x47229b5c, "gpio_request" },
	{ 0xe97c4103, "ioremap" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0xdecd0b29, "__stack_chk_fail" },
	{ 0x51a910c0, "arm_copy_to_user" },
	{ 0xae353d77, "arm_copy_from_user" },
	{ 0x822137e2, "arm_heavy_mb" },
	{ 0xcf86cdac, "queue_delayed_work_on" },
	{ 0xc5850110, "printk" },
	{ 0xb1ad28e0, "__gnu_mcount_nc" },
};

MODULE_INFO(depends, "");


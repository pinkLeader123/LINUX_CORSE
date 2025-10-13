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
	{ 0x98c1e16f, "platform_driver_unregister" },
	{ 0x9d74ee2b, "__platform_driver_register" },
	{ 0xc6f46339, "init_timer_key" },
	{ 0x22b4e33a, "gpiod_set_consumer_name" },
	{ 0x4a3286ee, "devm_gpiod_get" },
	{ 0x7c609794, "devm_kmalloc" },
	{ 0xc38c83b8, "mod_timer" },
	{ 0x526c3a6c, "jiffies" },
	{ 0xa87b8fec, "gpiod_set_value" },
	{ 0x82ee90dc, "timer_delete_sync" },
	{ 0xc5850110, "printk" },
	{ 0xb1ad28e0, "__gnu_mcount_nc" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Cmyvendor,p8-08-blink");
MODULE_ALIAS("of:N*T*Cmyvendor,p8-08-blinkC*");

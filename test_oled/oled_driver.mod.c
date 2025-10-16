#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
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
__used __section("__versions") = {
	{ 0x650b69b6, "module_layout" },
	{ 0x4b20adad, "i2c_del_driver" },
	{ 0x1337c472, "i2c_register_driver" },
	{ 0xf2a7370b, "_dev_warn" },
	{ 0x61856fda, "wake_up_process" },
	{ 0x3a0423, "kthread_create_on_node" },
	{ 0x92997ed8, "_printk" },
	{ 0xc358aaf8, "snprintf" },
	{ 0xb3f7646e, "kthread_should_stop" },
	{ 0x2e5810c6, "__aeabi_unwind_cpp_pr1" },
	{ 0x37a0cba, "kfree" },
	{ 0x9d669763, "memcpy" },
	{ 0x2d6fcc06, "__kmalloc" },
	{ 0xf3ca5fd0, "kthread_stop" },
	{ 0x54a74d8e, "of_device_is_compatible" },
	{ 0xe1e35a4e, "_dev_info" },
	{ 0x5f754e5a, "memset" },
	{ 0xf9a482f9, "msleep" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0xe7659ada, "_dev_err" },
	{ 0x3ea1b6e4, "__stack_chk_fail" },
	{ 0x360eb9f8, "i2c_transfer_buffer_flags" },
	{ 0xb1ad28e0, "__gnu_mcount_nc" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("i2c:ssd1306");
MODULE_ALIAS("i2c:ds1307");
MODULE_ALIAS("of:N*T*Csolomon,ssd1306");
MODULE_ALIAS("of:N*T*Csolomon,ssd1306C*");
MODULE_ALIAS("of:N*T*Cdallas,ds1307");
MODULE_ALIAS("of:N*T*Cdallas,ds1307C*");

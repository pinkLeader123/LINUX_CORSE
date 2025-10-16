# Tên module
obj-m += oled_driver.o

# Đường dẫn kernel source trên host (cross-compile)
KDIR ?= /home/phuc/BBB/beagle_bone_black/bb-kernel/KERNEL

# Toolchain (ví dụ dùng Linaro ARM cross toolchain)
CROSS_COMPILE ?= arm-linux-gnueabihf-
ARCH ?= arm

# Mặc định: build
all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

# Dọn dẹp
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

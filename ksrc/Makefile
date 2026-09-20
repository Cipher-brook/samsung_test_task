# The makefile builds kernel module

# Comment/uncomment the following line to enable/disable debugging
# DEBUG := y

BINARY := stt_cdev
PWD := $(shell pwd)
INCLUDE_DIR := -I$(src)/../common
KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
CFLAGS := -Wall -Werror -fstack-protector $(INCLUDE_DIR)

ifeq ($(DEBUG), y)
	C_FLAGS := -O -DDEBUG $(CFLAGS)
else
	C_FLAGS := -O2 $(CFLAGS)
endif

obj-m += $(BINARY).o
ccflags-y += $(C_FLAGS)

.PHONY: all
all: module

.PHONY: module
module:
	make -C $(KERNEL_DIR) M=$(PWD) modules

.PHONY: clean
clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean

.PHONY: install_kmod
install_kmod:
	insmod $(BINARY).ko

.PHONY: uninstall_kmod
uninstall_kmod:
	rmmod $(BINARY)

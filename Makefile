KPATH := /usr/src/linux-headers-`uname -r`

obj-m := ./src/panel-himax-hx8399c-fhdplus.o

.PHONY: all clean doc

all:
	make -C $(KPATH) M=$(CURDIR) modules
	xz -9 -k -f src/panel-himax-hx8399c-fhdplus.ko
clean:
	make -C $(KPATH) M=$(CURDIR) clean
	rm -rf ./doc

modules_install:
	make -C $(KPATH) M=$(CURDIR) modules_install
	depmod -A
doc:
	doxygen ./Doxyfile

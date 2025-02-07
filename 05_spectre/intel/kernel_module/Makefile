obj-m += fetch_probe_module.o
ccflags-y += -Wno-unused-result

OBJECT_FILES_NON_STANDARD := y

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

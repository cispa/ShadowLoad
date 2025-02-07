
all:
	gcc ${ccflags-y} -O3 -o shadowload gadget.S shadowload.c
	gcc ${ccflags-y} -DKERNEL_MODULE=1 -O3 -o shadowload_kernel gadget.S shadowload.c

clean:
	rm -f shadowload shadowload_kernel

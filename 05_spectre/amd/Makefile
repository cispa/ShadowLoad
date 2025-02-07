all:
	gcc -O3 -I kernel_module -o sidechannel sidechannel.c gadget.S -Wl,-z,noexecstack

clean:
	rm -f sidechannel

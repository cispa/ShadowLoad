all:
	gcc -O3 -o fetchprobe_cf fetchprobe_cf.c gadget.S -Wl,-z,noexecstack
	gcc -O3 -o fetchprobe_off fetchprobe_off.c gadget.S -Wl,-z,noexecstack

clean:
	rm -f fetchprobe_cf fetchprobe_off

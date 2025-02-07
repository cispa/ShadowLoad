all:
	gcc -O3 -o sidechannel_base64 sidechannel_base64.c gadget.S -Wl,-z,noexecstack
	chmod +x run_multiple.sh

clean:
	rm -f sidechannel_base64 calc_ambigious_count

import param_utils
import plot_utils
import run_utils
import sys

PAGE_SIZE = 4096
CACHE_LINE_SIZE = 64

if len(sys.argv) != 4:
    print(f"usage: python3 {sys.argv[0]} <CORES> <TIMER> <VICTIM>")

CORES = sys.argv[1]
TIMER = sys.argv[2]
VICTIM = sys.argv[3]

def comp(TIMER, VICTIM, FLAGS):
    global CORES
    run_utils.comp("test_prefetch_both_collisions", TIMER, VICTIM, FLAGS, CORES)
    
def run(stride, accesses, start_offset, access_offset, measure_offset, colliding_buffer_addr_and, colliding_buffer_addr_xor, colliding_load_addr_and, colliding_load_addr_xor, tests, cores=CORES):
    arguments = [str(stride), str(accesses), str(start_offset), str(access_offset), str(measure_offset), str(colliding_buffer_addr_and), str(colliding_buffer_addr_xor), str(colliding_load_addr_and), str(colliding_load_addr_xor), str(tests)]
    return run_utils.run("test_prefetch_both_collisions", arguments, cores=cores)

def test(prefix, TIMER, VICTIM, FLAGS, strides, diff_bits_mem, diff_bits_pc, accesses, repeats, tests, aligned, buffer_addr, load_addr, save=True):
    global PAGE_SIZE, CACHE_LINE_SIZE, CORES
    comp(TIMER, VICTIM, FLAGS)
    
    # if iterable is passed, we want to use it multiple times
    strides = list(strides)
    diff_bits_mem = list(diff_bits_mem)
    diff_bits_pc = list(diff_bits_pc)
    
    data = []
    
    for stride in strides:
        data_row = []
        for diff_bit_pc in diff_bits_pc:
            for diff_bit_mem in diff_bits_mem:
                res = 0
                for i in range(repeats):
                    if aligned:
                        r = run(stride, accesses, 0, stride * accesses, stride * (accesses + 1), "0x7fffffffffff", f"0x{(1 << diff_bit_mem):016x}", "0x7fffffffffff", f"0x{(1 << diff_bit_pc):016x}", tests, cores=CORES)
                    else:
                        r = run(stride, accesses, 2 * stride, 0, stride, "0x7fffffffffff", f"0x{(1 << diff_bit_mem):016x}", "0x7fffffffffff", f"0x{(1 << diff_bit_pc):016x}", tests, cores=CORES)
                    if len(r.results):
                        res += int(r.results[0])
                    else:
                        res = -1
                data_row.append(res)
        data.append(data_row)
    
    if save:
        name = f"out/test_prefetch_both_collisions_{prefix}_{'aligned' if aligned else 'unaligned'}_{accesses}_{','.join([TIMER, VICTIM] + FLAGS)}"
        x_ticks = [(a,b) for b in diff_bits_pc for a in diff_bits_mem]
        y_ticks = strides
    
        plot_utils.try_heatmap(
            name,
            "",
            "different bit (mem,pc)",
            "stride",
            x_ticks,
            y_ticks,
            data
        )
    
        with open(f"{name}.py", "w") as out:
            out.write(f"repeats = {repeats}\n")
            out.write(f"x_ticks = {x_ticks}\n")
            out.write(f"y_ticks = {y_ticks}\n")
            out.write(f"data = {data}\n")
    
    return data

repeats = 2
VICTIM_LOAD_ADDR = 0xcafebabe123
VICTIM_BUFFER_ADDR = 0xaabeef000
BASE_FLAGS = ["-DEVAL"]

BITS = 47

# our max stride is 2048 and we do max. 4 accesses. So 16KB (to also work with Apple's 16KB pages) should be enough for victim buffer
if VICTIM == "userspace":
    BASE_FLAGS.append("-DVICTIM_BUFFER_SIZE=0x4000")

# aligned test
for F_FENCE in [[], ["-DUSE_FENCE"]]:
    for F_ACCESS_MEMORY in [[], ["-DACCESS_MEMORY"]]:
        for accesses in range(1, 5):
            print(accesses,"/",5)
            # aligned
            test("mem", TIMER, VICTIM, BASE_FLAGS + F_FENCE + F_ACCESS_MEMORY, [512, 768, 1024, 2048], list(range(0, BITS)), [BITS - 1], accesses, repeats, 20, True, VICTIM_BUFFER_ADDR, VICTIM_LOAD_ADDR, save=True)
            test("pc", TIMER, VICTIM, BASE_FLAGS + F_FENCE + F_ACCESS_MEMORY, [512, 768, 1024, 2048], [BITS - 1], list(range(0, BITS)), accesses, repeats, 20, True, VICTIM_BUFFER_ADDR, VICTIM_LOAD_ADDR, save=True)
            # unaligned
            test("mem", TIMER, VICTIM, BASE_FLAGS + F_FENCE + F_ACCESS_MEMORY, [512, 768, 1024, 2048], list(range(0, BITS)), [BITS - 1], accesses, repeats, 20, False, VICTIM_BUFFER_ADDR, VICTIM_LOAD_ADDR, save=True)
            test("pc", TIMER, VICTIM, BASE_FLAGS + F_FENCE + F_ACCESS_MEMORY, [512, 768, 1024, 2048], [BITS - 1], list(range(0, BITS)), accesses, repeats, 20, False, VICTIM_BUFFER_ADDR, VICTIM_LOAD_ADDR, save=True)


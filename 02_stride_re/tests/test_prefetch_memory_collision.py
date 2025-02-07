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
    run_utils.comp("test_prefetch_memory_collision", TIMER, VICTIM, FLAGS, CORES)
    
def run(stride, accesses, start_offset, access_offset, measure_offset, colliding_buffer_addr_and, colliding_buffer_addr_xor, tests, cores=CORES):
    arguments = [str(stride), str(accesses), str(start_offset), str(access_offset), str(measure_offset), str(colliding_buffer_addr_and), str(colliding_buffer_addr_xor), str(tests)]
    return run_utils.run("test_prefetch_memory_collision", arguments, cores=cores)

def test(TIMER, VICTIM, FLAGS, strides, diff_bits, accesses, repeats, tests, buffer_addr, save=True):
    global PAGE_SIZE, CACHE_LINE_SIZE, CORES
    comp(TIMER, VICTIM, FLAGS)
    
    # if iterable is passed, we want to use it multiple times
    strides = list(strides)
    diff_bits = list(diff_bits)
    
    data = []
    
    for stride in strides:
        data_row = []
        for diff_bit in diff_bits:
            res = 0
            for i in range(repeats):
                r = run(stride, accesses, 0, stride * accesses, stride * (accesses + 1), "0x7fffffffffff", f"0x{(1 << diff_bit):016x}", tests, cores=CORES)
                if len(r.results):
                    try:
                        res += int(r.results[0])
                    except:
                        print(f"run({stride}, {accesses}, 0, {stride * accesses}, {stride * (accesses + 1)}, '0x7fffffffffff', 1 << {diff_bit}, {tests}, cores={CORES}) failed: {r.results}")
                elif res == 0:
                    res = -1
            data_row.append(res)
        data.append(data_row)
    
    
    if save:
        name = f"out/test_prefetch_memory_collision_{accesses}_{','.join([TIMER, VICTIM] + FLAGS)}"
        x_ticks = diff_bits
        y_ticks = strides
        
        plot_utils.try_heatmap(
            name,
            "",
            "different bit",
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

repeats = 3
VICTIM_BUFFER_ADDR = 0xaabeef000
BASE_FLAGS = ["-DEVAL"]

# our max stride is 2048 and we do max. 4 accesses. So 16KB (to also work with Apple's 16KB pages) should be enough for victim buffer
if VICTIM == "userspace":
    BASE_FLAGS.append("-DVICTIM_BUFFER_SIZE=0x4000")

for F_FENCE in [[], ["-DUSE_FENCE"]]:
    for F_ACCESS_MEMORY in [[], ["-DACCESS_MEMORY"]]:
        for accesses in range(1, 5):
            test(TIMER, VICTIM, BASE_FLAGS + F_FENCE + F_ACCESS_MEMORY, [512, 768, 1024, 2048], list(range(0, 48)), accesses, repeats, 20, VICTIM_BUFFER_ADDR, save=True)



import param_utils
import plot_utils
import run_utils
import sys
import statistics
import time

PAGE_SIZE = 4096
CACHE_LINE_SIZE = 64

if len(sys.argv) != 4:
    print(f"usage: python3 {sys.argv[0]} <CORES> <TIMER> <VICTIM>")

CORES = sys.argv[1]
TIMER = sys.argv[2]
VICTIM = sys.argv[3]

def comp(TIMER, VICTIM, FLAGS):
    global CORES
    run_utils.comp("test_shadow_load", TIMER, VICTIM, FLAGS, CORES)
    
def run(stride, accesses, aligned, colliding_buffer_addr_and, colliding_buffer_addr_xor, colliding_load_addr_and, colliding_load_addr_xor, flush_all, tests, cores=CORES):
    arguments = [str(stride), str(accesses), str(aligned), str(colliding_buffer_addr_and), str(colliding_buffer_addr_xor), str(colliding_load_addr_and), str(colliding_load_addr_xor), str(flush_all), str(tests)]
    return run_utils.run("test_shadow_load", arguments, cores=cores)

def get_res(data):
    res_hits = [r.split("hits: ")[1].split("\n")[0] for r in data.results if "hits: " in r]
    res_setup_time = [r.split("setup_time: ")[1].split("\n")[0] for r in data.results if "setup_time: " in r]
    res_prefetch_time = [r.split("prefetch_time: ")[1].split("\n")[0] for r in data.results if "prefetch_time: " in r]
    res_gadget_time = [r.split("gadget_time: ")[1].split("\n")[0] for r in data.results if "gadget_time: " in r]
    
    hits = int(res_hits[0]) if len(res_hits) else -1
    setup_time = int(res_setup_time[0]) if len(res_setup_time) else -1 
    prefetch_time = int(res_prefetch_time[0]) if len(res_prefetch_time) else -1
    gadget_time = int(res_gadget_time[0]) if len(res_gadget_time) else -1
    
    return hits, setup_time, prefetch_time, gadget_time

def test(TIMER, VICTIM, FLAGS, stride, diff_bit_mem, diff_bit_pc, accesses, repeats, tests, save=True):
    global PAGE_SIZE, CACHE_LINE_SIZE, CORES
    comp(TIMER, VICTIM, FLAGS)
    
    data = []
    
    aligned_cached = [
        get_res(run(stride, accesses, 1, "0x7fffffffffff", f"0x{(1 << diff_bit_mem):016x}", "0x7fffffffffff", f"0x{(1 << diff_bit_pc):016x}", 0, tests)) for i in range(repeats)
    ]
    aligned_uncached = [
        get_res(run(stride, accesses, 1, "0x7fffffffffff", f"0x{(1 << diff_bit_mem):016x}", "0x7fffffffffff", f"0x{(1 << diff_bit_pc):016x}", 1, tests)) for i in range(repeats)
    ]
    unaligned_cached = [
        get_res(run(stride, accesses, 0, "0x7fffffffffff", f"0x{(1 << diff_bit_mem):016x}", "0x7fffffffffff", f"0x{(1 << diff_bit_pc):016x}", 0, tests)) for i in range(repeats)
    ]
    unaligned_uncached = [
        get_res(run(stride, accesses, 0, "0x7fffffffffff", f"0x{(1 << diff_bit_mem):016x}", "0x7fffffffffff", f"0x{(1 << diff_bit_pc):016x}", 1, tests)) for i in range(repeats)
    ]
    data = [
        [statistics.mean(map(lambda x: x[0], aligned_cached)), statistics.mean(map(lambda x: x[0], aligned_uncached))],
        [statistics.mean(map(lambda x: x[0], unaligned_cached)), statistics.mean(map(lambda x: x[0], unaligned_uncached))]
    ]
    
    print(f"{accesses} {FLAGS}")
    print(f" AC: {statistics.mean(map(lambda x: x[0], aligned_cached))}, {statistics.mean(map(lambda x: x[1], aligned_cached))}, {statistics.mean(map(lambda x: x[2], aligned_cached))}, {statistics.mean(map(lambda x: x[3], aligned_cached))}")
    print(f" AU: {statistics.mean(map(lambda x: x[0], aligned_uncached))}, {statistics.mean(map(lambda x: x[1], aligned_uncached))}, {statistics.mean(map(lambda x: x[2], aligned_uncached))}, {statistics.mean(map(lambda x: x[3], aligned_uncached))}")
    print(f" UC: {statistics.mean(map(lambda x: x[0], unaligned_cached))}, {statistics.mean(map(lambda x: x[1], unaligned_cached))}, {statistics.mean(map(lambda x: x[2], unaligned_cached))}, {statistics.mean(map(lambda x: x[3], unaligned_cached))}")
    print(f" UU: {statistics.mean(map(lambda x: x[0], unaligned_uncached))}, {statistics.mean(map(lambda x: x[1], unaligned_uncached))}, {statistics.mean(map(lambda x: x[2], unaligned_uncached))}, {statistics.mean(map(lambda x: x[3], unaligned_uncached))}")
    print("")
    
    if save:
        name = f"out/test_shadow_load_{accesses}_{','.join([TIMER, VICTIM] + FLAGS)}"
        x_ticks = ["cached", "uncached"] 
        y_ticks = ["aligned", "unaligned"]
    
        plot_utils.try_heatmap(
            name,
            "",
            "",
            "",
            x_ticks,
            y_ticks,
            data
        )
    
        with open(f"{name}.py", "w") as out:
            out.write(f"aligned_cached = {aligned_cached}\n")
            out.write(f"aligned_uncached = {aligned_uncached}\n")
            out.write(f"unaligned_cached = {unaligned_cached}\n")
            out.write(f"unaligned_uncached = {unaligned_uncached}\n")
    
    return data

repeats = 1000
tests = 100
stride = 512

BASE_FLAGS = ["-DEVAL"]

# our max stride is 512 and we do max. 3 accesses. So 16KB (to also work with Apple's 16KB pages) should be enough for victim buffer
if VICTIM == "userspace":
    BASE_FLAGS.append("-DVICTIM_BUFFER_SIZE=0x4000")



test(TIMER, VICTIM, BASE_FLAGS, stride, 46, 46, 2, repeats, tests, save=True)
test(TIMER, VICTIM, BASE_FLAGS, stride, 46, 46, 3, repeats, tests, save=True)
test(TIMER, VICTIM, BASE_FLAGS, stride, 46, 46, 4, repeats, tests, save=True)

"""
for accesses in [1]:
    for F_FENCE in [[], ["-DUSE_FENCE"]]:
        for F_ACCESS_MEMORY in [[], ["-DACCESS_MEMORY"]]:
            test(TIMER, VICTIM, BASE_FLAGS + F_FENCE + F_ACCESS_MEMORY, stride, 46, 46, accesses, repeats, tests, save=True)

"""

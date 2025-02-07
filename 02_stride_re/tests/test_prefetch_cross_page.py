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
    run_utils.comp("test_prefetch_simple", TIMER, VICTIM, FLAGS, CORES)
    
def run(stride, accesses, start_offset, access_offset, measure_offset, cores=CORES):
    arguments = [str(stride), str(accesses), str(start_offset), str(access_offset), str(measure_offset)]
    return run_utils.run("test_prefetch_simple", arguments, cores=cores)

def test(TIMER, VICTIM, FLAGS, max_stride, max_accesses, repeats, aligned):
    global PAGE_SIZE, CACHE_LINE_SIZE, CORES
    comp(TIMER, VICTIM, FLAGS)
    
    results = dict()
    
    data = []
    
    for stride in range(64, max_stride + 1, 64):
        data_row = []
        for accesses in range(1, max_accesses + 1):
            res = 0
            for i in range(repeats):
                if aligned:
                    start_offset = 0
                    # make sure trigger access is on the last cache line of page
                    while ((start_offset + stride * accesses) % PAGE_SIZE) // CACHE_LINE_SIZE != (PAGE_SIZE // CACHE_LINE_SIZE) - 1:
                        start_offset += CACHE_LINE_SIZE
                    r = run(stride, accesses, start_offset, start_offset + stride * accesses, start_offset + stride * (accesses + 1), cores=CORES)
                else:
                    # make sure trigger offset is on last cache line of the page
                    start_offset = (PAGE_SIZE // CACHE_LINE_SIZE - 1) * CACHE_LINE_SIZE
                    r = run(stride, accesses, start_offset + 2 * stride, start_offset, start_offset + stride, cores=CORES)
                if len(r.results):
                    res += int(r.results[0])
                else:
                    res = -1
            data_row.append(res)
        data.append(data_row)
    
    name = f"out/test_prefetch_cross_page_{'aligned' if aligned else 'unaligned'}_{','.join([TIMER, VICTIM] + FLAGS)}"
    x_ticks = list(range(1, max_accesses + 1))
    y_ticks = list(range(64, max_stride + 1, 64))
    
    plot_utils.try_heatmap(
        name,
        "",
        "accesses",
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
    
    return x_ticks, y_ticks, data


repeats = 5

def find_stride_prefetching(x_ticks, y_ticks, data):

    res = []
    for s, stride in enumerate(y_ticks):
        if any(map(lambda x: x > repeats * 100 / 4, data[s])):
            res.append(stride)
    return res

BASE_FLAGS = ["-DEVAL"]


# our max stride is 4096 and we do max. 4 accesses. So 32KB (to also work with Apple's 16KB pages) should be enough for victim buffer
if VICTIM == "userspace":
    BASE_FLAGS.append("-DVICTIM_BUFFER_SIZE=0x8000")

# aligned
for F_FENCE in [[], ["-DUSE_FENCE"]]:
    for F_ACCESS_MEMORY in [[], ["-DACCESS_MEMORY"]]:
        x, y, d = test(TIMER, VICTIM, BASE_FLAGS + F_FENCE + F_ACCESS_MEMORY, 4096, 4, repeats, True)
        strides_result = find_stride_prefetching(x, y, d)
        name = f"out/cross_page_aligned_{','.join([TIMER, VICTIM] + BASE_FLAGS + F_FENCE + F_ACCESS_MEMORY)}"
        out = open(f"{name}_res.py", "w")
        out.write(f"stride_prefetching = {strides_result}\n")
        out.close()

# unalignedI
for F_FENCE in [[], ["-DUSE_FENCE"]]:
    for F_ACCESS_MEMORY in [[], ["-DACCESS_MEMORY"]]:
        x, y, d = test(TIMER, VICTIM, BASE_FLAGS + F_FENCE + F_ACCESS_MEMORY, 4096, 4, repeats, False)
        strides_result = find_stride_prefetching(x, y, d)
        name = f"out/cross_page_unaligned_{','.join([TIMER, VICTIM] + BASE_FLAGS + F_FENCE + F_ACCESS_MEMORY)}"
        out = open(f"{name}_res.py", "w")
        out.write(f"stride_prefetching = {strides_result}\n")
        out.close()


import plot_utils
import run_utils
import sys

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
    global CORES
    
    comp(TIMER, VICTIM, FLAGS)
    
    results = dict()
    
    data = []
    
    for stride in range(64, max_stride + 1, 64):
        data_row = []
        for accesses in range(1, max_accesses + 1):
            res = 0
            for i in range(repeats):
                if aligned:
                    r = run(stride, accesses, 0, stride * accesses, stride * (accesses + 1), cores=CORES)
                else:
                    r = run(stride, accesses, 2 * stride, 0, stride, cores=CORES)
                if len(r.results):
                    res += int(r.results[0])
                else:
                    res = -1
            data_row.append(res)    
        data.append(data_row)
    
    name = f"out/test_prefetch_simple_{'aligned' if aligned else 'unaligned'}_{','.join([TIMER, VICTIM] + FLAGS)}"
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
BASE_FLAGS = ["-DEVAL"]

def find_access_count(x_ticks, y_ticks, data):
    results = dict()
    for s, stride in enumerate(y_ticks):
        best_accesses = -1
        best_inc = 0
        last = 0
        for a, accesses in enumerate(data[s]):
            cur = accesses
            
            if cur - last > best_inc:
                best_inc = cur - last
                best_accesses = x_ticks[a]
            
            last = max([cur, last])
        results[stride] = (best_accesses, best_inc)
    return results

def find_min_max_stride(x_ticks, y_ticks, data):

    min_stride = 99999
    max_stride = -1
    for s, stride in enumerate(y_ticks):
        if any(map(lambda x: x > repeats * 100 / 4, data[s])):
            min_stride = min([min_stride, stride])
            max_stride = max([max_stride, stride])
    return min_stride, max_stride

def find_best_workload(workloads):
    best_workload = []
    best_workload_nr = -1
    for workload, d in workloads:
        cur_nr = sum(map(sum, d))
        print(cur_nr)
        if cur_nr > best_workload_nr:
            best_workload_nr = cur_nr
            best_workload = workload
    return workload
    

# aligned
# workloads = []
for F_FENCE in [[], ["-DUSE_FENCE"]]:
    for F_ACCESS_MEMORY in [[], ["-DACCESS_MEMORY"]]:
        x, y, d = test(TIMER, VICTIM, BASE_FLAGS + F_FENCE + F_ACCESS_MEMORY, 16448, 4, repeats, True)
        # workloads.append((BASE_FLAGS + F_FENCE + F_ACCESS_MEMORY, d))
        access_count_res = find_access_count(x, y, d)
        strides_result = find_min_max_stride(x, y, d)
        name = f"out/stride_simple_aligned_{','.join([TIMER, VICTIM] + BASE_FLAGS + F_FENCE + F_ACCESS_MEMORY)}"
        out = open(f"{name}_res.py", "w")
        out.write(f"access_count = {access_count_res}\n")
        out.write(f"min_stride, max_stride = {strides_result}\n")
        out.close()


# unalignedI
# workloads = []
for F_FENCE in [[], ["-DUSE_FENCE"]]:
    for F_ACCESS_MEMORY in [[], ["-DACCESS_MEMORY"]]:
        x, y, d = test(TIMER, VICTIM, BASE_FLAGS + F_FENCE + F_ACCESS_MEMORY, 16448, 4, repeats, False)
        # workloads.append((BASE_FLAGS + F_FENCE + F_ACCESS_MEMORY, d))
        access_count_res = find_access_count(x, y, d)
        strides_result = find_min_max_stride(x, y, d)
        name = f"out/stride_simple_unaligned_{','.join([TIMER, VICTIM] + BASE_FLAGS + F_FENCE + F_ACCESS_MEMORY)}"
        out = open(f"{name}_res.py", "w")
        out.write(f"access_count = {access_count_res}\n")
        out.write(f"min_stride, max_stride = {strides_result}\n")
        out.close()


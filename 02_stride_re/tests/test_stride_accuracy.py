import param_utils
import plot_utils
import run_utils
import sys
import statistics

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

def test(TIMER, VICTIM, FLAGS, strides, max_accesses, repeats, aligned, delta):
    global PAGE_SIZE, CACHE_LINE_SIZE, CORES
    comp(TIMER, VICTIM, FLAGS)
    
    results = dict()
    
    data = []
    
    for stride in strides:
        data_row = []
        for accesses in range(1, max_accesses + 1):
            res = 0
            for i in range(repeats):
                if aligned:
                    start_offset = 0
                    r = run(stride, accesses, start_offset, start_offset + stride * accesses + delta, start_offset + stride * (accesses + 1), cores=CORES)
                else:
                    start_offset = 0
                    r = run(stride, accesses, start_offset + 2 * stride, start_offset + delta, start_offset + stride, cores=CORES)
                if len(r.results):
                    res += int(r.results[0])
                else:
                    res = -1
            data_row.append(res)
        data.append(data_row)
    
    name = f"out/test_prefetch_accuacy_{'aligned' if aligned else 'unaligned'}_{','.join([TIMER, VICTIM] + FLAGS)}"
    x_ticks = list(range(1, max_accesses + 1))
    y_ticks = list(strides)
    
    with open(f"{name}.py", "w") as out:
        out.write(f"repeats = {repeats}\n")
        out.write(f"x_ticks = {x_ticks}\n")
        out.write(f"y_ticks = {y_ticks}\n")
        out.write(f"data = {data}\n")
    
    return x_ticks, y_ticks, data


repeats = 3

def find_access_count(x_ticks, y_ticks, data):
    results = list()
    for s, stride in enumerate(y_ticks):
        if any(map(lambda x: x > repeats * 100 / 4, data[s])):
    
            best_accesses = -1
            best_inc = 0
            last = 0
            for a, accesses in enumerate(data[s]):
                cur = accesses
            
                if cur - last > best_inc:
                    best_inc = cur - last
                    best_accesses = x_ticks[a]
            
                last = max([cur, last])
            results.append(best_accesses)
    return results

BASE_FLAGS = ["-DEVAL"]

# our max stride is 4096 and we do max. 6 accesses. So 32KB (to also work with Apple's 16KB pages) should be enough for victim buffer
if VICTIM == "userspace":
    BASE_FLAGS.append("-DVICTIM_BUFFER_SIZE=0x8000")

# aligned
for F_FENCE in [[], ["-DUSE_FENCE"]]:
    for F_ACCESS_MEMORY in [[], ["-DACCESS_MEMORY"]]:
        acc_count = -1
        acc_count_len = -1
        for delta in range(64):
            x, y, d = test(TIMER, VICTIM, BASE_FLAGS + F_FENCE + F_ACCESS_MEMORY, [192, 256, 512, 768, 1024], 6, repeats, True, delta)
            
            if acc_count == -1:
                r = find_access_count(x, y, d)
                acc_count_len = len(r)
                try:
                    acc_count = statistics.median(r)
                except:
                    acc_count = -1
                
            r = find_access_count(x, y, d)
            try:
                cur_acc_count = statistics.median(r)
            except:
                cur_acc_count = -2
            
            if len(r) != acc_count_len or cur_acc_count != acc_count:
                break
                
                
        name = f"out/stride_accuracy_aligned_{','.join([TIMER, VICTIM] + BASE_FLAGS + F_FENCE + F_ACCESS_MEMORY)}"
        out = open(f"{name}_res.py", "w")
        out.write(f"max_delta = {delta - 1}\n")
        out.close()

# unaligned

for F_FENCE in [[], ["-DUSE_FENCE"]]:
    for F_ACCESS_MEMORY in [[], ["-DACCESS_MEMORY"]]:
        acc_count = -1
        acc_count_len = -1
        for delta in range(64):
            x, y, d = test(TIMER, VICTIM, BASE_FLAGS + F_FENCE + F_ACCESS_MEMORY, [192, 256, 512, 768, 1024], 6, repeats, False, delta)
            
            if acc_count == -1:
                r = find_access_count(x, y, d)
                acc_count_len = len(r)
                acc_count = statistics.median(r)
               
            r = find_access_count(x, y, d)
            if len(r) != acc_count_len or statistics.median(r) != acc_count:
                break
                
                
        name = f"out/stride_accuracy_unaligned_{','.join([TIMER, VICTIM] + BASE_FLAGS + F_FENCE + F_ACCESS_MEMORY)}"
        out = open(f"{name}_res.py", "w")
        out.write(f"max_delta = {delta - 1}\n")
        out.close()

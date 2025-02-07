import sys
import statistics

import matplotlib.pyplot as plt

runs = {
    "meltdown_shadowload": [],
    # "meltdown_ref": [],
    # "meltdown_ref_cached": []
}

o = open(sys.argv[1].replace(".txt", ".csv"), "w")

o.write(",".join([f"cache line {i}" for i in range(64)]) + "\n")

for line in open(sys.argv[1]):
    if not line.startswith("meltdown"):
        continue
    
    name = line.split(" ")[0]
    
    setup_time, leak_time, leak_cachelines = eval(line[len(name):])
    
    runs[name].append((setup_time, leak_time, leak_cachelines))
    
    o.write(",".join(map(str, leak_cachelines)) + "\n")
    
    

medians = dict()

for run_name, run_data in runs.items():
    
    y_min = []
    y_max = []
    y_mean = []
    y_median = []
    y_total = []
    leakage_total = 0
    runtime_total = 0
    leakage_times = []
    setup_times = []
    for i in range(64):
        cur_y = []
        for r in run_data:
            runtime_total += r[0]
            leakage_total += r[1]
            cur_y.append(r[2][i])
            setup_times.append(r[0])
            leakage_times.append(r[1])
        
        y_min.append(min(cur_y))
        y_max.append(max(cur_y))
        y_mean.append(statistics.mean(cur_y))
        y_median.append(statistics.median(cur_y))
        y_total.append(sum(cur_y))
    
    correct_bytes = sum(y_total)
    total_bytes = 4096 * len(run_data)
    
    median_leak_time = statistics.median(leakage_times)
    median_setup_time = statistics.median(setup_times)
    
    print(f"{run_name}| correct: {correct_bytes}/{total_bytes} ({100*correct_bytes/total_bytes:.5f}%)")
    print(f"leak  time: {leakage_total / len(run_data) / 64} ({median_leak_time})")
    print(f"setup time: {runtime_total / len(run_data) / 64} ({median_setup_time})")
    
    print(f"{run_name}| leakage: {4096 * 1000000000/ median_leak_time} Bytes/sec.")
    print("")
    
    x = list(range(64))
    
    plt.xlabel("page offset (cacheline)")
    plt.ylabel("average correct bytes")
    plt.scatter(x, y_mean)
    
    medians[run_name] = y_mean
    
    plt.savefig(f"{run_name}.svg")
    plt.clf()


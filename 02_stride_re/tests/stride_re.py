import sys
import os
import time

if len(sys.argv) == 4:
    CORES = sys.argv[1]
    TIMER = sys.argv[2]
    VICTIM = sys.argv[3]
else:
    CORES = "1"
    TIMER = "rdtsc"
    VICTIM = "userspace"

os.system("mkdir out")

# turn off those segfault messages
os.system("sysctl -w debug.exception-trace=0")

scripts = [
    "test_prefetch_simple",
    "test_prefetch_cross_page",
    # "test_prefetch_memory_collision",
    # "test_prefetch_pc_collision",
    # "test_prefetch_both_collisions",
    "test_stride_accuracy"
]

kernel_scripts = [
    "test_prefetch_both_collisions",
    "test_shadowload"
]

for script in scripts:
    start = time.time()
    os.system(f"python3 {script}.py {CORES} {TIMER} {VICTIM}")
    print(f" test {script} took {time.time() - start} sec.")

os.system("cd ../victim/kernel/kernel_module; make; insmod auto_tool_module.ko")
for script in kernel_scripts:
    start = time.time()
    os.system(f"python3 {script}.py {CORES} {TIMER} kernel")
    print(f" test {script} took {time.time() - start} sec.")
os.system("rmmod auto_tool_module")

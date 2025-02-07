import sys
import subprocess
import os
import statistics

if os.geteuid() != 0:
    print("please run this script as root!")
    quit()

if len(sys.argv) == 3:
    core   = sys.argv[1]
    target = sys.argv[2]
else:
    core = "1"
    target = "intel"

repeats = 1000

if target not in ["intel", "amd"]:
    print(f"Unknown target: {target}")
    quit()

os.system(f"cd {target}; make")

os.system(f"cd {target}/kernel_module; make; insmod fetch_probe_module.ko")

def run():
    global core, target
    res = subprocess.check_output(["taskset", "-c", core, f"./sidechannel"], cwd=target).decode()
    return int(res.split("time: ")[1].split("\n")[0]), int(res.split("correct: ")[1].split("\n")[0])
    
if True:
    runtimes = []
    corrects = []
    
    for _ in range(repeats):
        runtime, correct = run()
        runtimes.append(runtime)
        corrects.append(correct)
    
    runtimes.sort()
    corrects.sort()
    
    print(f"rate   : {4096 * 1000000000 / statistics.median(runtimes) / 1000:.1f}KB/s")
    print(f"correct: {statistics.median(corrects) / 4096 * 100:.1f}%")
    print("--------------------")
    
    # uncomment to print the runtime and correct amount of bytes for each individual run
    # print(f"runtime: {runtimes}")
    # print(f"correct: {corrects}")
    
os.system("rmmod fetch_probe_module")


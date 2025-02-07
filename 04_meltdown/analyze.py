import subprocess
import time
import sys
import os

def execute(cmd, core):
    return subprocess.check_output(["taskset", "-c", str(core)] + cmd)

def run(binary, core_test):
    # run binary
    output = execute([binary], core_test).decode()
    
    # shadowload setup time (if applicable)
    if "shadowload setup took " in output:
        shadowload_setup_time = int(output.split("shadowload setup took ")[1].split(" ")[0])
    else:
        shadowload_setup_time = 0
    
    # leakage time
    leakage_time = int(output.split("leaking took ")[1].split(" ")[0])
    
    # correct
    correct_bytes = []
    cur_cacheline = 0
    while f"correct for cache_line {cur_cacheline}:" in output:
        correct_bytes.append(int(output.split(f"correct for cache_line {cur_cacheline}: ")[1].split("\n")[0]))
        cur_cacheline += 1
    
    return shadowload_setup_time, leakage_time, correct_bytes
    
def setup():
    # compile
    os.system("make")
    
    # compile kernel module
    os.system("cd kernel_module; make")
    
    # insert kernel module
    os.system("cd kernel_module; insmod meltdown_module.ko")

def cleanup():
    # remove kernel module
    os.system("rmmod meltdown_module")
    
if __name__ == "__main__":
    
    # enable huge pages
    os.system("sysctl -w vm.nr_hugepages=50")

    setup()
    
    # name binary, core, not bring to cache
    configurations = [
        ("meltdown_shadowload", "./meltdown_shadowload_single", (0,-1), 1),
        # ("meltdown_ref", "./meltdown_ref", (0,-1), 1),
        # ("meltdown_ref_cached", "./meltdown_ref", (1,5), 0)
    ]
    
    try:
        # measure 10000 times
        for i in range(10000):
            print(f"--- Test {i} ---")
            for name, binary, cores, load in configurations:
                core_test, core_loader = cores
                if core_loader != -1:
                    loader = subprocess.Popen(["taskset", "-c", str(core_loader), "./meltdown_loader", str(load)])
                time.sleep(0.1)
                print(name, run(binary, core_test))
                sys.stdout.flush()
                if core_loader != -1:
                    loader.kill()
                    time.sleep(0.1)
                
    except KeyboardInterrupt:
        pass
    cleanup()


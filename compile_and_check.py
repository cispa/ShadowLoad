import os
import sys

if os.geteuid() != 0:
    print("please run this script as root!")
    quit()

try:
    import matplotlib.pyplot as plt
    print("Matplotlib seems ok!")
except:
    print("Matplotlib is not installed!")


def compile_and_check(directory, files):
    os.system(f"cd {directory};AUTO_TOOL_FLAGS=-DEVAL make >/dev/null")
    
    for f in files:
        if not os.path.exists(os.path.join(directory, f)):
            print(f"directory {directory} is missing {f}!", file=sys.stderr)
            quit()
    
    print(f"directory {directory} seems ok!")


# Simple FetchProbe
compile_and_check("00_fetch_probe/intel", ["fetchprobe_cf", "fetchprobe_off"])
compile_and_check("00_fetch_probe/intel/kernel_module", ["fetchprobe_module.ko"])
compile_and_check("00_fetch_probe/amd", ["fetchprobe_cf", "fetchprobe_off"])
compile_and_check("00_fetch_probe/amd/kernel_module", ["fetchprobe_module.ko"])

# Simple Shadowload
compile_and_check("01_shadowload", ["shadowload", "shadowload_kernel"])
compile_and_check("01_shadowload/kernel_module", ["shadowload_module.ko"])

# StrideRE
compile_and_check("02_stride_re", ["tests/test_prefetch_simple", "tests/test_prefetch_memory_collision", "tests/test_prefetch_pc_collision", "tests/test_prefetch_both_collisions", "tests/test_shadow_load"])

# Base64
compile_and_check("03_base64", ["sidechannel_base64"])

# Meltdown
compile_and_check("04_meltdown", ["meltdown_shadowload_single"])
compile_and_check("04_meltdown/kernel_module", ["meltdown_module.ko"])

# Spectre
compile_and_check("05_spectre/intel", ["sidechannel"])
compile_and_check("05_spectre/intel/kernel_module", ["fetch_probe_module.ko"])
compile_and_check("05_spectre/amd", ["sidechannel"])
compile_and_check("05_spectre/amd/kernel_module", ["fetch_probe_module.ko"])

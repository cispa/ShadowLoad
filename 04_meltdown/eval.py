import os

if os.geteuid() != 0:
    print("please run this script as root!")
    quit()
    
cmdline = open("/proc/cmdline").read()
    
if "nopti" not in cmdline and "mitigations=off" not in cmdline:
    print("Please change your kernel command-line to disable page table isolation")
    quit()


os.system("python3 analyze.py>out.txt")
os.system("python3 plot.py out.txt")

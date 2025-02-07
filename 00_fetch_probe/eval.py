import os

if os.geteuid() != 0:
    print("please run this script as root!")
    quit()

platform = "intel"

print("Platform:", platform)

os.system(f"cd {platform}; python3 eval_fetchprobe_cf.py; python3 eval_fetchprobe_off.py")

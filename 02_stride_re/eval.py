import os

if os.geteuid() != 0:
    print("please run this script as root!")
    quit()


os.system("cd tests; python3 stride_re.py")

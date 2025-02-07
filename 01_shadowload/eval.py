import os
import sys
import subprocess

if os.geteuid() != 0:
    print("please run this script as root!")
    quit()

if len(sys.argv) > 1:
    core = sys.argv[1]
else:
    core = "0"

os.system("mkdir out")

def run(command, cores):
    return subprocess.check_output(["taskset", "-c", cores] + command)

def run_binary(binary, cores):
    output = run(["./" + binary], cores).decode()
    results = []
    for line in output.split("\n"):
        if not line:
            continue
        # accesses, stride, aligned, hits
        results.append(tuple(map(int, line.split())))
    return results
    


def run_and_plot(binary, cores):
    
    results = run_binary(binary, cores)
    
    # save all results
    with open(f"out/{binary}.py", "w") as out:
        out.write(f"result_{binary} = {results}")
    
    # try to plot
    try:
        import matplotlib.pyplot as plt
        plt.rc('font', size=6) 
        for al in [0,1]:
            strides = [i * 64 for i in range(1, 33)]
            accesses = list(range(1, 9))
        
            data = []
        
            for s in strides:
                cur_data = []
                for a in accesses:
                    matching = [hits for acc, stri, alig, hits in results if acc == a and stri == s and alig == al]
                    if matching == []:
                        cur_data.append(0)
                    else:
                        cur_data.append(matching[0])
                data.append(cur_data)
            fig, ax = plt.subplots()
            im = ax.imshow(data, vmin=0, vmax=100, interpolation="nearest")
            
            ax.set_xticks(list(range(len(accesses))))
            ax.set_xticklabels(list(map(str, accesses)))
            ax.set_yticks(list(range(len(strides))))
            ax.set_yticklabels(list(map(str, strides)))
            
            plt.setp(ax.get_xticklabels(), rotation=45, ha="right", rotation_mode="anchor")
            
            for i in range(len(strides)):
                for j in range(len(accesses)):
                    text = ax.text(j, i, data[i][j], ha="center", va="center", color="w")
            
            ax.set_title(f"Success Rate")
            ax.set_ylabel("stride")
            ax.set_xlabel("accesses")
            fig.tight_layout()
            
            plt.savefig(f"out/result_{binary}_{al}.svg")
    except:
        print("could not plot results")

run_and_plot("shadowload", core.split(",")[0])

os.system("cd kernel_module; insmod shadowload_module.ko")
run_and_plot("shadowload_kernel", core.split(",")[0])
os.system("rmmod shadowload_module")

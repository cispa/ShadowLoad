import os
import subprocess
import statistics

os.system("mkdir out")
os.system("make")
os.system("cd kernel_module; make; insmod fetchprobe_module.ko")

def run():
    res = subprocess.check_output(["taskset", "-c", "1", "./fetchprobe_off"]).decode()
    time = int(res.split("time: ")[1].split("\n")[0])
    correct = int(res.split("correct: ")[1].split("\n")[0])
    false_positives = int(res.split("false positives: ")[1].split("\n")[0])
    false_negatives = int(res.split("false negatives: ")[1].split("\n")[0])
    positives = int(res.split("positives: ")[1].split("\n")[0])
    negatives = int(res.split("negatives: ")[1].split("\n")[0])
    
    inv_correct = int(res.split("inv correct: ")[1].split("\n")[0])
    inv_false_positives = int(res.split("inv false positives: ")[1].split("\n")[0])
    inv_false_negatives = int(res.split("inv false negatives: ")[1].split("\n")[0])
    inv_positives = int(res.split("inv positives: ")[1].split("\n")[0])
    inv_negatives = int(res.split("inv negatives: ")[1].split("\n")[0])
    
    return time, correct, false_positives, false_negatives, positives, negatives, inv_correct, inv_false_positives, inv_false_negatives, inv_positives, inv_negatives
    

times = []
correct = []
false_positives = []
false_negatives = []
positives = []
negatives = []
inv_correct = []
inv_false_positives = []
inv_false_negatives = []
inv_positives = []
inv_negatives = []


for i in range(1000):
    time, c, fp, fn, p, n, ic, ifp, ifn, ip, _in = run()
    times.append(time / 1000000000)
    correct.append(c)
    false_positives.append(fp)
    false_negatives.append(fn)
    positives.append(p)
    negatives.append(n)
    
    inv_correct.append(ic)
    inv_false_positives.append(ifp)
    inv_false_negatives.append(ifn)
    inv_positives.append(ip)
    inv_negatives.append(_in)
    

with open("out/result_offset.py", "w") as out:
    out.write(f"times = {times}\n")
    out.write(f"correct = {correct}\n")
    out.write(f"false_positives = {false_positives}\n")
    out.write(f"false_negatives = {false_negatives}\n")
    out.write(f"positives = {positives}\n")
    out.write(f"negatives = {negatives}\n")
    out.write(f"inv_correct = {inv_correct}\n")
    out.write(f"inv_false_positives = {inv_false_positives}\n")
    out.write(f"inv_false_negatives = {inv_false_negatives}\n")
    out.write(f"inv_positives = {inv_positives}\n")
    out.write(f"inv_negatives = {inv_negatives}\n")

os.system("rmmod fetchprobe_module")

os.system("python3 analyze.py out/result_offset.py")


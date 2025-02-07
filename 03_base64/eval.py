import sys
import statistics
import numpy as np
import time
import os

results = []

start = time.time()
os.system("./run_multiple.sh 1000")
time = time.time() - start

for line in open("results"):
    if not line or "#" in line:
        continue
    results.append(tuple(map(int, line.split())))



incorrect = 0
invoks = []
unkns = []
for unknown, invocations in results:
    if unknown == 44:
        incorrect += 1
    else:
        unkns.append(unknown)
        invoks.append(invocations)

leakage = 256 * (1000 - incorrect) / time

print(f"leakage: {leakage:.2f} bit/sec.")


print(f"incorrect: {incorrect} / 1000 ({100 * incorrect / 1000:.2f}%)")
print(f"unknown left: {statistics.mean(unkns)}")
print(f"invokations : {statistics.mean(invoks)}")


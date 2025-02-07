import statistics
import sys

if len(sys.argv) != 2:
    print(f"usage: python3 {sys.argv[0]} <out file>")
    quit()

res = dict()

exec(open(sys.argv[1]).read(), res, res)

for k,v in res.items():
    if isinstance(v, list) and len(v) and (isinstance(v[0], int) or isinstance(v[0], float)):
        print(f"{k}: {statistics.median(v)}")

def calculate_fscore(res, pref):
    true_positives = statistics.median(res[pref + "positives"]) - statistics.median(res[pref + "false_negatives"])
    positives = statistics.median(res[pref + "positives"]) + statistics.median(res[pref + "false_positives"])
    precision = true_positives / positives
    print(pref, "precision:", f"{precision * 100:.1f}%")
    
    recall = true_positives / statistics.median(res[pref + "positives"])
    print(pref, "recall:", f"{recall * 100:.1f}%")
    
    f_score = 	2*precision*recall / (precision + recall)
    print(pref, "f-score:", f"{f_score * 100:.1f}%")
    

calculate_fscore(res, "")

if "inv_positives" in res.keys():
    calculate_fscore(res, "inv_")

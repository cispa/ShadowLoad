import os
import subprocess

class RunResult:

    def __init__(self, retval, debugs, infos, warnings, errors, fatals, results):
        self.retval = retval
        self.debugs = debugs
        self.infos = infos
        self.warnings = warnings
        self.errors = errors
        self.fatals = fatals
        self.results = results

def get_sibling_hyperthread(hyperthread):
    with open(f'/sys/devices/system/cpu/cpu{hyperthread}/topology/thread_siblings_list', 'r') as f:
        sibling_list = list(map(int, f.read().replace("-", ",").split(",")))
        sibling_list.remove(hyperthread)
        assert(len(sibling_list) == 1)
        return sibling_list[0]

def get_other_core(hyperthread):
    sibling = get_sibling_hyperthread(hyperthread)
    return max(sibling, hyperthread)+1

def comp(test, TIMER, VICTIM, FLAGS, CORES):

    victim=VICTIM
    additional_flags=[]
    if VICTIM == "hyperthread":
        additional_flags+=[f"-DTHREAD_CORE={get_sibling_hyperthread(int(CORES))}"]
    elif VICTIM == "core":
        victim="hyperthread"
        additional_flags+=[f"-DTHREAD_CORE={get_other_core(int(CORES))}"]

    os.environ["AUTO_TOOL_TIMER"] = TIMER
    os.environ["AUTO_TOOL_VICTIM"] = victim
    os.environ["AUTO_TOOL_FLAGS"] = " ".join(FLAGS+additional_flags)
    os.system(f"cd ..; make {test}")
    
def run(test, args, cores="1"):
    p = subprocess.Popen(["taskset", "-c", cores, f"./{test}"] + args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = p.communicate()
    debugs = []
    infos = []
    warnings = []
    errors = []
    fatals = []
    results = []
    
    for line in stdout.decode().split("\n"):
        if not line:
            continue
        
        if len(line) > 1 and line[0] == "R":
            data = line[2:]
        elif len(line) < 3 or ":" not in line or "]" not in line or line[0] not in ["D", "I", "!", "X", "#"]:
            continue
        else:
            filename = line[3:line.index("]")]
            fileline = int(line[line.index(":") + 1: line.index("]")]) 
            content = line[line.index("]") + 2:]
            data = (filename, fileline, content)
            
        kind = line[0]
        
        if kind == "D":
            debugs.append(data)
        elif kind == "I":
            infos.append(data)
        elif kind == "!":
            warnings.append(data)
        elif kind == "X":
            errors.append(data)
        elif kind == "#":
            fatals.append(data)
        elif kind == "R":
            results.append(data)
    
    return RunResult(p.returncode, debugs, infos, warnings, errors, fatals, results)

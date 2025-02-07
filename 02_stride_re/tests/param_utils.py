import os

def get_results_for(name):
    
    results = dict()

    for f in os.listdir("out"):
        if not f.endswith(".py"):
            continue
        
        if not f.startswith(name):
            continue
            
        params = f[len(name) + 1:-3].split(",")
        
        globs = dict()
        exec(open("out/" + f).read(), dict(), globs)
        
        
        
        results[tuple(params)] = globs
    return results

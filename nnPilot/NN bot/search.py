#!/usr/bin/env python3
import itertools
import subprocess
import shlex
import csv
from datetime import datetime
from pathlib import Path
from multiprocessing import Pool

# -------------------------------
# Settings
# -------------------------------
binary = "./MLPTrain"   # your compiled C program

# Search space
lrs      = [0.1, 0.3, 0.7, 0.75]
epochs   = [500, 1000, 1500, 2000]
decays   = [0.99, 0.992, 0.995, 0.997, 0.999, 0.9995]
max_workers = 8

# Output files
csv_path = Path("sweep_results.csv")

# -------------------------------
# Helpers
# -------------------------------
def now():
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def run_one(args):
    lr, epoch, decay = args
    # Your binary takes positional args: ./MLPTrain <lr> <epochs> <decay>
    cmd = f"{binary} {lr} {epoch} {decay}"
    print(f"[{now()}] Running: {cmd}")
    proc = subprocess.run(shlex.split(cmd),
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE,
                          text=True)
    stdout, stderr = proc.stdout, proc.stderr
    # Parse "Avg Accuracy"
    acc = None
    for line in (stdout + "\n" + stderr).splitlines():
        if "Avg Accuracy" in line:
            try:
                acc = float(line.strip().split()[-1])
            except:
                pass
    result = {
        "lr": lr,
        "epoch": epoch,
        "decay": decay,
        "status": proc.returncode,
        "acc": acc,
    }
    print(f"[{now()}] Done: {result}")
    return result

def cfg_key(lr, epoch, decay):
    # Canonicalize so floats match between CSV and your lists
    return (round(float(lr), 8), int(epoch), round(float(decay), 8))

# -------------------------------
# Main sweep
# -------------------------------
# --- Replace your "Main sweep" section with this ---
fieldnames = ["lr","epoch","decay","status","acc"]

# Load already-tested configs
seen = set()
if csv_path.exists():
    with csv_path.open("r", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                seen.add(cfg_key(row["lr"], row["epoch"], row["decay"]))
            except Exception:
                pass

# Ensure header exists
if not csv_path.exists() or csv_path.stat().st_size == 0:
    with csv_path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()

# Build combos and filter out ones we've already run
all_combos = list(itertools.product(lrs, epochs, decays))
combos = [c for c in all_combos if cfg_key(*c) not in seen]

print(f"Total combos: {len(all_combos)} | Skipping {len(all_combos)-len(combos)} previously tested | Running {len(combos)} new")

from multiprocessing import Pool

with Pool(processes=max_workers) as pool, csv_path.open("a", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=fieldnames)
    for res in pool.imap_unordered(run_one, combos):
        writer.writerow(res)
        f.flush()
        # Mark as seen so if you re-run mid-sweep it won’t redo completed ones
        seen.add(cfg_key(res["lr"], res["epoch"], res["decay"]))

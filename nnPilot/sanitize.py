import sys
import os

input_file = "replay2.txt"
output_file = "replay2_clean.txt"

with open(input_file, "r") as f, open("temp1", "w") as out:
    for line in f:
        tokens = line.strip().split()
        floats = []
        for tok in tokens:
            try:
                floats.append(float(tok))
            except ValueError:
                pass
        if len(floats) == 22:  # only keep correct lines
            out.write(line)
        else:
            print("Incorrect number of floats, line removed:", line.strip())

with open("temp1", "r") as f, open("temp2", "w") as out:
    for line in f:
        tokens = line.strip().split()
        if len(tokens) != 22:
            continue  # skip malformed lines

        floats = [float(tok) for tok in tokens]

        # last_val = floats[-1]
        # if last_val == 1.0:
        #     floats.append(0.0)
        # elif last_val == 0.0:
        #     floats.append(1.0)
        # elif last_val == 0.5:
        #     floats.append(0.5)
        # else:
        #     print(f"Unexpected last value {last_val}, skipping line")
        #     continue

        out.write(" ".join(f"{min(1.0,x):.6f}" for x in floats) + "\n")

os.system(f"awk '!seen[$0]++' temp2 > {output_file}") # remove duplicates
os.remove("temp1")
os.remove("temp2")
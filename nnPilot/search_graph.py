import matplotlib.pyplot as plt
import csv
from pathlib import Path

def cfg_key(lr, epoch, decay, acc):
    # Canonicalize so floats match between CSV and your lists
    return (round(float(lr), 8), int(epoch), round(float(decay), 8), round(float(acc), 8))

csv_path = Path("sweep_results.csv")
data = set()
with csv_path.open("r", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                data.add(cfg_key(row["lr"], row["epoch"], row["decay"], row["acc"]))
            except Exception:
                pass

data_lst = list(data)
# Define the data for the x and y axes
lr = [x[0] for x in data_lst]
epoch = [x[1] for x in data_lst]
decay = [x[2] for x in data_lst]
acc = [x[3] for x in data_lst]

fig, axes = plt.subplots(nrows=2, ncols=2)

# Plot the data on the axes
axes[0,0].scatter(lr, acc)
axes[0,0].set_title("lr - acc")
axes[0,0].set_xlabel("lr")
axes[0,0].set_ylabel("acc")

axes[1,0].scatter(epoch, acc)
axes[1,0].set_title("epoch - acc")
axes[1,0].set_xlabel("epoch")
axes[1,0].set_ylabel("acc")

axes[0,1].scatter(decay, acc)
axes[0,1].set_title("decay - acc")
axes[0,1].set_xlabel("decay")
axes[0,1].set_ylabel("acc")


# Display the plot
plt.show()
import argparse
import csv
import json
import matplotlib.pyplot as plt
import os
import pathlib
from tabulate import tabulate


_ARG_STATS = "stats"
_ARG_OUTPATH = "png"
_ARG_TABULATE = "markdown"
_ARG_CSV = "csv"


def parse_args_to_dict():
    parser = argparse.ArgumentParser(description="Plot the neck statistics file")
    parser.add_argument(
        _ARG_STATS, type=pathlib.Path, help="path to the statistics file"
    )
    parser.add_argument(
        f"--{_ARG_OUTPATH}", type=pathlib.Path, help="path to write the table as a png"
    )
    parser.add_argument(
        f"--{_ARG_TABULATE}",
        type=pathlib.Path,
        help="path to write the table as markdown",
    )
    parser.add_argument(
        f"--{_ARG_CSV}", type=pathlib.Path, help="path to write the table as csv"
    )
    return vars(parser.parse_args())


def plot(stats_data, png_path=None, tabulate_path=None, csv_path=None):
    header = [
        "Filename",
        "Using Simplified DFA",
        "Return Code",
        "Elapsed Time",
        "User Time",
        "System Time",
        "Max Resident Set (kbytes)",
    ]
    values = []
    colors = []

    stats = stats_data["statistics"]
    for stat in stats:
        config = stat["config"]

        filename = os.path.basename(config["path"])
        using_simplified_dfa = config["use-simplified-dfa"]
        returncode = stat["returncode"]

        time = stat["time"]
        elapsed_time = time["Elapsed (wall clock) time (h:mm:ss or m:ss)"]
        user_time = time["User time (seconds)"]
        sys_time = time["System time (seconds)"]
        max_res_set = time["Maximum resident set size (kbytes)"]

        values.append(
            [
                filename,
                using_simplified_dfa,
                returncode,
                elapsed_time,
                user_time,
                sys_time,
                max_res_set,
            ]
        )
        c = "w" if returncode == 0 else "#ffcccc"
        colors.append([c] * len(header))

    did_write_file = False

    tabulated_data = tabulate(values, tablefmt="pipe", headers=header)
    if tabulate_path is not None:
        did_write_file = True
        with open(tabulate_path, "w") as tf:
            tf.write(tabulated_data)

    if png_path is not None:
        did_write_file = True
        fig, ax = plt.subplots()

        # hide axes
        fig.patch.set_visible(False)
        ax.axis("off")
        ax.axis("tight")

        ax.table(cellText=values, cellColours=colors, colLabels=header, loc="center")

        plt.savefig(png_path, bbox_inches="tight", dpi=300)

    if csv_path is not None:
        did_write_file = True
        with open(csv_path, "w") as f:
            writer = csv.writer(f)
            writer.writerow(header)
            writer.writerows(values)

    if not did_write_file:
        print(tabulated_data)


if __name__ == "__main__":
    args = parse_args_to_dict()
    stats = json.load(args[_ARG_STATS].open())
    out_path = args[_ARG_OUTPATH]
    tabulate_path = args.get(_ARG_TABULATE, None)
    csv_path = args.get(_ARG_CSV, None)
    plot(stats, out_path, tabulate_path, csv_path)

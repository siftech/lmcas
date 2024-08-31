#!/usr/bin/env python3

import click
from collections.abc import Iterable
import csv
import json
from lmcas_metrics import (
    text_section_size,
    instruction_count,
    function_count,
    instruction_count_of_functions,
    rop_gadget_count,
)
from pathlib import Path
import stat
import sys
from typing import Any, Literal, Union


def binaries(test_dir: Path) -> Iterable[Path]:
    for bloated_file in test_dir.glob("result/**/bloated"):
        mode = bloated_file.stat().st_mode
        if stat.S_ISREG(mode) and stat.S_IMODE(mode) & stat.S_IXUSR != 0:
            yield bloated_file
            yield bloated_file.parent / bloated_file.parent.name


def metrics_for(binaries: Iterable[Path]) -> dict[str, dict[str, dict[str, Any]]]:
    out: dict[str, dict[str, dict[str, Any]]] = {}
    for binary in binaries:
        target_name = binary.parent.name
        debloatness = "bloated" if binary.name == "bloated" else "debloated"
        if target_name not in out:
            out[target_name] = {}
        out[target_name][debloatness] = metrics_for_one(binary)
    return out


def metrics_for_one(binary: Path) -> dict[str, Any]:
    return {
        "text_section_size": text_section_size(binary),
        "instruction_count": instruction_count(binary),
        "function_count": function_count(binary),
        "instruction_count_of_functions": instruction_count_of_functions(binary),
        "rop_gadget_count": rop_gadget_count(binary),
    }


@click.command()
@click.option(
    "-F", "--output-format", type=click.Choice(["csv", "json"]), default="csv"
)
@click.argument("test_dir", type=click.Path(file_okay=False, path_type=Path))
def main(output_format: Union[Literal["csv"], Literal["json"]], test_dir: Path) -> None:
    stats = metrics_for(binaries(test_dir))
    if output_format == "csv":
        writer = csv.writer(sys.stdout)
        writer.writerow(
            [
                "name",
                "bloated_text_section_size",
                "debloated_text_section_size",
                "bloated_instruction_count",
                "debloated_instruction_count",
                "bloated_function_count",
                "debloated_function_count",
                "bloated_rop_gadget_count",
                "debloated_rop_gadget_count",
            ]
        )
        for target_name in stats:
            writer.writerow(
                [
                    target_name,
                    stats[target_name]["bloated"]["text_section_size"],
                    stats[target_name]["debloated"]["text_section_size"],
                    stats[target_name]["bloated"]["instruction_count"],
                    stats[target_name]["debloated"]["instruction_count"],
                    stats[target_name]["bloated"]["function_count"],
                    stats[target_name]["debloated"]["function_count"],
                    stats[target_name]["bloated"]["rop_gadget_count"],
                    stats[target_name]["debloated"]["rop_gadget_count"],
                ]
            )
    elif output_format == "json":
        json.dump(stats, sys.stdout)
        print()


if __name__ == "__main__":
    main()

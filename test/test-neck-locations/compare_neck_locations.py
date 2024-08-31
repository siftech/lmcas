#!/usr/bin/env python3

from argparse import ArgumentParser
from pathlib import Path
import json
from os import getenv


def read_json(path: Path):
    with path.open() as f:
        return json.load(f)


def main() -> int:
    parser = ArgumentParser(description="Checks neck miner regression of placed necks")
    parser.add_argument(
        "target_name",
        type=str,
        help="the name of the target spec of which we will be checking the neck location",
    )
    parser.add_argument(
        "neck_json_path",
        type=Path,
        help="the path to the neck_miner_neck.json file that is generated during the debloating process",
    )
    args = parser.parse_args()

    rubrics_path = getenv("RUBRICS_PATH")
    if rubrics_path is None:
        raise Exception("Environment variable RUBRICS_PATH does not exist")
    rubrics_path = Path(rubrics_path)

    result_contents = read_json(args.neck_json_path)
    rubric_contents = read_json(rubrics_path / f"{args.target_name}.json")
    if result_contents == rubric_contents:
        print(
            f"success: neck locations for target {args.target_name} match rubric neck locations"
        )
        print("====================================================================")
        print(result_contents)
        print("==")
        print(rubric_contents)
        print("====================================================================")
        return 0
    else:
        print(
            f"failure: neck locations for target {args.target_name} do not match rubric neck locations"
        )
        print("====================================================================")
        print(result_contents)
        print("!=")
        print(rubric_contents)
        print("====================================================================")
        return 1


if __name__ == "__main__":
    exit(main())

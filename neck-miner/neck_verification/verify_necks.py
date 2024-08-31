#!/usr/bin/python
import argparse
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import time

_FIELD_MODULE_PATH = "path"
_FIELD_TAINT_CONFIG = "taint-config"
_FIELD_INFO_WO_GLOBALS = "function-local-points-to-info-wo-globals"
_FIELD_USE_SIMPLIFIED_DFA = "use-simplified-dfa"
_FIELD_TIMEOUT_DURATION = "timeout-duration"
_FIELD_TIMEOUT_KILL_AFTER = "timeout-kill-after"

_CONFIG_FIELDS = [
    _FIELD_MODULE_PATH,
    _FIELD_TAINT_CONFIG,
    _FIELD_INFO_WO_GLOBALS,
    _FIELD_USE_SIMPLIFIED_DFA,
    _FIELD_TIMEOUT_DURATION,
    _FIELD_TIMEOUT_KILL_AFTER,
]


def parse_args_to_dict():
    parser = argparse.ArgumentParser(
        description="Verify the necks defined in the config"
    )
    parser.add_argument("config", type=pathlib.Path, help="path to the config file")
    parser.add_argument(
        f"--stats", type=pathlib.Path, help="path to output the statistics file"
    )
    parser.add_argument(
        f"--write-cfg", type=pathlib.Path, help="path to output the CFG files"
    )
    return vars(parser.parse_args())


def build_neck_command(c, time_out_path, bin_path, cfg_dir=None):
    cmd = []
    cmd.append(f"/usr/bin/env time --quiet -o {time_out_path} -v")
    cmd.append(
        f"/usr/bin/env timeout --kill-after {c[_FIELD_TIMEOUT_KILL_AFTER]} {c[_FIELD_TIMEOUT_DURATION]}"
    )
    cmd.append(bin_path)
    cmd.append(f"-m {c[_FIELD_MODULE_PATH]}")
    cmd.append(f"-c {c[_FIELD_TAINT_CONFIG]}")
    cmd.append(f"--verify-neck")
    if c[_FIELD_INFO_WO_GLOBALS]:
        cmd.append(f"--function-local-points-to-info-wo-globals")
    if c[_FIELD_USE_SIMPLIFIED_DFA]:
        cmd.append(f"--use-simplified-dfa")

    if cfg_dir != None:
        filename = os.path.splitext(os.path.basename(c[_FIELD_MODULE_PATH]))[0]
        if c[_FIELD_USE_SIMPLIFIED_DFA]:
            filename += "-simplified-dfa"
        filename += ".dot"
        cmd.append(f"--write-cfg {os.path.join(cfg_dir, filename)}")

    return " ".join(cmd)


def store_module_statistics(stats, config, cmd, process_data, timing_output):
    time_data = None
    if timing_output:
        timing_start = timing_output.find("\tCommand being timed")
        timing_values = timing_output[timing_start:]
        time_results = [
            line.replace("\t", "").split(": ", 1)
            for line in timing_values.split("\n")
            if line
        ]
        time_data = {k: v for k, v in time_results}
        # cleanup specific entry
        cmd_value = time_data["Command being timed"]
        if cmd_value.startswith('"') and cmd_value.endswith('"'):
            time_data["Command being timed"] = cmd_value[1:-1]

    stats.append(
        {
            "command": cmd,
            "config": config,
            "returncode": process_data.returncode,
            "stdout": process_data.stdout,
            "stderr": process_data.stderr,
            "time": time_data,
        }
    )


def verify_neck(module_config, bin_path, stats=None, cfg_dir=None):
    with tempfile.NamedTemporaryFile(delete=True) as time_fp:
        cmd = build_neck_command(module_config, time_fp.name, bin_path, cfg_dir)
        process_data = subprocess.run(
            cmd,
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
        )
        if stats != None:
            time_fp.file.seek(0)
            timing_output = time_fp.file.read().decode("utf-8")
            store_module_statistics(
                stats, module_config, cmd, process_data, timing_output
            )
        return process_data


def get_module_configs(config):
    modules = config["modules"]
    module_defaults = config["module-defaults"]

    def module_config_or_default(module, field):
        if field in module:
            return module[field]
        if field in module_defaults:
            return module_defaults[field]
        raise Exception(
            f"Found unassigned field '{field}' that does not have a default value."
        )

    return [
        {field: module_config_or_default(module, field) for field in _CONFIG_FIELDS}
        for module in modules
    ]


def main(config, stats_path=None, cfg_dir=None):
    stats = []

    assert cfg_dir != None, "cfg_dir was None"

    bin_path = "build/tools/neck-miner/neck-miner-test"
    shutil.copy2("build/tools/neck-miner/neck-miner", bin_path)
    print(f"Copying bin_path to: {bin_path}")

    for module_config in get_module_configs(config):
        verify_neck(module_config, bin_path, stats, cfg_dir)
    if stats_path != None:
        with stats_path.open("w") as f:
            json.dump({"timestamp": time.time(), "statistics": stats}, f, indent=4)


if __name__ == "__main__":
    args = parse_args_to_dict()
    stats_path = args.get("stats")
    cfg_dir = args.get("write_cfg")
    config = json.load(args["config"].open())
    main(config, stats_path, cfg_dir)

import json
import os
import pytest
import shutil

from .verify_necks import get_module_configs, verify_neck

with open("./neck_verification/verify_necks_config.json") as f:
    config = json.load(f)
    TEST_DATA = get_module_configs(config)


@pytest.mark.parametrize("module_config", TEST_DATA)
def test_verify_necks(module_config):
    bin_path = "build/tools/neck-miner/neck-miner-test"
    shutil.copy2("build/tools/neck-miner/neck-miner", bin_path)

    process_data = verify_neck(module_config, bin_path)
    assert (
        process_data.returncode == 0
    ), "Return code was not 0\nSTDOUT:\n{}\nSTDERR:\n{}\n".format(
        process_data.stdout, process_data.stderr
    )

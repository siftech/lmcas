import pytest

pytest.register_assert_rewrite("make_tests")


def pytest_addoption(parser):
    parser.addoption("--bloated", action="store_true", help="run bloated tests")
    parser.addoption("--debloated", action="store_true", help="run debloated tests")


def pytest_generate_tests(metafunc):
    if "is_bloated" in metafunc.fixturenames:
        is_bloated = []
        bloated_id = []
        if metafunc.config.getoption("bloated"):
            is_bloated.append(True)
            bloated_id.append("bloated")
        if metafunc.config.getoption("debloated"):
            is_bloated.append(False)
            bloated_id.append("debloated")
        metafunc.parametrize("is_bloated", is_bloated, ids=bloated_id)

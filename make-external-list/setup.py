from setuptools import setup

setup(
    name="make-external-list",
    packages=["make_external_list"],
    install_requires=[],
    entry_points={
        "console_scripts": ["make_external_list = make_external_list.main:main"]
    },
)

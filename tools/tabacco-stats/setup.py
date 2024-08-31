from setuptools import setup

setup(
    name="tabacco-stats",
    packages=["tabacco_stats"],
    install_requires=[],
    entry_points={"console_scripts": ["tabacco-stats = tabacco_stats:main"]},
)

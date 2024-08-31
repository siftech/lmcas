#!/usr/bin/env python3

from functools import partial
import json
from pathlib import Path
import re
import sys


def main(stdin):
    _, book = stdin

    for section in book["sections"]:
        if "Chapter" in section:
            chapter = section["Chapter"]
            chapter["content"] = transform(chapter["path"], chapter["content"])

    return book


pattern = re.compile("{{#asciinema ([^}]+)}}")


def transform(path: str, content: str) -> str:
    return pattern.sub(partial(transform_match, path), content)


def transform_match(path: str, match: re.Match) -> str:
    name = match.group(1)
    root = "../" * (len(Path(path).parents) - 1)

    assert Path(f"src/screencasts/{name}.json").exists()
    return f"""
<noscript>JavaScript is required to render the screencast.</noscript>

<div id="{name}-screencast"></div>

<script>
document.addEventListener("DOMContentLoaded", () => {{
	const element = document.getElementById("{name}-screencast");
	const player = AsciinemaPlayer.create("{root}screencasts/{name}.json", element, {{
        autoPlay: true,
    }});
}});
</script>
"""


if __name__ == "__main__":
    json.dump(main(json.load(sys.stdin)), sys.stdout)

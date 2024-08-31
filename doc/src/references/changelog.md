# Release notes

## Unreleased

- TaBaCCo now removes functions that provably are not accessed after the neck (they are neither called nor returned to).
  The parts that are neatly page-aligned are unmapped, and the parts that aren't are overwritten with INT3 instructions.
  This will foil code reuse attacks that attempt to reuse this code introduced by TaBaCCo.

- Dead code is now more aggressively removed, resulting in code size reductions.

- Only DWARF info is stripped by default; the `-S` flag is renamed to `--strip-debug` accordingly.

- Add support for a `use_guiness` flag for targets which causes neck-miner to run both its own neck selection algorithm
  and the GuiNeSS neck selection algorithm. This produces a max of two candidate necks and then uses a heuristic to
  select between the resulting options. The neck-miner selected candidate is favored in most cases with the guiness neck
  only being selected either when neck-miner failed to select a neck or when the neck-miner neck does not appear on the
  guiness tape (so long as the guiness tape is not ending in a usage function). The `use_guiness` flag defaults to off.
- Adds support for `umask` syscall

- Fixes a deadlock bug when debloating programs that created pointers to thousands of different functions.

- Adds support for the `F_GETFD` command for the `fcntl` syscall

- Improves malloc implementation.
- Fixes determinism issues relating to [`AT_RANDOM`](https://man.archlinux.org/man/core/man-pages/getauxval.3.en#AT_RANDOM).

## 2023-04-10

- Multi-Option Specialization: A binary may now be created that supports more than one set of options, but only the options that are specified.
  See the "Debloating Dropbear" tutorial for an example of use.

- Robustness Checking: Additional checks are done on programs that are being debloated to ensure that program behavior doesn't change based on un-mocked syscalls.
  See the "Debloating Wget" tutorial for an example of use.

- Additional syscalls may now be mocked.

- The driver now strips output programs unless --no-strip is passed.

- The driver now prints a size comparison of the segment size of the programs it's debloated.
  This is a more accurate metric to compare programs with than file size.
  Further improvements to this functionality are planned, stay tuned.

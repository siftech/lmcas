# uwisc

These tests are from the University of Wisconsin-provided dataset containing selected programs from binutils, coreutils, and tcpdump.

Note that `patches/readelf.patch` is a really big patch!
It inlines `parse_args` into the `main` function, so we can probably simplify it significantly once we support subprocedure calls.

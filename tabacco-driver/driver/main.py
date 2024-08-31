#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

import click
from io import TextIOWrapper
import json
from lmcas_dejson import load_json
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Literal, Optional
from . import drive, utils, robustness
from .spec import Spec


@click.command()
@click.argument("debloating_spec_file", type=click.File("r"))
@click.argument("input_program", type=click.File("r"), required=False)
@click.option(
    "-o",
    "--output",
    type=click.Path(dir_okay=False, writable=True, path_type=Path),
    required=True,
    help="The path the debloated binary will be written to",
)
@click.option(
    "--tmpdir",
    type=click.Path(file_okay=False, writable=True, path_type=Path),
    help="The path temporary files will be written to",
)
@click.option(
    "--tabacco-safe-external-function-regex",
    type=str,
    multiple=True,
    help="Additional safe external function regexes to use during the specialization pass",
)
@click.option(
    "-O",
    "--optimize",
    type=click.Choice(["0", "1", "2", "3", "s", "z"]),
    default="3",
    help="Flags controlling how much optimization should be performed",
)
@click.option(
    "-v",
    "--verbose",
    count=True,
    default=0,
    help="Flag controlling verbose flags passed to components.",
)
@click.option(
    " /-N",
    "--debloat/--no-debloat",
    default=True,
    help="Don't debloat, just optimize and whole-program compile",
)
@click.option(
    "--stats/--no-stats",
    default=True,
    help="Whether to report stats about debloating",
)
@click.option(
    " /-S",
    "--strip-debug/--no-strip-debug",
    default=True,
    help="Don't strip the binary's debuginfo",
)
@click.option(
    "--input-type",
    type=click.Choice(["binary", "bitcode", "llvm-ir-src"]),
    default="binary",
    help="type of input-program",
)
@click.option(
    " /-M",
    "--use-neck-miner/--no-use-neck-miner",
    default=True,
    help="Don't use neck-miner, assume the neck is already marked",
)
@click.option(
    " /-R",
    "--robustness-checks/--no-robustness-checks",
    default=True,
    help="Try instrumentation pass multiple times with varying system values (uid, for instance, will vary).",
)
@click.option(
    "--robustness-checks-constant",
    count=True,
    default=3,
    help="Flag controlling the amount of different specs to try on each variable we vary for the robustness check (currently 5 variables).",
)
@click.option(
    " /-G",
    "--use-guiness/--no-use-guiness",
    default=False,
    help="Use the guiness approach for finding the neck",
)
def main(
    debloating_spec_file: TextIOWrapper,
    input_program: Optional[Path],
    output: Path,
    tmpdir: Optional[Path],
    optimize: str,
    verbose: int,
    debloat: bool,
    stats: bool,
    strip_debug: bool,
    tabacco_safe_external_function_regex: tuple[str, ...],
    input_type: Literal["binary", "bitcode", "llvm-ir-src"],
    use_neck_miner: bool,
    robustness_checks: bool,
    robustness_checks_constant: int,
    use_guiness: bool,
) -> None:
    """
    Debloats according to the debloating specification using TaBaCCo.
    """

    # Creates a temporary directory where intermediate files live during
    # the TaBaCCo passes if one is not supplied. The tmpdir_path is given
    # as an argument to all driver passes and used to access expected files.
    if tmpdir is None:
        temp_dir = TemporaryDirectory()
        tmpdir = Path(temp_dir.name)
    tmpdir = tmpdir.resolve()
    tmpdir.mkdir(parents=True, exist_ok=True)

    # Load and validate the debloating specification.
    debloating_spec = load_json(Spec, debloating_spec_file)
    debloating_spec_path = Path(debloating_spec_file.name)

    if input_program is None:
        input_path = debloating_spec.binary.resolve()
    else:
        input_path = Path(input_program.name)

    # Run the stages of TaBaCCo.
    step_num = 1
    step_num, input_bc = drive.extract(
        step_num, tmpdir, input_path, input_type, verbose
    )

    # Consolidate guiness flag
    run_guiness = use_guiness or debloating_spec.use_guiness

    if debloat:
        step_num, with_tabacco_helpers = drive.add_tabacco_helpers(
            step_num, tmpdir, input_bc, verbose
        )
        step_num, annotated = drive.annotate(
            step_num, tmpdir, with_tabacco_helpers, verbose
        )
        if debloating_spec.replace_sighup:
            # Do constant propogation prior to calling the SIGHUP removal pass
            # in order to maximize the number of discovered SIGHUP handlers
            # discovered.
            step_num, annotated = drive.mem2reg(step_num, tmpdir, annotated, verbose)
            step_num, annotated = drive.sccp(step_num, tmpdir, annotated, verbose)
            step_num, annotated = drive.dce(step_num, tmpdir, annotated, verbose)
            step_num, annotated = drive.reg2mem(step_num, tmpdir, annotated, verbose)
            # Now that constant propogation is complete, run the SIGHUP pass
            step_num, annotated = drive.handle_sighup(
                step_num, tmpdir, annotated, verbose
            )

        neck_locations = debloating_spec.extra_neck_locations

        # Combine the binary's bitcode with libc's
        combined = drive.combine(tmpdir, annotated, verbose)

        if use_neck_miner and debloating_spec.use_neck_miner:
            tape = None
            # Run Guiness
            # TODO: only runs guiness on first config for now
            if run_guiness:
                step_num, instrumented = drive.instrument(
                    step_num, tmpdir, annotated, verbose
                )

                config = debloating_spec.configs[0]
                step_num, tape = drive.produce_tape(
                    step_num, tmpdir, config, instrumented, verbose, True
                )

            step_num, neck_miner_neck_locations = drive.mine_necks(
                step_num,
                tmpdir,
                annotated,
                debloating_spec.neck_miner_config,
                verbose=verbose,
                use_guiness=run_guiness,
                combined=combined,
                tape=tape,
            )
            neck_locations += neck_miner_neck_locations

        neck_locations_path = tmpdir / "neck_locations.json"
        with neck_locations_path.open("w") as f:
            json.dump(neck_locations, f, cls=utils.DataclassJSONEncoder)

        step_num, instrumented = drive.instrument(
            step_num,
            tmpdir,
            annotated,
            verbose,
            neck_locations_path=neck_locations_path,
        )

        # If we're fuzzing, we call the function that handles it and return
        # without running all the stages of TaBaCCo
        if robustness_checks:
            step_num, found_difference = robustness.run_multi_tape_checker(
                step_num,
                instrumented,
                robustness_checks_constant,
                debloating_spec,
                tmpdir,
                verbose,
            )
            if found_difference:
                exit(1)
            else:
                print("Robustness checker did not find a difference.")

        tapes = []
        for option_config in debloating_spec.configs:
            step_num, tape = drive.produce_tape(
                step_num,
                tmpdir,
                option_config,
                instrumented,
                verbose,
            )
            tapes.append(tape)

        step_num, specialized = drive.specialize(
            step_num,
            tmpdir,
            debloating_spec,
            combined,
            tapes,
            tabacco_safe_external_function_regex,
            verbose,
            neck_locations_path=neck_locations_path,
        )
    else:
        specialized = input_bc

    step_num, optimized = drive.optimize(
        step_num, tmpdir, specialized, optimize, verbose
    )
    step_num, _ = drive.compile_to_native_code(
        step_num,
        tmpdir,
        output,
        optimized,
        add_libc=not debloat,
        strip_debug=strip_debug,
        verbose=verbose,
    )

    if debloat and stats:
        # Build a non-debloated binary.
        nondebloated_tmpdir = tmpdir / "nondebloated"
        nondebloated_tmpdir.mkdir()
        step_num, optimized_nondebloated = drive.optimize(
            step_num, nondebloated_tmpdir, input_bc, optimize, verbose
        )
        step_num, nondebloated = drive.compile_to_native_code(
            step_num,
            nondebloated_tmpdir,
            nondebloated_tmpdir / "final",
            optimized_nondebloated,
            add_libc=True,
            strip_debug=strip_debug,
            verbose=verbose,
        )

        # Run Bloaty.
        utils.run(
            "bloaty",
            "-d",
            "segments",
            "--domain=vm",
            output,
            "--",
            nondebloated,
            verbose=verbose,
        )


if __name__ == "__main__":
    main()

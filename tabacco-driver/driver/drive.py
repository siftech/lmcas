# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

import copy
import json
from lddcollect import process_elf
from lmcas_sh import sh
from pathlib import Path
from typing import Any, Literal, Optional, Union
import shutil
from . import utils
from .neck_miner import NeckMinerConfig, NeckMinerTaintConfig, NeckMinerTaintFunction
from .spec import NeckLocation, OptionConfig, Spec
from .utils import load_json_file

instrumentation_libc = Path(utils.getenv_or_die("INSTRUMENTATION_LIBC_PATH"))
musl_path = Path(utils.getenv_or_die("MUSL_PATH"))

crtbegin = sh.with_stdout.clang("-print-file-name=clang_rt.crtbegin-x86_64.o").strip()
compiler_rt = sh.with_stdout.clang(
    "-print-file-name=libclang_rt.builtins-x86_64.a"
).strip()
crtend = sh.with_stdout.clang("-print-file-name=clang_rt.crtend-x86_64.o").strip()


def extract(
    step_num: int,
    tmp: Path,
    input_program: Path,
    input_type: Literal["binary", "bitcode", "llvm-ir-src"],
    verbose: int,
) -> tuple[int, Path]:
    """
    Extracts the bitcode from the input, returning the next step number and
    the path to the bitcode.
    """
    if input_type == "binary":
        return extract_binary(step_num, tmp, input_program, verbose)

    if input_type == "bitcode":
        return extract_bitcode(step_num, tmp, input_program)

    if input_type == "llvm-ir-src":
        return extract_llvm_ir_src(step_num, tmp, input_program, verbose)

    raise Exception(f"Invalid input type: {input_type}")


def extract_binary(
    step_num: int, tmp: Path, input_program: Path, verbose: int
) -> tuple[int, Path]:
    """
    Extracts the bitcode from the binary, returning the next step number and
    the path to the extracted bitcode.
    """
    utils.step_done(step_num, f"Extract the bitcode")

    # Create the directory for the input bitcode files.
    input_bitcode_dir = tmp / "input"
    input_bitcode_dir.mkdir(exist_ok=True)

    # Find dependencies for the input binary and store their paths as strings.
    _, files, _ = process_elf([str(input_program)], dpkg=False)

    # Extract bitcode from the dependencies.
    input_bitcode = []
    for file_path in set(Path(file).resolve() for file in files):
        # Ignore the libc (our instrumented libc will be linked to later).
        if file_path.name == "libc.so":
            continue

        bitcode_path = input_bitcode_dir / (file_path.name + ".bc")
        utils.run("get-bc", "-o", bitcode_path, file_path, verbose=verbose)
        input_bitcode.append(bitcode_path)

    out = tmp.joinpath("extracted.bc")
    utils.run("llvm-link", "-o", out, *input_bitcode, verbose=verbose)
    return (step_num + 1, out)


def extract_bitcode(step_num: int, tmp: Path, input_program: Path) -> tuple[int, Path]:
    """
    Copies the bitcode from the input, returning the next step number and
    the path to the copied bitcode.
    """
    out = tmp.joinpath("copied.bc")
    utils.step_done(step_num, "Copying input program")
    shutil.copyfile(input_program, out)
    return (step_num + 1, out)


def extract_llvm_ir_src(
    step_num: int, tmp: Path, input_program: Path, verbose: int
) -> tuple[int, Path]:
    """
    Coverted to bitcode from llvm ir src, returning the next step number and
    the path to the converted bitcode.
    """
    out = tmp.joinpath("converted.bc")
    utils.step_done(step_num, "Converting input program to bitcode")
    utils.run("llvm-as", "-o", out, input_program, verbose=verbose)
    return (step_num + 1, out)


def mem2reg(
    step_num: int, tmp: Path, input_program: Path, verbose: int
) -> tuple[int, Path]:
    """Run the LLVM mem2reg (promote memory to register) transform pass"""
    out_name = Path(str(input_program).replace(".bc", "_mem2reg.bc"))
    out = tmp.joinpath(out_name)
    utils.step_done(step_num, "Running mem2reg")
    step_num += 1
    utils.run(
        "opt",
        "-mem2reg",
        "-verify",
        "-o",
        out,
        input_program,
        verbose=verbose,
    )
    return (step_num, out)


def sccp(
    step_num: int, tmp: Path, input_program: Path, verbose: int
) -> tuple[int, Path]:
    """Run the LLVM sccp (sparse conditional constant propagation) transform pass"""
    out_name = Path(str(input_program).replace(".bc", "_sccp.bc"))
    out = tmp.joinpath(out_name)
    utils.step_done(step_num, "Running sccp")
    step_num += 1
    utils.run("opt", "-sccp", "-verify", "-o", out, input_program, verbose=verbose)
    return (step_num, out)


def reg2mem(
    step_num: int, tmp: Path, input_program: Path, verbose: int
) -> tuple[int, Path]:
    """Run the LLVM reg2mem (demote all values to stack slots) transform pass"""
    out_name = Path(str(input_program).replace(".bc", "_reg2mem.bc"))
    out = tmp.joinpath(out_name)
    utils.step_done(step_num, "Running reg2mem")
    step_num += 1
    utils.run(
        "opt",
        "-reg2mem",
        "-verify",
        "-o",
        out,
        input_program,
        verbose=verbose,
    )
    return (step_num, out)


def dce(
    step_num: int, tmp: Path, input_program: Path, verbose: int
) -> tuple[int, Path]:
    """Run the LLVM dce (dead code elimination) transform pass"""
    out_name = Path(str(input_program).replace(".bc", "_cde.bc"))
    out = tmp.joinpath(out_name)
    utils.step_done(step_num, "Running dce")
    step_num += 1
    utils.run(
        "opt",
        "-dce",
        "-verify",
        "-o",
        out,
        input_program,
        verbose=verbose,
    )
    return (step_num, out)


def handle_sighup(
    step_num: int, tmp: Path, input_program: Path, verbose: int
) -> tuple[int, Path]:
    """
    Removes calls to signal and rt_sigaction that add a handler for SIGHUP. Note
    that this currently only handles cases where the provided signal can be
    resolved to a constant value. Starting with constant propogation is
    recommended.
    """
    out = tmp.joinpath("intermediate_disable_sighup.bc")
    utils.step_done(step_num, "Running disable SIGHUP")
    step_num += 1
    utils.run(
        "opt",
        "-load",
        utils.getenv_or_die("HANDLE_SIGHUP_PASS_PATH"),
        "-handle-sighup",
        "-verify",
        "-o",
        out,
        input_program,
        verbose=verbose,
    )
    return (step_num, out)


def add_tabacco_helpers(
    step_num: int, tmp: Path, input_bitcode: Path, verbose: int
) -> tuple[int, Path]:
    """
    Links in bitcode for various helper functions we link into all binaries
    (and eliminate if unused later). Returns the next step number and the path
    to the newly linked bitcode.
    """

    utils.step_done(step_num, "Link in TaBaCCo helpers")
    out = tmp / "with_tabacco_helpers.bc"
    helpers_path = utils.getenv_or_die("TABACCO_HELPERS_BITCODE")

    utils.run("llvm-link", "-o", out, input_bitcode, helpers_path, verbose=verbose)
    return (step_num + 1, out)


def annotate(
    step_num: int, tmp: Path, unannotated: Path, verbose: int
) -> tuple[int, Path]:
    """
    Runs the annotation pass on the bitcode, returning the next step number and
    the path to the newly annotated bitcode.
    """

    utils.step_done(step_num, "Annotate the bitcode")
    out = tmp / "annotated.bc"

    utils.run(
        "opt",
        "-load",
        utils.getenv_or_die("ANNOTATION_PASS_PATH"),
        "-annotate",
        # id-offset is 2^30 and exists to ensure there's no conflict between
        # the IDs in libc and the ones in the application (the libc's IDs start
        # at 0).
        "-id-offset",
        str(2**30),
        "-verify",
        "-o",
        out,
        unannotated,
        verbose=verbose,
    )
    return (step_num + 1, out)


def mine_necks(
    step_num: int,
    tmp: Path,
    annotated: Path,
    config: Optional[NeckMinerConfig],
    verbose: int,
    use_guiness: bool,
    combined: Path,
    tape: Optional[Path],
) -> tuple[int, list[NeckLocation]]:

    if config is None:
        config = NeckMinerConfig(
            function_local_points_to_info_wo_globals=True,
            use_simplified_dfa=True,
            taint_config=NeckMinerTaintConfig(
                name="cmd-tool-config",
                version=1.0,
                functions=[NeckMinerTaintFunction(name="main", params={"source": [1]})],
            ),
        )

    utils.step_done(step_num, "Mark the neck")
    out = tmp / "neck_miner_neck.json"

    # Write the neck miner configuration to a file
    taint_config = tmp / "neck_miner_taint_config.json"
    with taint_config.open("w") as f:
        json.dump(config.taint_config, f, cls=utils.DataclassJSONEncoder)

    args: list[Union[str, Path]] = [
        "-m",
        annotated,
        "-c",
        taint_config,
    ]
    if use_guiness:
        if tape is None:
            raise Exception("Invalid input: You must provide a tape when using guiness")
        args.append("--guiness-tape")
        args.append(tape)
        args.append("--combined-module")
        args.append(combined)
    if config.function_local_points_to_info_wo_globals:
        args.append("--function-local-points-to-info-wo-globals")
    if config.use_simplified_dfa:
        args.append("--use-simplified-dfa")
    args.append("--neck-placement")
    args.append(out)

    utils.run("neck-miner", *args, verbose=verbose, libsegfault=True)

    neck_locations: list[NeckLocation] = load_json_file(out, list[NeckLocation])
    return (step_num + 1, neck_locations)


def instrument(
    step_num: int,
    tmp: Path,
    annotated: Path,
    verbose: int,
    neck_locations_path: Optional[Path] = None,
) -> tuple[int, Path]:
    """
    Runs the instrumentation pass on annotated bitcode and builds the resulting
    bitcode to a binary, returning the next step number and the path to the
    binary.
    """

    utils.step_done(step_num, "Instrument the annotated bitcode")

    instrumented_bc = tmp / "instrumented.bc"
    flags: list[Union[str, Path]] = ["-instrument", "-verify", "-o", instrumented_bc]
    if isinstance(neck_locations_path, Path):
        flags += ["-neck-locations", neck_locations_path]
    utils.run(
        "opt",
        "-load",
        utils.getenv_or_die("INSTRUMENTATION_PASS_PATH"),
        *flags,
        annotated,
        verbose=verbose,
    )
    step_num += 1

    out = tmp / "instrumented"
    step_num = do_compile_and_link(
        step_num,
        tmp,
        "instrumented",
        out,
        instrumented_bc,
        instrumentation_libc / "libc-instrumented.a",
        verbose=verbose,
    )
    return (step_num, out)


def produce_tape(
    step_num: int,
    tmp: Path,
    option_config: OptionConfig,
    instrumented: Path,
    verbose: int,
    use_guiness: bool = False,
    tape_name: str = "tape.json",
) -> tuple[int, Path]:
    """
    Runs the instrumentation parent on an instrumented binary and a debloating
    specification file to produce a tape, returning the next step number and
    the path to the tape.
    """

    utils.step_done(step_num, "Run the instrumented program to produce a tape")

    # Create the instrumentation spec for the instrumentation-parent to run the
    # instrumented binary in accordance with the OptionConfig.
    instrumentation_spec: dict[str, Any] = {
        "binary": instrumented,
        "args": option_config.args,
        "env": option_config.env,
        "cwd": option_config.cwd,
        "syscall_mocks": copy.copy(option_config.syscall_mocks),
    }

    # temporarily add mock for ioctl if we are using guiness
    # all syscalls must be allowed through & there's a bug w/ allowing ioctl through
    if use_guiness:
        instrumentation_spec["syscall_mocks"]["sys_ioctl"] = {
            "terminal_dimensions": {
                "row": 25,
                "col": 80,
                "xpixel": 0,
                "ypixel": 0,
            },
            "unsafe_allow_tiocgwinsz": True,
        }

    instrumentation_spec_path = tmp / f"spec_{step_num}.json"
    with instrumentation_spec_path.open("w") as file:
        json.dump(instrumentation_spec, file, cls=utils.DataclassJSONEncoder)

    out = tmp / f"tape_{step_num}.json"
    args: list[Union[str, Path]] = []
    if verbose > 0:
        args = args + ["-" + ("v" * verbose)]
    if use_guiness:
        args = args + ["--no-fail-on-unhandled-syscall"]

    args = args + [instrumentation_spec_path, out]

    utils.run("instrumentation-parent", *args, verbose=verbose)
    return (step_num + 1, out)


def combine(tmp: Path, annotated: Path, verbose: int) -> Path:
    # Combine the binary's bitcode with libc's.
    combined = tmp / "combined.bc"
    utils.run(
        "llvm-link",
        "-o",
        combined,
        annotated,
        instrumentation_libc / "annotated.bc",
        verbose=verbose,
    )

    return combined


def specialize(
    step_num: int,
    tmp: Path,
    debloating_spec: Spec,
    combined: Path,
    tapes: list[Path],
    safe_external_function_regexes: tuple[str, ...],
    verbose: int,
    neck_locations_path: Optional[Path] = None,
) -> tuple[int, Path]:
    """
    Runs the specialization pass on annotated bitcode, returning the next step
    number and the path to the binary.

    The safe_external_function_regexes argument allows the user to specify which
    additional external function calls are safe and can be ignored if
    encountered. This is mostly a debugging option.
    """

    utils.step_done(step_num, "Specialize the annotated bitcode using the tape")

    # Collect additional arguments for the specialization pass.
    extra_args: list[Union[Path, str]] = []
    for regex in safe_external_function_regexes:
        extra_args.append("--tabacco-safe-external-function-regex")
        extra_args.append(regex)
    if isinstance(neck_locations_path, Path):
        extra_args += ["-neck-locations", neck_locations_path]

    # Make the specialization specification. This basically just involves
    # embedding the tapes into the debloating specification.
    specialization_spec = {
        "binary": debloating_spec.binary,
        "configs": [
            {
                "args": option_config.args,
                "env": option_config.env,
                "cwd": option_config.cwd,
                "tape": load_json_file(tapes[i]),
            }
            for i, option_config in enumerate(debloating_spec.configs)
        ],
    }

    # Write out the specialization specification.
    specialization_spec_path = tmp / f"specialization.json"
    with specialization_spec_path.open("w") as file:
        json.dump(specialization_spec, file, cls=utils.DataclassJSONEncoder)

    # Make the list of functions not to internalize.
    external_names = sh.with_stdout.make_external_list(
        musl_path / "crt1.o",
        musl_path / "crti.o",
        crtbegin,
        instrumentation_libc / "asm-objects.a",
        compiler_rt,
        crtend,
        musl_path / "crtn.o",
    ).strip()

    # Run the specialization pass.
    out = tmp / "specialized.bc"
    utils.run(
        "opt",
        "--load",
        utils.getenv_or_die("SPECIALIZATION_PASS_PATH"),
        "--tabacco-specialize",
        "--specialization-spec",
        specialization_spec_path,
        "--tabacco-safe-external-function-regex",
        "(alloc|aligned_alloc|__libc_malloc_impl|__libc_free|__libc_realloc|__malloc_donate)(\\.[0-9]+)?",
        "--tabacco-safe-external-function-regex",
        "mem(cmp|cpy|set)",
        "--tabacco-safe-external-function-regex",
        "__get_tp(\\.[0-9]+)?",
        "--tabacco-safe-external-function-regex",
        "getenv",
        "-internalize",
        "-internalize-public-api-list",
        external_names,
        *extra_args,
        "--verify",
        "-o",
        out,
        combined,
        verbose=verbose,
    )
    return (step_num + 1, out)


def optimize(
    step_num: int,
    tmp: Path,
    specialized: Path,
    opt_level: str,
    verbose: int,
) -> tuple[int, Path]:
    """
    Runs the standard LLVM optimizations on the bitcode at the user-specified
    optimization level.
    """

    utils.step_done(step_num, "Optimize the specialized bitcode")
    out = tmp / "optimized.bc"
    utils.run("opt", f"-O{opt_level}", "-o", out, specialized, verbose=verbose)
    return (step_num + 1, out)


def compile_to_native_code(
    step_num: int,
    tmp: Path,
    out: Path,
    optimized: Path,
    *,
    add_libc: bool = False,
    strip_debug: bool = False,
    verbose: int,
) -> tuple[int, Path]:
    """
    Compiles the bitcode to native code.
    """

    if add_libc:
        combined = tmp / "combined.bc"
        utils.run(
            "llvm-link",
            "-o",
            combined,
            optimized,
            instrumentation_libc / "annotated.bc",
            verbose=verbose,
        )
        optimized = combined

    step_num = do_compile_and_link(
        step_num,
        tmp,
        "final",
        out,
        optimized,
        instrumentation_libc / "asm-objects.a",
        verbose=verbose,
    )

    if strip_debug:
        utils.run("strip", "--strip-debug", out, verbose=verbose)

    return (step_num, out)


def do_compile_and_link(
    step_num: int,
    tmp: Path,
    label: str,
    out: Path,
    src: Path,
    *other_objs: Path,
    verbose: int,
) -> int:
    """
    Compiles the bitcode in src and links it to other_objs to produce a binary
    at out, returning the new step number.
    """

    utils.step_done(step_num, f"Compile the {label} bitcode")
    asm = tmp / f"{label}.s"

    utils.run(
        "llc",
        "--data-sections",
        "--function-sections",
        "-o",
        asm,
        src,
        verbose=verbose,
    )
    step_num += 1

    utils.step_done(step_num, f"Assemble the {label} program")
    obj = tmp / f"{label}.o"
    utils.run("as", "-o", obj, asm, verbose=verbose)
    step_num += 1

    # Remove assembly file because it's huge, and we can reconstruct it from
    # either of the bitcode or object files.
    #
    # TODO: Pipe llc into as to avoid having it be on disk at all.
    asm.unlink()

    utils.step_done(step_num, f"Link the {label} program")

    ld_path = utils.extract_command_output("clang", "-print-prog-name=ld")

    utils.run(
        ld_path,
        "--gc-sections",
        "-o",
        out,
        "-nostdlib",
        musl_path / "crt1.o",
        musl_path / "crti.o",
        crtbegin,
        obj,
        *other_objs,
        compiler_rt,
        crtend,
        musl_path / "crtn.o",
        verbose=verbose,
    )
    return step_num + 1

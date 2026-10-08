"""
Action to run the libFuzzer targets (`owl_*_fuzzer`) for a fixed time each.

The preset must build them (`OWL_FUZZING=ON` with AddressSanitizer, e.g. `linux-fuzz`). Each fuzzer runs on
`output/fuzz/<fuzzer>/corpus`, kept between runs on the same agent, seeded from `fuzz/corpus/<fuzzer>/` when that
folder exists. A crash, a leak or a timeout leaves its input in `output/fuzz/<fuzzer>/artifacts/` and fails the
action; replay it with `bin/<fuzzer> <input>`.
"""

from __future__ import annotations

from pathlib import Path

from ci import log, root
from ci.actions.base.action import BaseAction, PresetConfig
from ci.utils.run import run_command

DEFAULT_SECONDS = 300
FUZZER_GLOB = "owl_*_fuzzer"


def find_fuzzers(bin_dir: Path, only: str | None = None) -> list[Path]:
    """
    The fuzzer executables of a build.

    :param bin_dir: The `bin/` folder of the build.
    :param only: Keep only the fuzzer of this name (``owl_pack_reader_fuzzer``), if given.
    :return: The fuzzers, sorted by name.
    """
    found = sorted(p.resolve() for p in bin_dir.glob(FUZZER_GLOB) if p.is_file())
    return [p for p in found if only is None or p.name == only]


def fuzzer_command(binary: Path, work_dir: Path, seed_dir: Path, seconds: int) -> list[str]:
    """
    Command line running one fuzzer for a fixed time.

    :param binary: The fuzzer executable.
    :param work_dir: Its folder under `output/fuzz/` (corpus and artifacts).
    :param seed_dir: Its committed seed corpus, used only when the folder exists.
    :param seconds: Total fuzzing time.
    :return: The command, the growing corpus first so that new inputs are written there.
    """
    command = [
        str(binary),
        f"-max_total_time={seconds}",
        f"-artifact_prefix={work_dir / 'artifacts'}/",
        "-print_final_stats=1",
        str(work_dir / "corpus"),
    ]
    if seed_dir.is_dir():
        command.append(str(seed_dir))
    return command


class Fuzz(BaseAction):
    """Run every fuzzer of a preset for a fixed time.

    Extra arguments (after ``--``):
      * ``--time=<seconds>`` — fuzzing time per fuzzer, default 300;
      * ``--fuzzer=<name>`` — run only this fuzzer.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """Run the fuzzers.

        :param preset: A preset building the fuzzers.
        :param extra_args: Optional extra arguments, see the class docstring.
        :return: 0 when every fuzzer ran its time without finding anything, 1 otherwise or on failure.
        """
        args = self.parse_extra_args(extra_args)
        seconds = int(args.get("time", DEFAULT_SECONDS))
        fuzzers = find_fuzzers(preset.get_build_dir() / "bin", args.get("fuzzer"))
        if not fuzzers:
            log.error(
                f"Fuzz: no {FUZZER_GLOB} in {preset.get_build_dir() / 'bin'}; build the preset with "
                f"OWL_FUZZING=ON first."
            )
            return 1
        failed = []
        for binary in fuzzers:
            name = binary.name.removeprefix("owl_").removesuffix("_fuzzer")
            work_dir = root / "output" / "fuzz" / binary.name
            (work_dir / "corpus").mkdir(parents=True, exist_ok=True)
            (work_dir / "artifacts").mkdir(parents=True, exist_ok=True)
            log.info(f"Fuzz: {binary.name} for {seconds} s.")
            if (
                run_command(fuzzer_command(binary, work_dir, root / "fuzz" / "corpus" / name, seconds), cwd=work_dir)
                != 0
            ):
                log.error(f"Fuzz: {binary.name} found a failing input, see {work_dir.relative_to(root)}/artifacts.")
                failed.append(binary.name)
        if failed:
            log.error(f"Fuzz: {len(failed)} fuzzer(s) failed: {', '.join(failed)}.")
            return 1
        log.info(f"Fuzz: {len(fuzzers)} fuzzer(s), nothing found.")
        return 0

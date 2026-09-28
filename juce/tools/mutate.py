#!/usr/bin/env python3
"""Runs a mutation check on isolated copies of a project tree, in parallel (RQ-BLD-015).

A mutation check alters a source file on purpose (a text find/replace) to see whether a test fails.
Run in place, that edits the files the developer is working in and typically rebuilds and re-runs the
whole suite per alteration; this script instead copies the project into a scratch directory per
alteration, applies one alteration, builds only the CMake target that needs it, runs only the tests
selected for it under a per-test time limit, and reports the outcome — the working tree and its build
directory are never touched.

Alterations are read from a JSON file: a list of objects, each with
    name      a short name for the alteration
    file      path to the source file, relative to the project root
    find      the exact text to replace (must occur exactly once, or the alteration is skipped)
    replace   the replacement text
    target    the CMake target to build (only it, and what it depends on, is rebuilt)
    tests     the ctest -R regex selecting the tests to run for this alteration

Usage:
    python3 juce/tools/mutate.py --project-root <dir> --alterations <alterations.json> [--workers N]
        [--timeout SECONDS] [--config Debug] [--reuse-deps-from <build-dir>] [--json-out <file>]

(On Windows the interpreter is usually `python`.)

Exit status: 0 when every alteration was caught or skipped, 1 when at least one was missed (a real
test-coverage gap), 2 when the tool itself failed (bad input, the working tree changed, a build or
configure step could not even start).

[RQ-BLD-015, TASK-BLD-012]
"""
import argparse
import concurrent.futures
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile

EXIT_OK = 0
EXIT_MISSED = 1
EXIT_TOOL_ERROR = 2

DEFAULT_EXCLUDED_DIR_NAMES = {".git", "build", "build-win-akm", "build-win-local", "build-win-bld"}


class ToolError(Exception):
    """The tool itself could not proceed; not a mutation outcome."""


def load_alterations(path):
    try:
        data = json.loads(pathlib.Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise ToolError(f"{path}: cannot be read as JSON ({error})") from error
    if not isinstance(data, list):
        raise ToolError(f"{path}: must be a JSON list of alterations")
    required = {"name", "file", "find", "replace", "target", "tests"}
    for index, entry in enumerate(data):
        if not isinstance(entry, dict) or not required.issubset(entry):
            raise ToolError(f"{path}: alteration [{index}] must have {sorted(required)}")
    return data


def is_excluded(relative_parts, excluded_dir_names):
    return any(part in excluded_dir_names for part in relative_parts)


def hash_tree(root, excluded_dir_names):
    """One hash per file under `root`, excluding `excluded_dir_names` at any depth: the manifest that
    proves the working tree was not touched (RQ-BLD-015)."""
    digest = hashlib.sha256()
    for file_path in sorted(root.rglob("*")):
        if not file_path.is_file():
            continue
        relative = file_path.relative_to(root)
        if is_excluded(relative.parts, excluded_dir_names):
            continue
        digest.update(str(relative).encode("utf-8"))
        digest.update(file_path.read_bytes())
    return digest.hexdigest()


def copy_tree(root, destination, excluded_dir_names):
    def ignore(current_dir, names):
        return [name for name in names if name in excluded_dir_names]

    shutil.copytree(root, destination, ignore=ignore)


def apply_alteration(copy_root, alteration):
    """Edits the file in place inside the copy; returns None when applied, or a skip reason."""
    target_file = copy_root / alteration["file"]
    try:
        text = target_file.read_text(encoding="utf-8")
    except OSError as error:
        return f"cannot read {alteration['file']} ({error})"
    count = text.count(alteration["find"])
    if count != 1:
        return f"the text was found {count} times in {alteration['file']}, not once"
    target_file.write_text(text.replace(alteration["find"], alteration["replace"], 1), encoding="utf-8")
    return None


def configure(copy_root, build_dir, generator, reuse_deps_from, cmake_args):
    command = ["cmake", "-S", str(copy_root), "-B", str(build_dir)]
    if generator:
        command += ["-G", generator]
    if reuse_deps_from:
        deps_dir = pathlib.Path(reuse_deps_from) / "_deps"
        if deps_dir.is_dir():
            command.append(f"-DFETCHCONTENT_BASE_DIR={deps_dir}")
    command += cmake_args
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    return result.returncode == 0, result.stdout + result.stderr


def build(build_dir, target, config):
    command = ["cmake", "--build", str(build_dir), "--target", target]
    if config:
        command += ["--config", config]
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    return result.returncode == 0, result.stdout + result.stderr


def run_tests(build_dir, tests_regex, timeout_seconds, config):
    command = ["ctest", "--test-dir", str(build_dir), "-R", tests_regex, "--timeout", str(timeout_seconds),
               "--stop-on-failure", "--output-on-failure"]
    if config:
        command += ["-C", config]
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    return result.returncode == 0, result.stdout + result.stderr


def run_one_alteration(project_root, alteration, scratch_root, generator, config, reuse_deps_from, cmake_args,
                        timeout_seconds, keep_scratch):
    name = alteration["name"]
    copy_root = pathlib.Path(scratch_root) / name
    if copy_root.exists():
        shutil.rmtree(copy_root)
    copy_tree(pathlib.Path(project_root), copy_root, DEFAULT_EXCLUDED_DIR_NAMES)
    try:
        skip_reason = apply_alteration(copy_root, alteration)
        if skip_reason:
            return {"name": name, "outcome": "skipped", "detail": skip_reason}

        build_dir = copy_root / "_mutate_build"
        configured, configure_log = configure(copy_root, build_dir, generator, reuse_deps_from, cmake_args)
        if not configured:
            return {"name": name, "outcome": "caught", "detail": "configure failed", "log": configure_log}

        built, build_log = build(build_dir, alteration["target"], config)
        if not built:
            return {"name": name, "outcome": "caught", "detail": "build failed", "log": build_log}

        passed, test_log = run_tests(build_dir, alteration["tests"], timeout_seconds, config)
        outcome = "missed" if passed else "caught"
        return {"name": name, "outcome": outcome, "detail": "tests " + ("passed" if passed else "failed"),
                "log": test_log}
    finally:
        if not keep_scratch:
            shutil.rmtree(copy_root, ignore_errors=True)


def run_mutation_check(project_root, alterations, workers, generator, config, reuse_deps_from, cmake_args,
                        timeout_seconds, keep_scratch):
    project_root = pathlib.Path(project_root).resolve()
    if not project_root.is_dir():
        raise ToolError(f"{project_root}: not a directory")

    before = hash_tree(project_root, DEFAULT_EXCLUDED_DIR_NAMES)
    scratch_root = tempfile.mkdtemp(prefix="mutate-")
    try:
        results = []
        with concurrent.futures.ProcessPoolExecutor(max_workers=workers) as pool:
            futures = {
                pool.submit(run_one_alteration, project_root, alteration, scratch_root, generator, config,
                            reuse_deps_from, cmake_args, timeout_seconds, keep_scratch): alteration["name"]
                for alteration in alterations
            }
            for future in concurrent.futures.as_completed(futures):
                results.append(future.result())
    finally:
        if not keep_scratch:
            shutil.rmtree(scratch_root, ignore_errors=True)

    after = hash_tree(project_root, DEFAULT_EXCLUDED_DIR_NAMES)
    if before != after:
        raise ToolError(f"the working tree at {project_root} changed while the check ran")

    order = {alteration["name"]: index for index, alteration in enumerate(alterations)}
    results.sort(key=lambda result: order[result["name"]])
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--project-root", required=True, help="the CMake project to check (never modified)")
    parser.add_argument("--alterations", required=True, help="a JSON file listing the alterations to try")
    parser.add_argument("--workers", type=int, default=1, help="alterations run at once (default 1)")
    parser.add_argument("--timeout", type=float, default=60, dest="timeout_seconds",
                        help="per-test time limit in seconds (default 60)")
    parser.add_argument("--generator", default=None, help="CMake generator (default: the platform's own)")
    parser.add_argument("--config", default=None, help="build/test configuration for a multi-config generator")
    parser.add_argument("--reuse-deps-from", default=None,
                        help="a build directory whose _deps/ FetchContent sources are reused, not re-downloaded")
    parser.add_argument("--cmake-arg", action="append", default=[], dest="cmake_args",
                        help="an extra -D... passed to the configure step (repeatable)")
    parser.add_argument("--json-out", default=None, help="also write the results as JSON to this file")
    parser.add_argument("--keep-scratch", action="store_true", help="keep the scratch copies for inspection")
    args = parser.parse_args()

    try:
        alterations = load_alterations(args.alterations)
        results = run_mutation_check(args.project_root, alterations, args.workers, args.generator, args.config,
                                     args.reuse_deps_from, args.cmake_args, args.timeout_seconds, args.keep_scratch)
    except ToolError as error:
        print(f"mutate.py: {error}", file=sys.stderr)
        return EXIT_TOOL_ERROR

    missed = 0
    for result in results:
        print(f"{result['outcome']:>7}  {result['name']}  ({result['detail']})")
        if result["outcome"] == "missed":
            missed += 1

    if args.json_out:
        pathlib.Path(args.json_out).write_text(json.dumps(results, indent=2), encoding="utf-8")

    return EXIT_MISSED if missed else EXIT_OK


if __name__ == "__main__":
    sys.exit(main())

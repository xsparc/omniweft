#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build an isolated native identity-TRS mutant; require the real GPU oracle to reject it."""
import argparse
import difflib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import sys
import tempfile

from render_test_support import Evidence, Failure, ROOT, digest, execute_main, require

TARGET = "src/presentation.cpp"
ORIGINAL = "    const world::Transform render_transform = source;"
MUTANT = "    const world::Transform render_transform{};"
EXPECTED_FAILURE = "initial: GPU ID mask"


def cache_values(build):
    raw = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    return dict(re.findall(r"^([A-Za-z0-9_-]+):[^=\n]+=(.*)$",raw,re.MULTILINE))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir",type=Path,required=True)
    parser.add_argument("--evidence-dir",type=Path,required=True)
    args = parser.parse_args()
    build = args.build_dir.resolve(strict=True)
    executable_name = "omniweft_examples.exe" if sys.platform == "win32" else "omniweft_examples"
    executable = build / executable_name
    evidence = Evidence(args.evidence_dir,executable,"gpu","deliberate_native_identity_transform")
    source_before = (ROOT / TARGET).read_bytes()
    executable_before = executable.read_bytes()
    try:
        cache = cache_values(build)
        require(Path(cache["CMAKE_HOME_DIRECTORY"]).resolve() == ROOT,"baseline build belongs to the exact current source")
        require(cache.get("OW_ENABLE_VULKAN") in ("ON","TRUE","1"),"baseline configuration actually enables Vulkan")
        source_text = source_before.decode("utf-8-sig")
        require(source_text.count(ORIGINAL) == 1,"identity mutation matches exactly one native transform binding")
        mutated_text = source_text.replace(ORIGINAL,MUTANT)
        require(mutated_text != source_text,"mutation changes the real presentation transform")
        with tempfile.TemporaryDirectory(prefix="ow-render-mutant-") as temporary:
            temporary = Path(temporary)
            source,mutated_build = temporary/"source",temporary/"build"
            source.mkdir()
            tracked = evidence.run(["git","ls-files","-z"],["git","ls-files","-z"],timeout=10)
            require(tracked.returncode == 0,"enumerate exact tracked candidate sources")
            # Copy only repository-tracked files; private logs, tooling caches and user configuration stay out.
            for relative in tracked.stdout.split("\0"):
                if not relative:
                    continue
                path = PurePosixPath(relative)
                require(not path.is_absolute() and ".." not in path.parts and ".git" not in path.parts,
                        "tracked source remains repository-relative")
                original = ROOT / relative
                require(original.is_file() and not original.is_symlink() and original.resolve().is_relative_to(ROOT),
                        "tracked source copy cannot escape checkout")
                destination = source / relative
                destination.parent.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(original,destination)
            (source/TARGET).write_text(mutated_text,encoding="utf-8",newline="\n")
            mutated_source = (source/TARGET).read_bytes()
            evidence.manifest["mutation"] = {
                "source_path":TARGET, "original_source_sha256":digest(source_before),
                "mutated_source_sha256":digest(mutated_source),
                "original_executable_sha256":digest(executable_before),
                "expected_oracle_failure":EXPECTED_FAILURE}
            patch = "".join(difflib.unified_diff(source_text.splitlines(keepends=True),
                      mutated_text.splitlines(keepends=True),fromfile="original/"+TARGET,tofile="mutant/"+TARGET))
            evidence.retain("mutation.patch",patch.encode("utf-8"))
            cmake = cache["CMAKE_COMMAND"]
            configure = [cmake,"-S",str(source),"-B",str(mutated_build),"-G","Ninja","-DBUILD_TESTING=OFF"]
            public = ["<cmake>","-S","<mutant-source>","-B","<mutant-build>","-G","Ninja","-DBUILD_TESTING=OFF"]
            keys = sorted(key for key in cache if key.startswith("OW_") or key in {
                "CMAKE_BUILD_TYPE","CMAKE_C_COMPILER","CMAKE_CXX_COMPILER","CMAKE_MAKE_PROGRAM",
                "CMAKE_C_FLAGS","CMAKE_CXX_FLAGS","CMAKE_EXE_LINKER_FLAGS"})
            for key in keys:
                value = cache[key]
                if value.endswith("-NOTFOUND"):
                    continue
                configure.append("-D"+key+"="+value)
                # Only fixed switches are copied literally. Compiler/dependency paths and flags remain private.
                shown = value if key in ("OW_ENABLE_VULKAN","CMAKE_BUILD_TYPE") else "<same-baseline-setting>"
                public.append("-D"+key+"="+shown)
            configured = evidence.run(configure,public,timeout=180)
            require(configured.returncode == 0,"isolated mutant configures with baseline compiler and pinned dependencies")
            built = evidence.run([cmake,"--build",str(mutated_build),"--target","omniweft_examples","--parallel","2"],
                                 ["<cmake>","--build","<mutant-build>","--target","omniweft_examples","--parallel","2"],
                                 timeout=300)
            require(built.returncode == 0,"isolated identity mutant compiles")
            mutated_executable = mutated_build/executable_name
            evidence.manifest["mutation"]["mutated_executable_sha256"] = digest(mutated_executable.read_bytes())
            command = [sys.executable,str(ROOT/"tests/render_oracle.py"),"--executable",str(mutated_executable),
                       "--gpu","--case-set","transform"]
            oracle = evidence.run(command,["<python>","tests/render_oracle.py","--executable","<mutant-executable>",
                                          "--gpu","--case-set","transform"],timeout=60)
            require(oracle.returncode == 1,"unmodified independent GPU oracle rejects native identity mutant")
            require(EXPECTED_FAILURE in oracle.stderr,"mutant fails specifically on real GPU ID coverage")
            # Retain only a fixed expected assertion and exit code, never raw native/vendor process output.
            evidence.retain_json("oracle-rejection.json",{"exit_code":oracle.returncode,
                                 "assertion":EXPECTED_FAILURE,"status":"expected_failure"})
        require(digest((ROOT/TARGET).read_bytes()) == digest(source_before),"original native source remains byte-for-byte intact")
        require(digest(executable.read_bytes()) == digest(executable_before),"original native executable remains byte-for-byte intact")
        evidence.manifest["lanes"] = {"gpu_mutation_proof":"passed"}
        evidence.finish("passed")
        print(json.dumps({"status":"passed","candidate_sha":evidence.manifest["candidate_sha"],
                          "proof":"independent real GPU oracle rejected a native identity transform mutation"}))
        return 0
    except Failure as error:
        evidence.finish("failed",str(error))
        raise
    except Exception:
        evidence.finish("failed","unexpected internal error; private details withheld")
        raise
    finally:
        # Report source/binary preservation even if the mutant failed to build or start.
        if digest((ROOT/TARGET).read_bytes()) != digest(source_before) or digest(executable.read_bytes()) != digest(executable_before):
            raise Failure("mutation attempt altered protected original source or executable")


if __name__ == "__main__":
    raise SystemExit(execute_main(main))


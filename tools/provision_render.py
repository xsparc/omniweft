#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Explicit optional graphics dependency provisioning; CMake never downloads."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, default=ROOT / ".cache" / "render-deps")
    options = parser.parse_args()
    pins = json.loads((ROOT / "toolchains" / "render-dependencies.json").read_text(encoding="utf-8"))
    destination = options.directory.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    for name, pin in pins["source_archives"].items():
        archive = destination / pin["archive"]
        if not archive.exists():
            temporary = archive.with_suffix(archive.suffix + ".partial")
            with urllib.request.urlopen(pin["url"], timeout=60) as source, temporary.open("xb") as output:
                shutil.copyfileobj(source, output)
            temporary.rename(archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != pin["sha256"]:
            raise RuntimeError(f"{name}: archive checksum mismatch; remove the cached archive and retry")
        target = destination / pin["directory"]
        if not target.exists():
            with tarfile.open(archive, "r:gz") as package:
                members = package.getmembers()
                if any(not member.name.startswith(pin["directory"] + "/") and member.name != pin["directory"] for member in members):
                    raise RuntimeError(f"{name}: unexpected archive root")
                package.extractall(destination, filter="data")
        # Expected source hashes come from the reviewed archive, never the existing tree.
        entries = {}
        with tarfile.open(archive, "r:gz") as package:
            for member in package.getmembers():
                if member.isfile():
                    relative = member.name[len(pin["directory"])+1:]
                    entries[relative] = hashlib.sha256(package.extractfile(member).read()).hexdigest()
        for relative, expected in entries.items():
            if hashlib.sha256((target / relative).read_bytes()).hexdigest() != expected:
                raise RuntimeError(f"{name}: source file mismatch; remove the extracted tree and retry")
        manifest = json.dumps(entries, sort_keys=True, indent=2) + "\n"
        manifest_hash = hashlib.sha256(manifest.encode()).hexdigest()
        if "source_manifest_sha256" in pin and manifest_hash != pin["source_manifest_sha256"]:
            raise RuntimeError(f"{name}: source manifest differs from the reviewed pin")
        (destination / (name + "-files.json")).write_text(manifest, encoding="utf-8", newline="\n")
        print(f"{name}: verified archive and provisioned source files")
    return 0
if __name__ == "__main__":
    raise SystemExit(main())

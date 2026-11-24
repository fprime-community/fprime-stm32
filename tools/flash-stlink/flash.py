#!/usr/bin/env python3
"""Flash an installed STM32 F Prime deployment through ST-LINK."""

import argparse
import configparser
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile


def project_root(start: Path) -> Path:
    for path in (start.resolve(), *start.resolve().parents):
        if (path / "settings.ini").is_file() and (path / "CMakeLists.txt").is_file():
            return path
    raise ValueError(f"No F Prime project found above {start}; use --project-root")


def default_toolchain(root: Path) -> str:
    config = configparser.ConfigParser()
    with (root / "settings.ini").open(encoding="utf-8") as settings:
        config.read_file(settings)
    if not config.has_option("fprime", "default_toolchain"):
        raise ValueError("settings.ini has no [fprime] default_toolchain; use --toolchain")
    return config.get("fprime", "default_toolchain")


def find_images(root: Path, toolchain: str, build_dir: Path | None) -> dict[str, Path]:
    if build_dir is None:
        artifact_dir = root / "build-artifacts" / toolchain
        return {
            entry.name: entry / "bin" / entry.name
            for entry in sorted(artifact_dir.iterdir())
            if entry.is_dir() and (entry / "bin" / entry.name).is_file()
        } if artifact_dir.is_dir() else {}
    binary_dir = build_dir / "bin" / toolchain
    return {
        entry.name: entry
        for entry in sorted(binary_dir.iterdir())
        if entry.is_file() and not entry.suffix
    } if binary_dir.is_dir() else {}


def validate_image(image: Path) -> None:
    with image.open("rb") as stream:
        header = stream.read(20)
    if (
        len(header) < 20
        or header[:6] != b"\x7fELF\x01\x01"
        or struct.unpack_from("<HH", header, 16) != (2, 40)
    ):
        raise ValueError(f"Not a 32-bit little-endian ARM executable ELF: {image}")


def find_programmer(explicit: str | None) -> str:
    if explicit:
        executable = shutil.which(explicit)
        if executable is None:
            raise ValueError(f"STM32CubeProgrammer executable not found: {explicit}")
        return executable
    executable = shutil.which("STM32_Programmer_CLI")
    if executable:
        return executable
    bundled = [
        path for path in
        (Path.home() / ".local/share/stm32cube/bundles/programmer").glob("*/bin/STM32_Programmer_CLI")
        if path.is_file() and os.access(path, os.X_OK)
    ]
    if bundled:
        return str(max(bundled, key=lambda path: tuple(int(n) for n in re.findall(r"\d+", path.parent.parent.name))))
    raise ValueError("STM32_Programmer_CLI not found; add it to PATH or use --programmer")


def select_deployment(images: dict[str, Path]) -> str:
    print("Available built deployments:")
    names = sorted(images)
    for index, name in enumerate(names, 1):
        print(f"  {index}. {name}")
    while True:
        choice = input("Select deployment (number or name): ").strip()
        if choice in images:
            return choice
        if choice.isdecimal() and 1 <= int(choice) <= len(names):
            return names[int(choice) - 1]
        print("Invalid selection; try again.", file=sys.stderr)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("deployment", nargs="?", help="deployment name; omit for an interactive menu")
    parser.add_argument("--project-root", type=Path, help="F Prime project root (default: search from current directory)")
    parser.add_argument("-t", "--toolchain", help="artifact toolchain (default: settings.ini)")
    parser.add_argument("--build-dir", type=Path, help="use <build-dir>/bin/<toolchain> instead of installed artifacts")
    parser.add_argument("--programmer", help="STM32_Programmer_CLI path (default: PATH or STM32Cube bundle)")
    parser.add_argument("--serial", help="ST-LINK serial number when multiple probes are connected")
    parser.add_argument("--list", action="store_true", help="list built deployments without flashing")
    parser.add_argument("--dry-run", action="store_true", help="show the programmer command without connecting")
    args = parser.parse_args(argv)

    try:
        root = project_root(args.project_root or Path.cwd())
        toolchain = args.toolchain or default_toolchain(root)
        build_dir = args.build_dir.resolve() if args.build_dir else None
        images = find_images(root, toolchain, build_dir)
        if not images:
            raise ValueError("No built deployment images found; build and install a deployment first")
        if args.list:
            for name, image in images.items():
                print(f"{name}: {image}")
            return 0
        deployment = args.deployment or select_deployment(images)
        if deployment not in images:
            raise ValueError(f"No built image for {deployment!r}; available: {', '.join(images)}")
        image = images[deployment].resolve()
        validate_image(image)
        programmer = find_programmer(args.programmer)
        print(f"Deployment: {deployment}\nImage: {image}\nProgrammer: {programmer}", flush=True)
        connect = ["-c", "port=SWD"]
        if args.serial:
            connect.append(f"sn={args.serial}")
        if args.dry_run:
            print(f"Command: {programmer} {' '.join(connect)} -d <temporary-elf-copy> -v -rst")
            return 0
        # F Prime installs extensionless executables; CubeProgrammer identifies ELF files by extension.
        with tempfile.TemporaryDirectory(prefix="fprime-stm32-flash-") as directory:
            elf = Path(directory) / "deployment.elf"
            shutil.copyfile(image, elf)
            result = subprocess.run([programmer, *connect, "-d", str(elf), "-v", "-rst"], check=False)
        if result.returncode:
            print(f"STM32CubeProgrammer failed with exit code {result.returncode}", file=sys.stderr)
        return result.returncode
    except (OSError, ValueError, configparser.Error) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    except (EOFError, KeyboardInterrupt):
        print("\nAborted.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

import re
from dataclasses import dataclass
from pathlib import Path

from fprime_stm32.errors import CubeMxCMakeError, ProjectDiscoveryError, StartupScriptError, LinkerScriptError
from fprime_stm32.family import Family


@dataclass
class ProjectContext:
    namespace_root: Path
    hardware_dir: Path


@dataclass
class CubeMxSources:
    linker_script: Path
    startup_script: Path
    chip_tag: str
    cubemx_cmake_file: Path


def find_fprime_project_root(cwd: Path, deployment: str | None = None) -> ProjectContext:
    """Locate the F' deployment namespace root (the directory containing Hardware/)."""
    if deployment is not None:
        hardware_dir = cwd / deployment / "Hardware"
        if not hardware_dir.is_dir():
            raise ProjectDiscoveryError(
                f"--deployment {deployment!r} does not contain a Hardware/ directory "
                f"(expected {hardware_dir})"
            )
        return ProjectContext(namespace_root=cwd / deployment, hardware_dir=hardware_dir)

    direct = cwd / "Hardware"
    if direct.is_dir():
        return ProjectContext(namespace_root=cwd, hardware_dir=direct)

    candidates = sorted(p.parent for p in cwd.glob("*/Hardware") if p.is_dir())
    if len(candidates) == 1:
        return ProjectContext(namespace_root=candidates[0], hardware_dir=candidates[0] / "Hardware")
    if len(candidates) > 1:
        names = ", ".join(c.name for c in candidates)
        raise ProjectDiscoveryError(
            f"Found multiple deployment namespaces with a Hardware/ directory ({names}). "
            "Run from inside the target deployment directory, or pass --deployment <name>."
        )

    raise ProjectDiscoveryError(
        f"No F' project found in {cwd}. Expected a 'Hardware/' directory here, or a "
        "'<Deployment>/Hardware/' directory one level down. Run this command from the "
        "root of an F' deployment project."
    )


def _find_one(candidates: list[Path], kind: str) -> Path:
    if not candidates:
        raise LinkerScriptError(f"No {kind} found") if kind == "linker script" else StartupScriptError(
            f"No {kind} found"
        )
    if len(candidates) > 1:
        listing = "\n".join(f"  - {c}" for c in candidates)
        message = f"Found multiple candidate {kind}s, expected exactly one:\n{listing}"
        raise LinkerScriptError(message) if kind == "linker script" else StartupScriptError(message)
    return candidates[0]


def find_cubemx_project(hardware_dir: Path) -> Path:
    """Find the single CubeMX project (a Hardware/ subdirectory with a CubeMX .ioc file or
    cmake/stm32cubemx/CMakeLists.txt) when the user doesn't name one."""
    candidates = sorted(
        p
        for p in hardware_dir.iterdir()
        if p.is_dir() and (any(p.glob("*.ioc")) or (p / "cmake" / "stm32cubemx" / "CMakeLists.txt").is_file())
    )
    if not candidates:
        raise ProjectDiscoveryError(
            f"No CubeMX project found under {hardware_dir}. Generate one into Hardware/<name>_hal/ "
            "or pass its path explicitly."
        )
    if len(candidates) > 1:
        names = ", ".join(c.name for c in candidates)
        raise ProjectDiscoveryError(
            f"Found multiple CubeMX projects under {hardware_dir} ({names}). Pass the one to sync explicitly."
        )
    return candidates[0]


def discover_cubemx_sources(cubemx_path: Path, family: Family) -> CubeMxSources:
    if not cubemx_path.is_dir():
        raise ProjectDiscoveryError(f"CubeMX project path does not exist or is not a directory: {cubemx_path}")

    linker_candidates = sorted(cubemx_path.glob("*FLASH.ld"))
    if not linker_candidates:
        linker_candidates = sorted(cubemx_path.glob("*.ld"))
    if not linker_candidates:
        linker_candidates = sorted(cubemx_path.glob("build/**/*.ld"))
    linker_script = _find_one(linker_candidates, "linker script")

    startup_candidates = sorted((cubemx_path / "Core" / "Startup").glob(family.startup_glob))
    if not startup_candidates:
        startup_candidates = sorted(cubemx_path.glob(family.startup_glob))
    startup_script = _find_one(startup_candidates, "startup assembly file")

    chip_match = re.search(family.chip_re, startup_script.stem, re.IGNORECASE)
    chip_tag = chip_match.group(0) if chip_match else startup_script.stem

    cubemx_cmake_file = cubemx_path / "cmake" / "stm32cubemx" / "CMakeLists.txt"
    if not cubemx_cmake_file.is_file():
        raise CubeMxCMakeError(
            f"No CubeMX-generated {cubemx_cmake_file} found. Regenerate this project in "
            "STM32CubeMX/STM32CubeIDE with Project Manager -> Project -> Toolchain/IDE set to "
            "'CMake' (not Makefile/IAR/Keil) - that option is what produces cmake/stm32cubemx/CMakeLists.txt."
        )

    return CubeMxSources(
        linker_script=linker_script,
        startup_script=startup_script,
        chip_tag=chip_tag,
        cubemx_cmake_file=cubemx_cmake_file,
    )


def find_root_cmakelists(namespace_root: Path) -> Path:
    """Locate the outer bootstrap root's CMakeLists.txt (the one with project(...) in it),
    one directory above the F' namespace root."""
    root_cmake = namespace_root.parent / "CMakeLists.txt"
    if not root_cmake.is_file():
        raise ProjectDiscoveryError(f"No root CMakeLists.txt found at {root_cmake}")
    if "project(" not in root_cmake.read_text():
        raise ProjectDiscoveryError(f"{root_cmake} has no project(...) call - not the project's root CMakeLists.txt")
    return root_cmake


def find_namespace_cmakelists(namespace_root: Path) -> Path:
    namespace_cmake = namespace_root / "CMakeLists.txt"
    if not namespace_cmake.is_file():
        raise ProjectDiscoveryError(f"No namespace CMakeLists.txt found at {namespace_cmake}")
    return namespace_cmake


def find_deployment_to_wire(namespace_root: Path, explicit_name: str | None) -> Path | None:
    """Find the F' Deployment directory (under Deployments/) whose CMakeLists.txt/Top/CMakeLists.txt
    should be wired for the stm32h7 platform. Returns None if none exist yet and none was requested.
    """
    deployments_dir = namespace_root / "Deployments"

    if explicit_name is not None:
        deployment_dir = deployments_dir / explicit_name
        if not deployment_dir.is_dir():
            raise ProjectDiscoveryError(f"--wire-deployment {explicit_name!r} not found (expected {deployment_dir})")
    else:
        if not deployments_dir.is_dir():
            return None
        candidates = sorted(p for p in deployments_dir.iterdir() if p.is_dir())
        if not candidates:
            return None
        if len(candidates) > 1:
            names = ", ".join(c.name for c in candidates)
            raise ProjectDiscoveryError(
                f"Found multiple deployments under {deployments_dir} ({names}). "
                "Pass --wire-deployment <name> to pick one."
            )
        deployment_dir = candidates[0]

    if not (deployment_dir / "CMakeLists.txt").is_file():
        raise ProjectDiscoveryError(f"{deployment_dir} has no CMakeLists.txt")
    if not (deployment_dir / "Top" / "CMakeLists.txt").is_file():
        raise ProjectDiscoveryError(f"{deployment_dir} has no Top/CMakeLists.txt")

    return deployment_dir

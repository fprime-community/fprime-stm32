import argparse
import difflib
import sys
from pathlib import Path

from fprime_stm32.cubemx_cmake import parse_cubemx_cmake
from fprime_stm32.discovery import (
    discover_cubemx_sources,
    find_cubemx_project,
    find_deployment_to_wire,
    find_fprime_project_root,
    find_namespace_cmakelists,
    find_root_cmakelists,
)
from fprime_stm32.errors import CliError, ProjectDiscoveryError
from fprime_stm32.family import AUTO, FAMILY_CHOICES, detect_family
from fprime_stm32.hardware_cmake import (
    discover_extra_config_sources,
    plan_hardware_config_scaffold,
    render_hardware_cmakelists,
)
from fprime_stm32.linker import patch_linker_script
from fprime_stm32.memory_model import parse_memory_block
from fprime_stm32.project_wiring import (
    ensure_deployment_registered,
    patch_deployment_cmakelists,
    patch_namespace_cmakelists,
    patch_root_cmakelists,
    patch_top_cmakelists,
)
from fprime_stm32.report import FileChange, SyncReport
from fprime_stm32.startup import patch_startup_script


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="fprime-stm32")
    subparsers = parser.add_subparsers(dest="command", required=True)

    sync = subparsers.add_parser(
        "sync", help="Port a CubeMX STM32H7/STM32F4 project's linker/startup files into an F' Hardware/ directory"
    )
    sync.add_argument(
        "cubemx_project",
        type=Path,
        nargs="?",
        default=None,
        help="Path to the STM32CubeMX project directory (auto-detected under Hardware/ if omitted)",
    )
    sync.add_argument(
        "--family",
        choices=FAMILY_CHOICES,
        default=AUTO,
        help="STM32 family to target; 'auto' (default) detects it from the CubeMX project files",
    )
    sync.add_argument(
        "--dry-run", action="store_true", help="Show the proposed changes without writing any files"
    )
    sync.add_argument(
        "--deployment",
        default=None,
        help="F' namespace directory name to target, when there's more than one Hardware/ directory",
    )
    sync.add_argument(
        "--wire-deployment",
        default=None,
        help="Deployment directory name (under Deployments/) to wire for the stm32h7 platform. "
        "Auto-detected if there's exactly one deployment.",
    )
    return parser


def _resolve_hal_dir_name(cwd: Path, cubemx_project: Path, hardware_dir: Path) -> str:
    cubemx_abs = (cubemx_project if cubemx_project.is_absolute() else cwd / cubemx_project).resolve()
    hardware_abs = hardware_dir.resolve()
    if cubemx_abs.parent != hardware_abs:
        raise ProjectDiscoveryError(
            f"{cubemx_abs} is not a direct subdirectory of {hardware_abs}. "
            "Create the CubeMX project under Hardware/<name>_hal/ (e.g. Hardware/stm32h743_hal/) "
            "and run sync against that directory."
        )
    return cubemx_abs.name


def _run_sync(
    cwd: Path,
    cubemx_project: Path | None,
    dry_run: bool,
    deployment: str | None,
    wire_deployment: str | None,
    family_name: str = AUTO,
) -> int:
    project = find_fprime_project_root(cwd, deployment=deployment)
    if cubemx_project is None:
        cubemx_project = find_cubemx_project(project.hardware_dir)
    family = detect_family(cubemx_project, family_name)
    hal_dir_name = _resolve_hal_dir_name(cwd, cubemx_project, project.hardware_dir)
    sources = discover_cubemx_sources(cubemx_project, family)

    linker_text = sources.linker_script.read_text()
    startup_text = sources.startup_script.read_text()
    cubemx_cmake_text = sources.cubemx_cmake_file.read_text()

    linker_out = project.hardware_dir / "linker" / sources.linker_script.name
    startup_out = project.hardware_dir / "startup" / sources.startup_script.name
    cmake_out = project.hardware_dir / "CMakeLists.txt"

    existing_linker_text = linker_out.read_text() if linker_out.exists() else None

    memory_map = parse_memory_block(linker_text, family.name)
    patched_linker, linker_actions = patch_linker_script(
        linker_text, memory_map, existing_output_text=existing_linker_text, family=family.name
    )
    patched_startup, startup_actions = patch_startup_script(startup_text, family.name)
    cubemx_info = parse_cubemx_cmake(cubemx_cmake_text)

    scaffold = plan_hardware_config_scaffold(project.hardware_dir, family)
    extra_config_sources = discover_extra_config_sources(project.hardware_dir, list(scaffold))
    patched_cmake = render_hardware_cmakelists(
        hal_dir_name=hal_dir_name,
        info=cubemx_info,
        linker_out_name=linker_out.name,
        startup_out_name=startup_out.name,
        extra_config_sources=extra_config_sources,
        hardware_dir=project.hardware_dir,
        family=family,
        scaffolded=list(scaffold),
    )
    cmake_actions = [
        f"Regenerated Hardware/CMakeLists.txt for chip {cubemx_info.chip_define} from {hal_dir_name}/",
        f"HAL sources: {len(cubemx_info.driver_sources)} HAL driver + "
        f"{len(cubemx_info.application_sources)} peripheral-init (from CubeMX) + "
        f"{len(extra_config_sources)} project-owned (config/src/)",
        "Exported FPRIME_STM32_LINKER_SCRIPT / FPRIME_STM32_STARTUP_SOURCE / "
        "FPRIME_STM32_IT_SOURCE cache variables for the deployment CMakeLists.txt to consume",
    ]

    # Project-wide build-graph wiring: guarantees Hardware/ and config/ are reachable from the
    # build, and (if a deployment exists) that it's linked against the detected platform/HAL/allocator.
    project_wiring = []

    root_cmake_path = find_root_cmakelists(project.namespace_root)
    namespace_cmake_path = find_namespace_cmakelists(project.namespace_root)
    # `fprime-util new` puts config/ inside the namespace directory; register it from whichever
    # CMakeLists.txt sits next to it (root if there's no namespace-level config/).
    config_in_namespace = (project.namespace_root / "config").is_dir()
    patched_root_cmake, root_actions = patch_root_cmakelists(
        root_cmake_path.read_text(), add_config=not config_in_namespace
    )
    patched_namespace_cmake, namespace_actions = patch_namespace_cmakelists(
        namespace_cmake_path.read_text(), add_config=config_in_namespace
    )

    deployment_dir = find_deployment_to_wire(project.namespace_root, wire_deployment)
    deployment_note = None
    if deployment_dir is None:
        deployment_note = (
            "No deployment wired: no Deployments/ found yet. Once you've created one, re-run "
            "sync (it auto-detects a single deployment) or pass --wire-deployment <name>."
        )
    else:
        patched_root_cmake, patched_namespace_cmake, extra_root_actions, extra_namespace_actions = (
            ensure_deployment_registered(
                patched_root_cmake, patched_namespace_cmake, project.namespace_root.name, deployment_dir.name
            )
        )
        root_actions += extra_root_actions
        namespace_actions += extra_namespace_actions

        deployment_cmake_path = deployment_dir / "CMakeLists.txt"
        top_cmake_path = deployment_dir / "Top" / "CMakeLists.txt"
        patched_deployment_cmake, deployment_actions = patch_deployment_cmakelists(deployment_cmake_path.read_text(), family.name)
        patched_top_cmake, top_actions = patch_top_cmakelists(top_cmake_path.read_text())
        project_wiring.append(
            FileChange(f"Deployment CMakeLists.txt ({deployment_dir.name})", deployment_cmake_path, deployment_actions)
        )
        project_wiring.append(FileChange(f"Top/CMakeLists.txt ({deployment_dir.name})", top_cmake_path, top_actions))

    project_wiring.insert(0, FileChange("Namespace CMakeLists.txt", namespace_cmake_path, namespace_actions))
    project_wiring.insert(0, FileChange("Root CMakeLists.txt", root_cmake_path, root_actions))

    report = SyncReport(
        chip_tag=sources.chip_tag,
        family=family.name,
        scaffolded=list(scaffold),
        memory_map=memory_map,
        linker_actions=linker_actions,
        startup_actions=startup_actions,
        cmake_actions=cmake_actions,
        linker_out=linker_out,
        startup_out=startup_out,
        cmake_out=cmake_out,
        dry_run=dry_run,
        project_wiring=project_wiring,
        deployment_note=deployment_note,
    )

    patched_texts = {
        linker_out: patched_linker,
        startup_out: patched_startup,
        cmake_out: patched_cmake,
        root_cmake_path: patched_root_cmake,
        namespace_cmake_path: patched_namespace_cmake,
    }
    for relative, content in scaffold.items():
        patched_texts[project.hardware_dir / relative] = content
    if deployment_dir is not None:
        patched_texts[deployment_cmake_path] = patched_deployment_cmake
        patched_texts[top_cmake_path] = patched_top_cmake

    if dry_run:
        for out_path, after_text in patched_texts.items():
            before_text = out_path.read_text() if out_path.exists() else ""
            diff = difflib.unified_diff(
                before_text.splitlines(keepends=True),
                after_text.splitlines(keepends=True),
                fromfile=str(out_path) if out_path.exists() else "(new file)",
                tofile=str(out_path),
            )
            diff_text = "".join(diff)
            if diff_text:
                print(f"--- diff for {out_path} ---")
                print(diff_text)
    else:
        for out_path, after_text in patched_texts.items():
            out_path.parent.mkdir(parents=True, exist_ok=True)
            out_path.write_text(after_text)

    print(report.render())
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)

    try:
        if args.command == "sync":
            return _run_sync(Path.cwd(), args.cubemx_project, args.dry_run, args.deployment, args.wire_deployment, args.family)
    except CliError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    parser.error(f"unknown command: {args.command}")
    return 2


if __name__ == "__main__":
    sys.exit(main())

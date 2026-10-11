import shutil
from pathlib import Path

import pytest

from fprime_stm32.cli import main

FIXTURE_DIR = Path(__file__).parent / "fixtures" / "cubemx_stm32h753"

ROOT_CMAKELISTS = """cmake_minimum_required(VERSION 3.24.2)
project(MyDeployment C CXX)

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/MyDeployment")
"""

NAMESPACE_CMAKELISTS = """add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Components")
"""

DEPLOYMENT_CMAKELISTS = """add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Top/")
register_fprime_deployment(
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Main.cpp"
    DEPENDS
        ${FPRIME_CURRENT_MODULE}_Top
)
"""

TOP_CMAKELISTS = """register_fprime_module(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/instances.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Topology.cpp"
    DEPENDS
        Fw_Logger
)
"""


@pytest.fixture
def fprime_project(tmp_path, monkeypatch):
    deployment = tmp_path / "MyDeployment"
    (deployment / "Hardware").mkdir(parents=True)
    (tmp_path / "CMakeLists.txt").write_text(ROOT_CMAKELISTS)
    (deployment / "CMakeLists.txt").write_text(NAMESPACE_CMAKELISTS)
    monkeypatch.chdir(deployment)
    return deployment


def _add_target_deployment(namespace_root, name):
    target = namespace_root / "Deployments" / name
    (target / "Top").mkdir(parents=True)
    (target / "CMakeLists.txt").write_text(DEPLOYMENT_CMAKELISTS)
    (target / "Top" / "CMakeLists.txt").write_text(TOP_CMAKELISTS)
    return target


@pytest.fixture
def cubemx_project(fprime_project):
    dest = fprime_project / "Hardware" / "cubemx_stm32h753"
    shutil.copytree(FIXTURE_DIR, dest)
    return dest


def test_sync_writes_linker_startup_and_cmake(fprime_project, cubemx_project):
    exit_code = main(["sync", str(cubemx_project)])

    assert exit_code == 0
    linker_out = fprime_project / "Hardware" / "linker" / "STM32H753xx_FLASH.ld"
    startup_out = fprime_project / "Hardware" / "startup" / "startup_stm32h753xx.s"
    cmake_out = fprime_project / "Hardware" / "CMakeLists.txt"
    assert linker_out.exists()
    assert startup_out.exists()
    assert cmake_out.exists()
    assert "AXI_SRAM" in linker_out.read_text()
    assert "_sdtcm_bss" in startup_out.read_text()

    cmake_text = cmake_out.read_text()
    assert "cubemx_stm32h753/Core/Src/gpio.c" in cmake_text
    assert "STM32H753xx" in cmake_text
    assert 'set(FPRIME_STM32_LINKER_SCRIPT' in cmake_text
    assert "linker/STM32H753xx_FLASH.ld" in cmake_text
    assert "startup/startup_stm32h753xx.s" in cmake_text
    assert "cubemx_stm32h753/Core/Src/stm32h7xx_it.c" in cmake_text  # via FPRIME_STM32_IT_SOURCE


def test_sync_wires_root_and_namespace_cmakelists(fprime_project, cubemx_project):
    exit_code = main(["sync", str(cubemx_project)])

    assert exit_code == 0
    root_text = (fprime_project.parent / "CMakeLists.txt").read_text()
    assert "enable_language(ASM)" in root_text
    assert 'add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/config")' in root_text

    namespace_text = (fprime_project / "CMakeLists.txt").read_text()
    assert 'add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Hardware")' in namespace_text


def test_sync_auto_detects_single_deployment(fprime_project, cubemx_project):
    deployment_dir = _add_target_deployment(fprime_project, "Stm32h7Deployment")

    exit_code = main(["sync", str(cubemx_project)])

    assert exit_code == 0
    deployment_text = (deployment_dir / "CMakeLists.txt").read_text()
    assert "restrict_platforms(stm32h7)" in deployment_text
    assert "FprimeStm32Allocator" in deployment_text
    top_text = (deployment_dir / "Top" / "CMakeLists.txt").read_text()
    assert "FprimeStm32Allocator" in top_text


def test_sync_skips_deployment_wiring_when_none_exist(fprime_project, cubemx_project, capsys):
    exit_code = main(["sync", str(cubemx_project)])

    assert exit_code == 0
    captured = capsys.readouterr()
    assert "No deployment wired" in captured.out


def test_sync_requires_flag_when_multiple_deployments(fprime_project, cubemx_project, capsys):
    _add_target_deployment(fprime_project, "DeploymentA")
    _add_target_deployment(fprime_project, "DeploymentB")

    exit_code = main(["sync", str(cubemx_project)])

    assert exit_code == 1
    captured = capsys.readouterr()
    assert "multiple deployments" in captured.err
    assert "--wire-deployment" in captured.err


def test_sync_wire_deployment_flag_selects_explicit_target(fprime_project, cubemx_project):
    _add_target_deployment(fprime_project, "DeploymentA")
    deployment_b = _add_target_deployment(fprime_project, "DeploymentB")

    exit_code = main(["sync", str(cubemx_project), "--wire-deployment", "DeploymentB"])

    assert exit_code == 0
    assert "restrict_platforms(stm32h7)" in (deployment_b / "CMakeLists.txt").read_text()


def test_dry_run_writes_nothing(fprime_project, cubemx_project):
    root_before = (fprime_project.parent / "CMakeLists.txt").read_text()
    namespace_before = (fprime_project / "CMakeLists.txt").read_text()

    exit_code = main(["sync", str(cubemx_project), "--dry-run"])

    assert exit_code == 0
    assert not (fprime_project / "Hardware" / "linker").exists()
    assert not (fprime_project / "Hardware" / "startup").exists()
    assert not (fprime_project / "Hardware" / "CMakeLists.txt").exists()
    assert (fprime_project.parent / "CMakeLists.txt").read_text() == root_before
    assert (fprime_project / "CMakeLists.txt").read_text() == namespace_before


def test_fails_cleanly_outside_fprime_project(tmp_path, monkeypatch, cubemx_project, capsys):
    empty_dir = tmp_path / "not_an_fprime_project"
    empty_dir.mkdir()
    monkeypatch.chdir(empty_dir)

    exit_code = main(["sync", str(cubemx_project)])

    assert exit_code == 1
    captured = capsys.readouterr()
    assert "No F' project found" in captured.err


def test_fails_when_cubemx_project_not_under_hardware(fprime_project, tmp_path, capsys):
    outside_project = tmp_path / "cubemx_stm32h753_outside"
    shutil.copytree(FIXTURE_DIR, outside_project)

    exit_code = main(["sync", str(outside_project)])

    assert exit_code == 1
    captured = capsys.readouterr()
    assert "not a direct subdirectory of" in captured.err


F4_FIXTURE_DIR = Path(__file__).parent / "fixtures" / "cubemx_stm32f401"


@pytest.fixture
def f4_cubemx_project(fprime_project):
    dest = fprime_project / "Hardware" / "cubemx_stm32f401"
    shutil.copytree(F4_FIXTURE_DIR, dest)
    return dest


def test_f4_sync_auto_detects_family_and_wires_everything(fprime_project, f4_cubemx_project):
    deployment_dir = _add_target_deployment(fprime_project, "Stm32f4Deployment")

    exit_code = main(["sync", str(f4_cubemx_project)])

    assert exit_code == 0
    hardware = fprime_project / "Hardware"
    assert "_sstack = _estack - _Min_Stack_Size;" in (hardware / "linker" / "STM32F401XX_FLASH.ld").read_text()
    assert "bl Stm32_registerBootstrapAllocator" in (hardware / "startup" / "startup_stm32f401xe.s").read_text()

    cmake_text = (hardware / "CMakeLists.txt").read_text()
    assert 'FPRIME_PLATFORM STREQUAL "stm32f4"' in cmake_text
    assert "PlatformStm32f4" in cmake_text
    assert "config/src/stm32f4_clock.c" in cmake_text  # scaffolded from the family template
    assert (hardware / "config" / "src" / "stm32f4_clock.c").exists()
    assert (hardware / "config" / "include" / "PlatformMemory.hpp").exists()

    assert "restrict_platforms(stm32f4)" in (deployment_dir / "CMakeLists.txt").read_text()


def test_sync_auto_finds_cubemx_project_when_path_omitted(fprime_project, f4_cubemx_project):
    exit_code = main(["sync"])

    assert exit_code == 0
    assert (fprime_project / "Hardware" / "linker" / "STM32F401XX_FLASH.ld").exists()


def test_family_flag_contradicting_project_fails(fprime_project, f4_cubemx_project, capsys):
    exit_code = main(["sync", str(f4_cubemx_project), "--family", "stm32h7"])

    assert exit_code == 1
    assert "looks like stm32f4" in capsys.readouterr().err
    assert not (fprime_project / "Hardware" / "linker").exists()


def test_sync_does_not_overwrite_existing_config_glue(fprime_project, f4_cubemx_project):
    glue = fprime_project / "Hardware" / "config" / "src" / "stm32f4_clock.c"
    glue.parent.mkdir(parents=True)
    glue.write_text("// board specific\n")

    assert main(["sync", str(f4_cubemx_project)]) == 0

    assert glue.read_text() == "// board specific\n"


def test_config_registered_in_namespace_when_it_lives_there(fprime_project, cubemx_project):
    (fprime_project / "config").mkdir()

    assert main(["sync", str(cubemx_project)]) == 0

    assert "/config" in (fprime_project / "CMakeLists.txt").read_text()
    assert "/config" not in (fprime_project.parent / "CMakeLists.txt").read_text()

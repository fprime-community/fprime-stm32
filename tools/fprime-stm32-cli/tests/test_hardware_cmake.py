from pathlib import Path

from fprime_stm32.cubemx_cmake import parse_cubemx_cmake
from fprime_stm32.family import get_family
from fprime_stm32.hardware_cmake import discover_extra_config_sources, render_hardware_cmakelists

CMAKE_FIXTURE = Path(__file__).parent / "fixtures" / "cubemx_stm32h753" / "cmake" / "stm32cubemx" / "CMakeLists.txt"


def _render(hardware_dir, extra_config_sources=()):
    info = parse_cubemx_cmake(CMAKE_FIXTURE.read_text())
    return render_hardware_cmakelists(
        hal_dir_name="stm32h743_hal",
        info=info,
        linker_out_name="STM32H743xx_FLASH.ld",
        startup_out_name="startup_stm32h743xx.s",
        extra_config_sources=list(extra_config_sources),
        hardware_dir=hardware_dir,
        family=get_family("stm32h7"),
    )


def test_hal_sources_prefixed_with_hal_dir_name(tmp_path):
    text = _render(tmp_path)
    assert "stm32h743_hal/Core/Src/gpio.c" in text
    assert "stm32h743_hal/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_uart.c" in text
    # it_source is linked directly into the deployment, not the archive - it must not appear
    # as an unquoted HAL_SOURCES list entry (only inside the quoted FPRIME_STM32_IT_SOURCE line).
    assert "        stm32h743_hal/Core/Src/stm32h7xx_it.c\n" not in text


def test_extra_config_sources_included_verbatim(tmp_path):
    text = _render(tmp_path, extra_config_sources=["config/src/stm32h7_clock.c"])
    assert "config/src/stm32h7_clock.c" in text


def test_cache_variables_present(tmp_path):
    text = _render(tmp_path)
    assert 'set(FPRIME_STM32_LINKER_SCRIPT\n        "${CMAKE_CURRENT_LIST_DIR}/linker/STM32H743xx_FLASH.ld" CACHE INTERNAL "")' in text
    assert (
        'set(FPRIME_STM32_STARTUP_SOURCE\n        "${CMAKE_CURRENT_LIST_DIR}/startup/startup_stm32h743xx.s" CACHE INTERNAL "")'
        in text
    )
    assert (
        'set(FPRIME_STM32_IT_SOURCE\n        "${CMAKE_CURRENT_LIST_DIR}/stm32h743_hal/Core/Src/stm32h7xx_it.c" CACHE INTERNAL "")'
        in text
    )


def test_config_include_added_only_if_present(tmp_path):
    without = _render(tmp_path)
    assert "config/include" not in without

    (tmp_path / "config" / "include").mkdir(parents=True)
    with_include = _render(tmp_path)
    assert '"${CMAKE_CURRENT_LIST_DIR}/config/include"' in with_include


def test_discover_extra_config_sources(tmp_path):
    assert discover_extra_config_sources(tmp_path) == []

    src_dir = tmp_path / "config" / "src"
    src_dir.mkdir(parents=True)
    (src_dir / "stm32h7_clock.c").write_text("")
    (src_dir / "tim2_clock.cpp").write_text("")

    assert discover_extra_config_sources(tmp_path) == [
        "config/src/stm32h7_clock.c",
        "config/src/tim2_clock.cpp",
    ]


def test_f4_template_uses_f4_platform(tmp_path):
    f4_fixture = Path(__file__).parent / "fixtures" / "cubemx_stm32f401" / "cmake" / "stm32cubemx" / "CMakeLists.txt"
    text = render_hardware_cmakelists(
        hal_dir_name="stm32f401_hal",
        info=parse_cubemx_cmake(f4_fixture.read_text()),
        linker_out_name="STM32F401XX_FLASH.ld",
        startup_out_name="startup_stm32f401xe.s",
        extra_config_sources=[],
        hardware_dir=tmp_path,
        family=get_family("stm32f4"),
    )
    assert 'FPRIME_PLATFORM STREQUAL "stm32f4"' in text
    assert "PlatformStm32f4" in text
    assert "STM32F401xE" in text
    assert "$<" not in text
    assert "stm32f401_hal/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c" in text
    assert "stm32f401_hal/Core/Src/system_stm32f4xx.c" in text

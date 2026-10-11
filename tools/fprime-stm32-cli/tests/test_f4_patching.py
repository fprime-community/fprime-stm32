import re
from pathlib import Path

import pytest

from fprime_stm32.errors import LinkerScriptError
from fprime_stm32.linker import patch_linker_script
from fprime_stm32.memory_model import parse_memory_block
from fprime_stm32.startup import patch_startup_script

F4 = Path(__file__).parent / "fixtures" / "cubemx_stm32f401"
LINKER = F4 / "STM32F401XX_FLASH.ld"
STARTUP = F4 / "startup_stm32f401xe.s"


def test_f4_memory_map():
    memory_map = parse_memory_block(LINKER.read_text(), "stm32f4")

    assert memory_map.flash.length_bytes == 512 * 1024
    assert memory_map.ram.origin == 0x20000000
    assert memory_map.ram.length_bytes == 96 * 1024
    assert memory_map.dtcm is None and memory_map.axi_sram is None
    assert memory_map.warnings == []


def test_f4_linker_patch():
    text = LINKER.read_text()
    patched, actions = patch_linker_script(text, parse_memory_block(text, "stm32f4"), family="stm32f4")

    assert re.search(r"_Min_Stack_Size\s*=\s*0x1000;", patched)
    assert "_sstack = _estack - _Min_Stack_Size;" in patched
    assert re.search(r"\.bss \(NOLOAD\) :", patched)
    assert re.search(r"\._user_heap_stack \(NOLOAD\) :", patched)
    assert "DTCM_RAM" not in patched and "AXI_SRAM" not in patched
    assert actions


def test_f4_linker_patch_keeps_larger_stack():
    text = LINKER.read_text().replace("_Min_Stack_Size = 0x400;", "_Min_Stack_Size = 0x2000;")
    patched, _ = patch_linker_script(text, parse_memory_block(text, "stm32f4"), family="stm32f4")
    assert re.search(r"_Min_Stack_Size\s*=\s*0x2000;", patched)


def test_f4_linker_patch_rejects_unrecognized_template():
    with pytest.raises(LinkerScriptError):
        patch_linker_script("MEMORY { }", None, family="stm32f4")


def test_f4_startup_patch():
    patched, actions = patch_startup_script(STARTUP.read_text(), "stm32f4")

    assert patched.count("bl Stm32_registerBootstrapAllocator") == 1
    assert "_sdtcm_bss" not in patched
    assert patched.index("Stm32_registerBootstrapAllocator") < patched.index("__libc_init_array")
    assert ".fpu fpv4-sp-d16" in patched and "softvfp" not in patched
    assert "LoopForever:" in patched
    assert len(actions) == 3


def test_f4_startup_patch_idempotent():
    once, _ = patch_startup_script(STARTUP.read_text(), "stm32f4")
    twice, actions = patch_startup_script(once, "stm32f4")
    assert once == twice
    assert actions == ["startup script already patched, skipped"]

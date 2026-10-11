import re

from fprime_stm32.errors import StartupScriptError

_ZERO_BSS_LOOP_RE = re.compile(
    r"LoopFillZero[Bb]ss:\s*\n\s*cmp\s+r2,\s*r4\s*\n\s*bcc\s+FillZero[Bb]ss\b"
)

_DTCM_BSS_ZERO_BLOCK = """
  ldr r2, =_sdtcm_bss
  ldr r4, =_edtcm_bss
  movs r3, #0
  b LoopFillZeroDtcmBss

FillZeroDtcmBss:
  str r3, [r2]
  adds r2, r2, #4

LoopFillZeroDtcmBss:
  cmp r2, r4
  bcc FillZeroDtcmBss

/* Register the fixed allocator before any C++ constructor can call new. */
  bl Stm32_registerBootstrapAllocator
"""


_ALLOCATOR_HOOK = """
/* Register the fixed allocator before C++ constructors can allocate. */
  bl Stm32_registerBootstrapAllocator
"""

_MAIN_RETURN_RE = re.compile(r"(bl\s+main\s*\n)[ \t]*bx[ \t]+lr[ \t]*")
_SOFT_FPU_RE = re.compile(r"^([ \t]*\.fpu[ \t]+)softvfp\b", re.MULTILINE)


def patch_startup_script(text: str, family: str = "stm32h7") -> tuple[str, list[str]]:
    if family == "stm32f4":
        return _patch_f4_startup_script(text)
    return _patch_h7_startup_script(text)


def _patch_f4_startup_script(text: str) -> tuple[str, list[str]]:
    """F4 has no DTCM, so only the allocator hook is needed, plus Cortex-M4F fixups."""
    if "Stm32_registerBootstrapAllocator" in text:
        return text, ["startup script already patched, skipped"]

    match = _ZERO_BSS_LOOP_RE.search(text)
    if match is None:
        raise StartupScriptError(
            "Could not find the standard CubeMX .bss zero-fill loop "
            "(LoopFillZero{B,b}ss / FillZero{B,b}ss) - unrecognized startup script template"
        )

    patched = text[: match.end()] + "\n" + _ALLOCATOR_HOOK + text[match.end() :]
    actions = [
        "Inserted call to Stm32_registerBootstrapAllocator before C++ static constructors "
        "(requires linking fprime-stm32's Allocator/BootstrapAllocator.cpp)"
    ]

    patched, n = _SOFT_FPU_RE.subn(r"\1fpv4-sp-d16", patched)
    if n:
        actions.append("Set .fpu to fpv4-sp-d16 (the F4 toolchain builds hard-float)")

    patched, n = _MAIN_RETURN_RE.subn(r"\1LoopForever:\n  b LoopForever", patched)
    if n:
        actions.append("Replaced the return from main with an infinite loop")

    return patched, actions


def _patch_h7_startup_script(text: str) -> tuple[str, list[str]]:
    if "_sdtcm_bss" in text:
        return text, ["startup script already patched, skipped"]

    match = _ZERO_BSS_LOOP_RE.search(text)
    if match is None:
        raise StartupScriptError(
            "Could not find the standard CubeMX .bss zero-fill loop "
            "(LoopFillZero{B,b}ss / FillZero{B,b}ss) - unrecognized startup script template"
        )

    patched = text[: match.end()] + _DTCM_BSS_ZERO_BLOCK + text[match.end() :]
    actions = [
        "Inserted .dtcm_bss zero-fill loop after the standard .bss zero-fill loop",
        "Inserted call to Stm32_registerBootstrapAllocator before C++ static constructors "
        "(requires linking fprime-stm32's Allocator/BootstrapAllocator.cpp)",
    ]
    return patched, actions

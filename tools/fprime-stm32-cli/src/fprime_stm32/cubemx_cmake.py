import re
from dataclasses import dataclass

from fprime_stm32.errors import CubeMxCMakeError

# CubeMX emits either ${CMAKE_CURRENT_SOURCE_DIR}/../../ (older) or ${CMAKE_SOURCE_DIR}/ (newer).
_PATH_PREFIX_RE = re.compile(r"^\$\{CMAKE_(?:CURRENT_)?SOURCE_DIR\}/(?:\.\./\.\./)?")

_EXCLUDED_APPLICATION_BASENAMES = {"main.c", "sysmem.c", "syscalls.c"}

_SET_BLOCK_RE = {
    name: re.compile(rf"set\(\s*{name}\b(.*?)\)", re.DOTALL)
    for name in ("MX_Defines_Syms", "MX_Include_Dirs", "MX_Application_Src", "STM32_Drivers_Src")
}


@dataclass
class CubeMxCMakeInfo:
    chip_define: str
    all_defines: list[str]
    include_dirs: list[str]
    application_sources: list[str]
    it_source: str
    driver_sources: list[str]


def _extract_tokens(text: str, var_name: str) -> list[str]:
    match = _SET_BLOCK_RE[var_name].search(text)
    if match is None:
        raise CubeMxCMakeError(
            f"Could not find 'set({var_name} ...)' in cmake/stm32cubemx/CMakeLists.txt "
            "(unrecognized CubeMX CMake template)"
        )
    return match.group(1).split()


def _strip_path_prefix(token: str) -> str:
    return _PATH_PREFIX_RE.sub("", token)


def parse_cubemx_cmake(text: str) -> CubeMxCMakeInfo:
    # Drop CMake generator expressions (e.g. $<$<CONFIG:Debug>:DEBUG>): F' picks its own build type.
    all_defines = [t for t in _extract_tokens(text, "MX_Defines_Syms") if not t.startswith("$<")]
    include_dirs = [_strip_path_prefix(t) for t in _extract_tokens(text, "MX_Include_Dirs")]
    application_tokens = [_strip_path_prefix(t) for t in _extract_tokens(text, "MX_Application_Src")]
    driver_sources = [_strip_path_prefix(t) for t in _extract_tokens(text, "STM32_Drivers_Src")]

    chip_candidates = [d for d in all_defines if re.fullmatch(r"STM32\w+", d)]
    if len(chip_candidates) != 1:
        raise CubeMxCMakeError(
            f"Expected exactly one STM32 chip define in MX_Defines_Syms, found: {chip_candidates}"
        )
    chip_define = chip_candidates[0]

    it_sources = [t for t in application_tokens if re.search(r"_it\.c$", t)]
    if len(it_sources) != 1:
        raise CubeMxCMakeError(
            f"Expected exactly one '*_it.c' source in MX_Application_Src, found: {it_sources}"
        )
    it_source = it_sources[0]

    application_sources = [
        t
        for t in application_tokens
        if t != it_source and not t.endswith(".s") and t.rsplit("/", 1)[-1] not in _EXCLUDED_APPLICATION_BASENAMES
    ]

    return CubeMxCMakeInfo(
        chip_define=chip_define,
        all_defines=all_defines,
        include_dirs=include_dirs,
        application_sources=application_sources,
        it_source=it_source,
        driver_sources=driver_sources,
    )

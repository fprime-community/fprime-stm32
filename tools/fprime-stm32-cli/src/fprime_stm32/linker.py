import re

from fprime_stm32.errors import LinkerScriptError
from fprime_stm32.memory_model import MemoryMap

def _render_reserved_section(section_name, region_name, start_symbol, end_symbol, extra_wildcards):
    """Render a `.section_name (NOLOAD)` block with the standard `*(.section_name[.*])`
    wildcards plus any project-added `extra_wildcards` (e.g. per-symbol placement rules
    carried forward from an already-synced output file, see _extract_extra_wildcards).
    """
    lines = [
        f".{section_name} (NOLOAD) : ALIGN(32)",
        "{",
        "  . = ALIGN(32);",
        f"  {start_symbol} = .;",
        "",
        f"  *(.{section_name})",
        f"  *(.{section_name}.*)",
        *(f"  {w}" for w in extra_wildcards),
        "",
        "  . = ALIGN(32);",
        f"  {end_symbol} = .;",
        f"}} >{region_name}",
        "",
    ]
    return "\n".join(lines) + "\n"


def _extract_extra_wildcards(existing_text: str, section_name: str, standard_patterns: set[str]) -> list[str]:
    """Find any `*(...)` input-section wildcards already present in an existing output
    file's `.section_name (NOLOAD) { ... }` block, beyond the two standard ones this tool
    always emits. These are hand-added, project-specific placement rules (e.g. pinning a
    specific mangled symbol's `-fdata-sections` section into DTCM/AXI SRAM) that a full
    regeneration must not silently drop.
    """
    match = re.search(rf"\.{section_name}\s*\(NOLOAD\)[^{{]*\{{(.*?)\}}\s*>\s*\w+", existing_text, re.DOTALL)
    if match is None:
        return []
    extras = []
    for line in match.group(1).splitlines():
        stripped = line.strip()
        if stripped.startswith("*(") and stripped not in standard_patterns:
            extras.append(stripped)
    return extras


_DATA_TDATA_RETARGET_RE = re.compile(
    r"^([ \t]*\.(?:data|tdata)\b.*?\{.*?\})\s*>\s*DTCM_RAM\s*AT>\s*FLASH",
    re.DOTALL | re.MULTILINE,
)
_TBSS_BSS_RETARGET_RE = re.compile(
    r"^([ \t]*\.(?:tbss|bss)\b.*?\{.*?\})\s*>\s*DTCM_RAM\b(?!\s*AT>)",
    re.DOTALL | re.MULTILINE,
)
_PLAIN_BSS_HEADER_RE = re.compile(r"^([ \t]*)\.bss\b", re.MULTILINE)
_DISCARD_RE = re.compile(r"^[ \t]*/DISCARD/", re.MULTILINE)


def _rename_region(text: str, old_name: str, new_name: str) -> str:
    """Rename a MEMORY region, touching only linker-script syntax (never comments/prose):
    the MEMORY{} table entry, ORIGIN()/LENGTH() calls, and '>NAME' output-section selectors.
    """
    escaped = re.escape(old_name)
    text = re.sub(rf"^(?P<indent>[ \t]*){escaped}(?=[ \t]*\()", rf"\g<indent>{new_name}", text, flags=re.MULTILINE)
    text = re.sub(rf"\b(ORIGIN|LENGTH)\(\s*{escaped}\s*\)", rf"\1({new_name})", text)
    text = re.sub(rf">(\s*){escaped}\b", rf">\1{new_name}", text)
    return text


def patch_linker_script(
    text: str, memory_map: MemoryMap, existing_output_text: str | None = None, family: str = "stm32h7"
) -> tuple[str, list[str]]:
    if family == "stm32f4":
        return _patch_f4_linker_script(text)
    return _patch_h7_linker_script(text, memory_map, existing_output_text)


_F4_MIN_STACK_BYTES = 0x1000
_MIN_STACK_RE = re.compile(r"^(_Min_Stack_Size\s*=\s*)(0[xX][0-9a-fA-F]+|\d+)\s*;[^\n]*", re.MULTILINE)
_NOLOAD_TARGET_RE = re.compile(r"^([ \t]*\.(?:bss|_user_heap_stack))[ \t]*:", re.MULTILINE)


def _patch_f4_linker_script(text: str) -> tuple[str, list[str]]:
    """F4 has one contiguous, DMA-visible SRAM, so no region remap is needed. The script only
    needs a bare-metal-sized MSP stack (with an `_sstack` symbol) and NOLOAD .bss/heap sections.
    """
    actions: list[str] = []
    patched = text

    stack_match = _MIN_STACK_RE.search(patched)
    if stack_match is None:
        raise LinkerScriptError("Could not find '_Min_Stack_Size' in the linker script (unrecognized template)")
    current = int(stack_match.group(2), 0)
    stack_size = max(current, _F4_MIN_STACK_BYTES)
    replacement = (
        f"{stack_match.group(1)}0x{stack_size:X};    /* reserve for the bare-metal MSP stack */"
    )
    if "_sstack" not in patched:
        replacement += "\n_sstack = _estack - _Min_Stack_Size;"
        actions.append("Defined _sstack = _estack - _Min_Stack_Size")
    patched = patched[: stack_match.start()] + replacement + patched[stack_match.end() :]
    if stack_size != current:
        actions.append(f"Raised _Min_Stack_Size from 0x{current:X} to 0x{stack_size:X}")

    patched, n = _NOLOAD_TARGET_RE.subn(r"\1 (NOLOAD) :", patched)
    if n == 0:
        raise LinkerScriptError("Could not find the .bss/._user_heap_stack output sections (unrecognized template)")
    actions.append(f"Marked {n} section(s) (.bss/._user_heap_stack) NOLOAD")

    return patched, actions


def _patch_h7_linker_script(
    text: str, memory_map: MemoryMap, existing_output_text: str | None = None
) -> tuple[str, list[str]]:
    actions: list[str] = []

    dtcm_old = memory_map.dtcm.name
    axi_old = memory_map.axi_sram.name

    patched = text
    if dtcm_old != "DTCM_RAM":
        patched = _rename_region(patched, dtcm_old, "DTCM_RAM")
        actions.append(f"Renamed memory region {dtcm_old} -> DTCM_RAM")
    if axi_old != "AXI_SRAM":
        patched = _rename_region(patched, axi_old, "AXI_SRAM")
        actions.append(f"Renamed memory region {axi_old} -> AXI_SRAM")

    patched, n = _DATA_TDATA_RETARGET_RE.subn(r"\1 >AXI_SRAM AT> FLASH", patched)
    if n == 0:
        raise LinkerScriptError(
            "Could not find the .data/.tdata output sections targeting DTCM RAM "
            "(unrecognized linker script template) - refusing to patch"
        )
    actions.append(f"Retargeted {n} initialized-data section(s) (.data/.tdata) to AXI_SRAM")

    patched, n = _TBSS_BSS_RETARGET_RE.subn(r"\1 >AXI_SRAM", patched)
    if n == 0:
        raise LinkerScriptError(
            "Could not find the .tbss/.bss output sections targeting DTCM RAM "
            "(unrecognized linker script template) - refusing to patch"
        )
    actions.append(f"Retargeted {n} zero-initialized section(s) (.tbss/.bss) to AXI_SRAM")

    dtcm_extras = []
    axi_extras = []
    if existing_output_text is not None:
        dtcm_extras = _extract_extra_wildcards(existing_output_text, "dtcm_bss", {"*(.dtcm_bss)", "*(.dtcm_bss.*)"})
        axi_extras = _extract_extra_wildcards(existing_output_text, "axi_sram", {"*(.axi_sram)", "*(.axi_sram.*)"})

    header_match = _PLAIN_BSS_HEADER_RE.search(patched)
    if header_match is None:
        raise LinkerScriptError("Could not find the .bss output section to anchor .dtcm_bss insertion")
    indent = header_match.group(1)
    dtcm_text = _render_reserved_section("dtcm_bss", "DTCM_RAM", "_sdtcm_bss", "_edtcm_bss", dtcm_extras)
    dtcm_block = "\n".join(
        (indent + line) if line else line for line in dtcm_text.rstrip("\n").split("\n")
    ) + "\n\n"
    patched = patched[: header_match.start()] + dtcm_block + patched[header_match.start() :]
    actions.append("Inserted .dtcm_bss NOLOAD section (DTCM_RAM) with _sdtcm_bss/_edtcm_bss")
    if dtcm_extras:
        actions.append(
            f"Preserved {len(dtcm_extras)} custom .dtcm_bss placement rule(s) from the "
            f"existing linker script: {', '.join(dtcm_extras)}"
        )

    axi_text = _render_reserved_section("axi_sram", "AXI_SRAM", "_saxi_sram", "_eaxi_sram", axi_extras)
    axi_block = "\n".join(
        (indent + line) if line else line for line in axi_text.rstrip("\n").split("\n")
    ) + "\n\n"
    discard_match = _DISCARD_RE.search(patched)
    if discard_match is not None:
        insert_at = patched.rfind("\n", 0, discard_match.start()) + 1
        patched = patched[:insert_at] + axi_block + patched[insert_at:]
    else:
        last_brace = patched.rstrip().rfind("}")
        patched = patched[:last_brace] + axi_block + patched[last_brace:]
    actions.append("Appended .axi_sram NOLOAD section (AXI_SRAM) with _saxi_sram/_eaxi_sram")
    if axi_extras:
        actions.append(
            f"Preserved {len(axi_extras)} custom .axi_sram placement rule(s) from the "
            f"existing linker script: {', '.join(axi_extras)}"
        )

    return patched, actions

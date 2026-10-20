import re
from dataclasses import dataclass

from fprime_stm32.errors import LinkerScriptError

# Memory maps are fixed by silicon, regardless of what CubeMX names the regions.
AXI_SRAM_ORIGIN = 0x24000000
DTCM_RAM_ORIGIN = 0x20000000
MIN_AXI_SRAM_BYTES = 128 * 1024

F4_SRAM_ORIGIN = 0x20000000
MIN_F4_SRAM_BYTES = 64 * 1024

_MEMORY_BLOCK_RE = re.compile(r"MEMORY\s*\{(?P<body>.*?)\}", re.DOTALL)
_REGION_LINE_RE = re.compile(
    r"^\s*(?P<name>\w+)\s*\((?P<attrs>[^)]*)\)\s*:\s*"
    r"ORIGIN\s*=\s*(?P<origin>0[xX][0-9a-fA-F]+)\s*,\s*"
    r"LENGTH\s*=\s*(?P<length>\d+)\s*(?P<unit>[KkMm]?)\s*$",
    re.MULTILINE,
)

_UNIT_MULTIPLIERS = {"": 1, "k": 1024, "m": 1024 * 1024}


@dataclass
class MemoryRegion:
    name: str
    attrs: str
    origin: int
    length_bytes: int


@dataclass
class MemoryMap:
    regions: list[MemoryRegion]
    flash: MemoryRegion
    warnings: list[str]
    dtcm: MemoryRegion | None = None  # H7 only
    axi_sram: MemoryRegion | None = None  # H7 only
    ram: MemoryRegion | None = None  # F4: the single contiguous SRAM

    def region_named(self, name: str) -> MemoryRegion | None:
        for region in self.regions:
            if region.name == name:
                return region
        return None


def parse_memory_block(linker_text: str, family: str = "stm32h7") -> MemoryMap:
    block = _MEMORY_BLOCK_RE.search(linker_text)
    if block is None:
        raise LinkerScriptError("No MEMORY { ... } block found in linker script")

    regions = []
    for match in _REGION_LINE_RE.finditer(block.group("body")):
        unit = match.group("unit").lower()
        length_bytes = int(match.group("length")) * _UNIT_MULTIPLIERS[unit]
        regions.append(
            MemoryRegion(
                name=match.group("name"),
                attrs=match.group("attrs"),
                origin=int(match.group("origin"), 16),
                length_bytes=length_bytes,
            )
        )

    if not regions:
        raise LinkerScriptError("MEMORY block found but no memory regions could be parsed from it")

    flash = next((r for r in regions if r.name.upper().startswith("FLASH")), None)
    if flash is None:
        raise LinkerScriptError("MEMORY block has no FLASH region")

    if family == "stm32f4":
        ram = next((r for r in regions if r.origin == F4_SRAM_ORIGIN), None)
        if ram is None:
            raise LinkerScriptError(
                f"MEMORY block has no region at SRAM origin 0x{F4_SRAM_ORIGIN:08X} "
                "(not an STM32F4 CubeMX linker script?)"
            )
        warnings = []
        if ram.length_bytes < MIN_F4_SRAM_BYTES:
            warnings.append(
                f"SRAM region is only {ram.length_bytes // 1024} KiB, below the "
                f"{MIN_F4_SRAM_BYTES // 1024} KiB comfortable F' bare-metal minimum; "
                "shrink FPRIME_STM32_BOOTSTRAP_POOL_SIZE and the topology to fit"
            )
        return MemoryMap(regions=regions, flash=flash, warnings=warnings, ram=ram)

    dtcm = next((r for r in regions if r.origin == DTCM_RAM_ORIGIN), None)
    axi_sram = next((r for r in regions if r.origin == AXI_SRAM_ORIGIN), None)
    if dtcm is None:
        raise LinkerScriptError(
            f"MEMORY block has no region at DTCM RAM origin 0x{DTCM_RAM_ORIGIN:08X} "
            "(not an STM32H7 CubeMX linker script?)"
        )
    if axi_sram is None:
        raise LinkerScriptError(
            f"MEMORY block has no region at AXI SRAM origin 0x{AXI_SRAM_ORIGIN:08X} "
            "(not an STM32H7 CubeMX linker script?)"
        )

    warnings = []
    if axi_sram.length_bytes < MIN_AXI_SRAM_BYTES:
        warnings.append(
            f"AXI SRAM region is only {axi_sram.length_bytes // 1024} KiB, below the "
            f"{MIN_AXI_SRAM_BYTES // 1024} KiB F' bare-metal minimum "
            "(Fw::Buffer pools / BootstrapAllocator heap may not fit)"
        )

    return MemoryMap(regions=regions, flash=flash, warnings=warnings, dtcm=dtcm, axi_sram=axi_sram)

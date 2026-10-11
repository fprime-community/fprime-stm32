from dataclasses import dataclass, field
from pathlib import Path

from fprime_stm32.memory_model import MemoryMap


@dataclass
class FileChange:
    label: str
    path: Path
    actions: list[str]


@dataclass
class SyncReport:
    chip_tag: str
    memory_map: MemoryMap
    linker_actions: list[str]
    startup_actions: list[str]
    cmake_actions: list[str]
    linker_out: Path
    startup_out: Path
    cmake_out: Path
    dry_run: bool
    family: str = "stm32h7"
    scaffolded: list[str] = field(default_factory=list)
    project_wiring: list[FileChange] = field(default_factory=list)
    deployment_note: str | None = None
    warnings: list[str] = field(default_factory=list)

    def render(self) -> str:
        def region_line(label: str, region) -> str:
            kib = region.length_bytes / 1024
            return f"  {label:<10} 0x{region.origin:08X}  {kib:.0f} KiB  (region {region.name!r})"

        memory = self.memory_map
        region_lines = [region_line("FLASH", memory.flash)]
        for label, region in (("AXI SRAM", memory.axi_sram), ("DTCM RAM", memory.dtcm), ("SRAM", memory.ram)):
            if region is not None:
                region_lines.append(region_line(label, region))

        lines = [
            f"fprime-stm32 sync - {self.family.upper()} target: {self.chip_tag}",
            "",
            "Memory bounds extracted:",
            *region_lines,
            "",
            "Linker script changes:",
            *[f"  - {a}" for a in self.linker_actions],
            "",
            "Startup script changes:",
            *[f"  - {a}" for a in self.startup_actions],
            "",
            "Hardware/CMakeLists.txt changes:",
            *[f"  - {a}" for a in self.cmake_actions],
            "",
        ]

        if self.scaffolded:
            lines.append("Hardware/config scaffolded from the fprime-stm32 template (review before flashing):")
            lines.extend(f"  - {name}" for name in self.scaffolded)
            lines.append("")

        for change in self.project_wiring:
            lines.append(f"{change.label} changes:")
            if change.actions:
                lines.extend(f"  - {a}" for a in change.actions)
            else:
                lines.append("  - already wired, no changes needed")
            lines.append("")

        if self.deployment_note:
            lines.append(self.deployment_note)
            lines.append("")

        all_warnings = [*self.memory_map.warnings, *self.warnings]
        if all_warnings:
            lines.append("Warnings:")
            lines.extend(f"  ! {w}" for w in all_warnings)
            lines.append("")

        out_files = [self.linker_out, self.startup_out, self.cmake_out, *(c.path for c in self.project_wiring)]
        if self.dry_run:
            lines.append("(dry run - no files were written)")
            lines.extend(f"  would write: {p}" for p in out_files)
        else:
            lines.append("Files written:")
            lines.extend(f"  {p}" for p in out_files)

        return "\n".join(lines)

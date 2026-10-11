import re
from dataclasses import dataclass
from pathlib import Path

from fprime_stm32.errors import FamilyDetectionError


@dataclass(frozen=True)
class Family:
    name: str  # also the F' platform name and the Templates/<name> directory
    platform_target: str  # CMake target exported by cmake/platform/<name>.cmake
    ioc_family: str  # value of `Mcu.Family=` in a CubeMX .ioc file
    startup_glob: str
    chip_re: str
    hal_driver_dir: str


FAMILIES: dict[str, Family] = {
    "stm32h7": Family(
        name="stm32h7",
        platform_target="PlatformStm32h7",
        ioc_family="STM32H7",
        startup_glob="startup_stm32h7*.s",
        chip_re=r"stm32h7\w*",
        hal_driver_dir="STM32H7xx_HAL_Driver",
    ),
    "stm32f4": Family(
        name="stm32f4",
        platform_target="PlatformStm32f4",
        ioc_family="STM32F4",
        startup_glob="startup_stm32f4*.s",
        chip_re=r"stm32f4\w*",
        hal_driver_dir="STM32F4xx_HAL_Driver",
    ),
}

AUTO = "auto"
FAMILY_CHOICES = [AUTO, *FAMILIES]


def get_family(name: str) -> Family:
    try:
        return FAMILIES[name]
    except KeyError:
        raise FamilyDetectionError(f"Unsupported STM32 family {name!r} (supported: {', '.join(FAMILIES)})") from None


def _read_ioc_family(cubemx_path: Path) -> str | None:
    for ioc in sorted(cubemx_path.glob("*.ioc")):
        match = re.search(r"^Mcu\.Family\s*=\s*(\S+)", ioc.read_text(errors="replace"), re.MULTILINE)
        if match:
            return match.group(1).upper()
    return None


def detect_family_signals(cubemx_path: Path) -> list[tuple[str, str]]:
    """List (artifact, family) pairs for each family-revealing CubeMX artifact."""
    signals: list[tuple[str, str]] = []

    for family in FAMILIES.values():
        startup_dirs = (cubemx_path / "Core" / "Startup", cubemx_path)
        if any(any(d.glob(family.startup_glob)) for d in startup_dirs if d.is_dir()):
            signals.append(("startup file", family.name))
        if (cubemx_path / "Drivers" / family.hal_driver_dir).is_dir():
            signals.append(("HAL driver directory", family.name))

    ioc_family = _read_ioc_family(cubemx_path)
    if ioc_family is not None:
        for family in FAMILIES.values():
            if ioc_family == family.ioc_family:
                signals.append((".ioc Mcu.Family", family.name))

    cubemx_cmake = cubemx_path / "cmake" / "stm32cubemx" / "CMakeLists.txt"
    if cubemx_cmake.is_file():
        text = cubemx_cmake.read_text(errors="replace")
        for family in FAMILIES.values():
            if re.search(family.chip_re, text, re.IGNORECASE):
                signals.append(("chip define", family.name))

    return signals


def detect_family(cubemx_path: Path, requested: str = AUTO) -> Family:
    """Resolve the target family from CubeMX output, cross-checking an explicit --family."""
    signals = detect_family_signals(cubemx_path)
    detected = {name for _, name in signals}

    if len(detected) > 1:
        listing = ", ".join(f"{source} -> {name}" for source, name in signals)
        raise FamilyDetectionError(f"{cubemx_path} has conflicting STM32 family indicators ({listing})")

    if requested != AUTO:
        family = get_family(requested)
        if detected and family.name not in detected:
            raise FamilyDetectionError(
                f"--family {requested} was requested but {cubemx_path} looks like {next(iter(detected))} "
                f"({', '.join(source for source, _ in signals)})"
            )
        return family

    if not detected:
        raise FamilyDetectionError(
            f"Could not detect the STM32 family of {cubemx_path}. Pass --family "
            f"{{{','.join(FAMILIES)}}} explicitly."
        )
    return get_family(next(iter(detected)))

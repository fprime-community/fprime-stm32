class CliError(Exception):
    """Base class for errors reported cleanly to the user (no traceback)."""


class ProjectDiscoveryError(CliError):
    """Raised when the F' project root or its Hardware/ directory can't be located."""


class LinkerScriptError(CliError):
    """Raised when a CubeMX linker script is missing, malformed, or an unrecognized template."""


class StartupScriptError(CliError):
    """Raised when a CubeMX startup assembly file is missing or an unrecognized template."""


class CubeMxCMakeError(CliError):
    """Raised when the CubeMX-generated cmake/stm32cubemx/CMakeLists.txt is missing or malformed."""


class FamilyDetectionError(CliError):
    """Raised when the STM32 family can't be determined, is unsupported, or contradicts --family."""

import shutil
from pathlib import Path

import pytest

from fprime_stm32.errors import FamilyDetectionError
from fprime_stm32.family import detect_family

FIXTURES = Path(__file__).parent / "fixtures"
H7 = FIXTURES / "cubemx_stm32h753"
F4 = FIXTURES / "cubemx_stm32f401"


def test_detects_h7_from_files():
    assert detect_family(H7).name == "stm32h7"


def test_detects_f4_from_files():
    assert detect_family(F4).name == "stm32f4"


def test_explicit_family_matching_files_is_accepted():
    assert detect_family(F4, "stm32f4").name == "stm32f4"


def test_explicit_family_contradicting_files_is_rejected():
    with pytest.raises(FamilyDetectionError, match="looks like stm32f4"):
        detect_family(F4, "stm32h7")


def test_unknown_project_requires_explicit_family(tmp_path):
    with pytest.raises(FamilyDetectionError, match="--family"):
        detect_family(tmp_path)
    assert detect_family(tmp_path, "stm32f4").name == "stm32f4"


def test_conflicting_signals_are_rejected(tmp_path):
    shutil.copytree(F4, tmp_path / "p")
    shutil.copy(H7 / "startup_stm32h753xx.s", tmp_path / "p" / "startup_stm32h753xx.s")
    with pytest.raises(FamilyDetectionError, match="conflicting"):
        detect_family(tmp_path / "p")

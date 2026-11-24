import contextlib
import io
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest import mock

import flash


class FlashTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        (self.root / "CMakeLists.txt").touch()
        (self.root / "settings.ini").write_text("[fprime]\ndefault_toolchain = stm32h7\n")
        self.image = self.root / "build-artifacts/stm32h7/RadioReference/bin/RadioReference"
        self.image.parent.mkdir(parents=True)
        self.image.write_bytes(b"\x7fELF\x01\x01" + bytes(10) + struct.pack("<HH", 2, 40))

    def test_list_and_interactive_selection(self):
        other = self.root / "build-artifacts/stm32h7/ReferenceDeployment/bin/ReferenceDeployment"
        other.parent.mkdir(parents=True)
        other.write_bytes(self.image.read_bytes())
        with contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(flash.main(["--project-root", str(self.root), "--list"]), 0)
        self.assertIn("RadioReference:", output.getvalue())
        self.assertIn("ReferenceDeployment:", output.getvalue())
        with mock.patch("builtins.input", side_effect=["3", "2"]), \
             contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(flash.select_deployment(flash.find_images(self.root, "stm32h7", None)),
                             "ReferenceDeployment")

    def test_build_directory_and_invalid_image(self):
        binary_dir = self.root / "custom-build/bin/stm32h7"
        binary_dir.mkdir(parents=True)
        image = binary_dir / "RadioReference"
        image.write_bytes(b"not an ELF")
        with contextlib.redirect_stderr(io.StringIO()) as error:
            result = flash.main(["RadioReference", "--project-root", str(self.root),
                                 "--build-dir", str(self.root / "custom-build")])
        self.assertEqual(result, 1)
        self.assertIn("Not a 32-bit", error.getvalue())

    def test_programmer_receives_elf_and_failure_is_reported(self):
        def run(command, check):
            self.assertFalse(check)
            self.assertEqual(command[1:4], ["-c", "port=SWD", "sn=abc"])
            self.assertEqual(command[4], "-d")
            self.assertEqual(Path(command[5]).suffix, ".elf")
            self.assertEqual(Path(command[5]).read_bytes(), self.image.read_bytes())
            self.assertEqual(command[6:], ["-v", "-rst"])
            return subprocess.CompletedProcess(command, 7)

        with mock.patch("flash.find_programmer", return_value="/fake/programmer"), \
             mock.patch("flash.subprocess.run", side_effect=run), \
             contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()) as error:
            result = flash.main(["RadioReference", "--project-root", str(self.root), "--serial", "abc"])
        self.assertEqual(result, 7)
        self.assertIn("exit code 7", error.getvalue())

    def test_dry_run_never_executes_programmer(self):
        with mock.patch("flash.find_programmer", return_value="/fake/programmer"), \
             mock.patch("flash.subprocess.run") as run, contextlib.redirect_stdout(io.StringIO()):
            result = flash.main(["RadioReference", "--project-root", str(self.root), "--dry-run"])
        self.assertEqual(result, 0)
        run.assert_not_called()

    def test_unknown_deployment_does_not_run(self):
        with mock.patch("flash.subprocess.run") as run, contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(flash.main(["Missing", "--project-root", str(self.root)]), 1)
        run.assert_not_called()


if __name__ == "__main__":
    unittest.main()

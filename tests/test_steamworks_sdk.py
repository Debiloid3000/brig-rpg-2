import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from verify_steamworks_sdk import REQUIRED_FILES, verify_sdk


class SteamworksSdkSetupTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.sdk = Path(self.temp.name) / "sdk"

    def tearDown(self):
        self.temp.cleanup()

    def test_missing_sdk_explains_official_download_path(self):
        with self.assertRaisesRegex(ValueError, "partner.steamgames.com"):
            verify_sdk(self.sdk)

    def test_valid_32_bit_sdk_layout_passes(self):
        for relative in REQUIRED_FILES:
            target = self.sdk / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b"SDK")
        result = verify_sdk(self.sdk)
        self.assertEqual(result["headers"], 2)
        self.assertEqual(result["windows_x86_files"], 2)
        self.assertEqual(result["linux_x86_files"], 1)
        self.assertEqual(len(result["validated_files"]), len(REQUIRED_FILES))

    def test_missing_matchmaking_header_is_reported(self):
        for relative in REQUIRED_FILES:
            if relative == Path("public/steam/isteammatchmaking.h"):
                continue
            target = self.sdk / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b"SDK")
        with self.assertRaisesRegex(ValueError, "isteammatchmaking.h"):
            verify_sdk(self.sdk)


if __name__ == "__main__":
    unittest.main()

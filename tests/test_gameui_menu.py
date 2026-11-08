"""Checks the reviewed Steam Legacy GameUI resource overrides."""
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from brig_assets import digest
from validate_brig import APPROVED_RESOURCE_OVERRIDES


class GameMenuResourceTests(unittest.TestCase):
    def test_only_the_three_intended_gameui_resources_are_overridden(self):
        self.assertEqual(
            set(APPROVED_RESOURCE_OVERRIDES),
            {
                "resource/gamemenu.res",
                "resource/gameui_english.txt",
                "resource/gameui_russian.txt",
            },
        )
        for relative_path, expected_sha256 in APPROVED_RESOURCE_OVERRIDES.items():
            source = ROOT / "_folder/brig" / relative_path
            self.assertTrue(source.is_file(), relative_path)
            self.assertEqual(digest(source), expected_sha256, relative_path)

    def test_gamemenu_routes_play_and_multiplayer_to_client_vgui(self):
        menu = (ROOT / "_folder/brig/resource/gamemenu.res").read_text(encoding="utf-8-sig")
        self.assertIn('"command" "engine brig_menu_play"', menu)
        self.assertIn('"command" "engine brig_menu_multiplayer"', menu)
        self.assertNotIn("#GameUI_GameMenu_NewGame", menu)
        self.assertIn('"command" "OpenSaveGameDialog"', menu)
        self.assertIn('"command" "OpenLoadGameDialog"', menu)
        self.assertIn('"notmulti" "1"', menu)

    def test_both_language_files_have_brig_menu_labels(self):
        for language in ("english", "russian"):
            text = (ROOT / f"_folder/brig/resource/gameui_{language}.txt").read_text(encoding="utf-8-sig")
            self.assertIn('"BRIG_GameMenu_Play"', text)
            self.assertIn('"BRIG_GameMenu_Multiplayer"', text)
            self.assertIn('"BRIG_Menu_Singleplayer"', text)
            self.assertIn('"BRIG_Menu_ServerBrowser"', text)


if __name__ == "__main__":
    unittest.main()

"""Checks BRIG GameUI resources, VGUI buttons and build installation routing."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class GameMenuResourceTests(unittest.TestCase):
    def test_gamemenu_routes_play_and_multiplayer_to_client_vgui(self):
        menu = (ROOT / "_folder/brig/resource/gamemenu.res").read_text(encoding="utf-8-sig")
        self.assertIn('"command" "engine brig_menu_play"', menu)
        self.assertIn('"command" "engine brig_menu_multiplayer"', menu)
        self.assertNotIn("#GameUI_GameMenu_NewGame", menu)
        self.assertIn('"command" "OpenSaveGameDialog"', menu)
        self.assertIn('"command" "OpenLoadGameDialog"', menu)
        self.assertIn('"notmulti" "1"', menu)

    def test_every_brig_token_used_by_client_exists_in_both_locales(self):
        source = (ROOT / "cl_dll/brig_main_menu.cpp").read_text(encoding="utf-8")
        required = set(re.findall(r'"(BRIG_[A-Za-z0-9_]+)"', source))
        for language in ("english", "russian"):
            text = (ROOT / f"_folder/brig/resource/gameui_{language}.txt").read_text(encoding="utf-8-sig")
            primary = {
                key for key in re.findall(r'^\s*"([^"]+)"\s+"', text, re.M)
                if not key.startswith("[")
            }
            missing = sorted(required - primary)
            self.assertFalse(missing, f"{language} is missing BRIG tokens: {missing}")

    def test_custom_vgui_buttons_are_mouse_clickable(self):
        source = (ROOT / "cl_dll/brig_main_menu.cpp").read_text(encoding="utf-8")
        self.assertIn("addActionSignal(new CBrigMenuButtonSignal(this, i))", source)
        self.assertIn("setMouseClickEnabled(MOUSE_LEFT, true)", source)

    def test_language_selection_can_refresh_after_startup(self):
        source = (ROOT / "cl_dll/brig_main_menu.cpp").read_text(encoding="utf-8")
        self.assertIn('language.rfind("ru-", 0)', source)
        self.assertIn("loadedRussian == russian", source)
        self.assertIn('russian ? "resource/gameui_russian.txt"', source)

    def test_build_copy_requires_explicit_brig_destination(self):
        script = (ROOT / "filecopy.bat").read_text(encoding="utf-8")
        self.assertIn("BRIG_GAME_DIR", script)
        self.assertNotIn("halflife_updated", script)
        for project in ("hl_cdll.vcxproj", "hldll.vcxproj"):
            data = (ROOT / "projects/vs2019" / project).read_text(encoding="utf-8")
            self.assertNotIn("<PostBuildEvent />", data)


if __name__ == "__main__":
    unittest.main()

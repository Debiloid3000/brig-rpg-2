import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from brig_server import build_command


class BrigDedicatedServerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.hlds = self.root / "hlds.exe"
        self.hlds.write_bytes(b"MZ")
        self.mod = self.root / "brig"
        (self.mod / "dlls").mkdir(parents=True)
        (self.mod / "maps").mkdir()
        (self.mod / "liblist.gam").write_text('game "Brig RPG 2"\n', encoding="utf-8")
        (self.mod / "dlls" / "hl.dll").write_bytes(b"MZ")
        (self.mod / "maps" / "c0a0.bsp").write_bytes(b"\x1e\x00\x00\x00" + bytes(124))
        (self.mod / "maps" / "c0a0.cfg").write_text(
            "brig_coop 1\nbrig_deathmatch 0\nbrig_team_deathmatch 0\n",
            encoding="utf-8",
        )
        (self.mod / "maps" / "brig_maplist.txt").write_text(
            "# BRIG map catalog\nc0a0\ncrossfire // registered competitive map\n",
            encoding="utf-8",
        )
        (self.mod / "maps" / "crossfire.bsp").write_bytes(b"\x1e\x00\x00\x00" + bytes(124))
        (self.mod / "maps" / "crossfire.cfg").write_text(
            "brig_coop 0\nbrig_deathmatch 1\nbrig_team_deathmatch 1\n",
            encoding="utf-8",
        )

    def tearDown(self):
        self.temp.cleanup()

    def test_coop_command_uses_server_mode_and_map_before_post_map_overrides(self):
        command, cwd = build_command(self.hlds, self.mod, map_name="c0a0")
        self.assertEqual(cwd, self.root)
        self.assertIn("-game", command)
        self.assertEqual(command[command.index("-game") + 1], "brig")
        self.assertEqual(command[command.index("+deathmatch") + 1], "0")
        self.assertEqual(command[command.index("+coop") + 1], "1")
        self.assertEqual(command[command.index("+mp_teamplay") + 1], "0")
        self.assertEqual(command[command.index("+map") + 1], "c0a0")
        self.assertLess(command.index("+coop"), command.index("+map"))
        self.assertGreater(command.index("+hostname", command.index("+map")), command.index("+map"))
        self.assertEqual(command[command.index("+mp_timelimit") + 1], "0")
        self.assertEqual(command[command.index("+mp_fraglimit") + 1], "0")
        self.assertEqual(command[command.index("+mp_allowmonsters") + 1], "1")

    def test_coop_ignores_competitive_time_and_frag_limits(self):
        command, _ = build_command(
            self.hlds, self.mod, map_name="c0a0", mode="coop",
            time_limit_minutes=90, frag_limit=500,
        )
        self.assertEqual(command[command.index("+mp_timelimit") + 1], "0")
        self.assertEqual(command[command.index("+mp_fraglimit") + 1], "0")

    def test_existing_bsp_and_cfg_are_rejected_when_missing_from_manifest(self):
        (self.mod / "maps" / "unlisted.bsp").write_bytes(bytes((30, 0, 0, 0)) + bytes(124))
        (self.mod / "maps" / "unlisted.cfg").write_text("brig_coop 1\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "not registered"):
            build_command(self.hlds, self.mod, map_name="unlisted")

    def test_deathmatch_and_team_deathmatch_require_matching_map_flags(self):
        dm, _ = build_command(self.hlds, self.mod, map_name="crossfire", mode="deathmatch")
        tdm, _ = build_command(self.hlds, self.mod, map_name="crossfire", mode="team_deathmatch")
        self.assertEqual(dm[dm.index("+deathmatch") + 1], "1")
        self.assertEqual(dm[dm.index("+coop") + 1], "0")
        self.assertEqual(dm[dm.index("+mp_teamplay") + 1], "0")
        self.assertEqual(tdm[tdm.index("+mp_teamplay") + 1], "1")

    def test_host_values_are_passed_as_separate_arguments(self):
        command, _ = build_command(
            self.hlds, self.mod, hostname="BRIG Co-op Server", password="team pass", lan=False
        )
        self.assertIn("BRIG Co-op Server", command)
        self.assertIn("team pass", command)
        self.assertEqual(command[command.index("+sv_lan") + 1], "0")
        self.assertEqual(command[command.index("+sv_lan", command.index("+map")) + 1], "0")

    def test_session_settings_are_applied_before_and_after_map(self):
        command, _ = build_command(
            self.hlds,
            self.mod,
            map_name="crossfire",
            mode="deathmatch",
            time_limit_minutes=45,
            frag_limit=50,
            friendly_fire=True,
            fall_damage=True,
            flashlight=False,
            allow_monsters=True,
            allow_spectators=True,
        )
        map_index = command.index("+map")
        for name, value in (
            ("+mp_timelimit", "45"),
            ("+mp_fraglimit", "50"),
            ("+mp_friendlyfire", "1"),
            ("+mp_falldamage", "1"),
            ("+mp_flashlight", "0"),
            ("+mp_allowmonsters", "1"),
            ("+allow_spectators", "1"),
        ):
            self.assertEqual(command[command.index(name) + 1], value)
            self.assertEqual(command[command.index(name, map_index) + 1], value)

    def test_invalid_session_limits_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "time_limit_minutes"):
            build_command(self.hlds, self.mod, time_limit_minutes=121)
        with self.assertRaisesRegex(ValueError, "frag_limit"):
            build_command(self.hlds, self.mod, frag_limit=1001)

    def test_unregistered_or_unsupported_map_mode_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "not enabled for coop"):
            build_command(self.hlds, self.mod, map_name="crossfire", mode="coop")

    def test_invalid_map_identifier_and_console_injection_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "plain BSP identifier"):
            build_command(self.hlds, self.mod, map_name="../c0a0")
        with self.assertRaisesRegex(ValueError, "console delimiter"):
            build_command(self.hlds, self.mod, hostname='BRIG";quit')

    def test_non_goldsrc_bsp_is_rejected(self):
        (self.mod / "maps" / "badmap.bsp").write_bytes(b"\x1d\x00\x00\x00" + bytes(124))
        (self.mod / "maps" / "badmap.cfg").write_text("brig_coop 1\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "not a GoldSrc BSP"):
            build_command(self.hlds, self.mod, map_name="badmap")

    def test_invalid_player_count_and_port_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "max_players"):
            build_command(self.hlds, self.mod, max_players=33)
        with self.assertRaisesRegex(ValueError, "port"):
            build_command(self.hlds, self.mod, port=0)


if __name__ == "__main__":
    unittest.main()

import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from package_brig import copy_map_metadata
from validate_brig import MAP_METADATA_FILES, validate_generated_map_metadata


class MapMetadataPackagingTests(unittest.TestCase):
    def test_manifest_matches_every_installed_bsp_and_cfg(self):
        maps_dir = ROOT / "_folder/brig/maps"
        manifest = maps_dir / "brig_maplist.txt"
        ids = [
            line.strip().split()[0].lower()
            for line in manifest.read_text(encoding="utf-8").splitlines()
            if line.strip() and not line.lstrip().startswith("#") and not line.lstrip().startswith("//")
        ]

        self.assertEqual(len(ids), len(set(ids)))
        self.assertTrue(all(len(map_id) <= 31 for map_id in ids))
        bsp_ids = {path.stem.lower() for path in maps_dir.glob("*.bsp")}
        cfg_ids = {path.stem.lower() for path in maps_dir.glob("*.cfg")}
        self.assertEqual(set(ids), bsp_ids)
        self.assertEqual(set(ids), cfg_ids)

        for map_id in ids:
            config = (maps_dir / f"{map_id}.cfg").read_text(encoding="utf-8")
            values = {}
            for line in config.splitlines():
                parts = line.strip().split(None, 1)
                if len(parts) == 2 and not parts[0].startswith(("//", "#")):
                    values[parts[0].lower()] = parts[1].strip().strip('"')

            if values.get("brig_campaign") == "half_life":
                self.assertEqual(values.get("brig_coop"), "1", map_id)
            elif values.get("brig_campaign") == "hazard_course":
                self.assertEqual(values.get("brig_singleplayer"), "1", map_id)
                self.assertEqual(values.get("brig_coop"), "1", map_id)
                self.assertEqual(values.get("brig_deathmatch"), "0", map_id)
                self.assertEqual(values.get("brig_team_deathmatch"), "0", map_id)
            elif values.get("brig_campaign") == "half_life_deathmatch":
                self.assertEqual(values.get("brig_deathmatch"), "1", map_id)
                self.assertEqual(values.get("brig_team_deathmatch"), "1", map_id)
                self.assertEqual(values.get("brig_singleplayer"), "0", map_id)

    def test_only_registered_map_metadata_is_staged_and_matches_source(self):
        with tempfile.TemporaryDirectory() as temp:
            stage = Path(temp) / "brig"
            copy_map_metadata(stage)

            actual = {
                path.relative_to(stage).as_posix(): path
                for path in stage.rglob("*")
                if path.is_file()
            }
            self.assertEqual(set(actual), set(MAP_METADATA_FILES))

            errors = []
            validate_generated_map_metadata(stage, actual, errors, require=True)
            self.assertEqual(errors, [])

            for name in MAP_METADATA_FILES:
                self.assertEqual((stage / name).read_bytes(), (ROOT / "_folder/brig" / name).read_bytes())

    def test_missing_or_modified_map_metadata_fails_validation(self):
        with tempfile.TemporaryDirectory() as temp:
            stage = Path(temp) / "brig"
            copy_map_metadata(stage)

            (stage / "maps/c0a0.cfg").write_text('brig_title "tampered"\n', encoding="utf-8")
            (stage / "maps/c1a0.cfg").unlink()

            actual = {
                path.relative_to(stage).as_posix(): path
                for path in stage.rglob("*")
                if path.is_file()
            }
            errors = []
            validate_generated_map_metadata(stage, actual, errors, require=True)

            self.assertTrue(any("differs from source: maps/c0a0.cfg" in e for e in errors))
            self.assertTrue(any("missing generated map metadata: maps/c1a0.cfg" in e for e in errors))

    def test_unstaged_metadata_is_allowed_only_for_unpacked_source_tree(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp)
            errors = []
            validate_generated_map_metadata(source, {}, errors, require=False)
            self.assertEqual(errors, [])

            validate_generated_map_metadata(source, {}, errors, require=True)
            self.assertEqual(len(errors), len(MAP_METADATA_FILES))


if __name__ == "__main__":
    unittest.main()

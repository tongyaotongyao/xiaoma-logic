"""冒险层关卡数据的回归测试。

这些用例直接读 `Content/Levels/levels.json`，所以以后每改一次关卡数据，
`python -m unittest discover -s Tests -v` 都会把可达性、par 和机关摆放重新验一遍。
"""

import copy
import json
import unittest
from pathlib import Path

from Tools import validate_adventure as va

DATA = json.loads((Path(__file__).resolve().parents[1]
                   / "Content" / "Levels" / "levels.json").read_text(encoding="utf-8"))


def adventure(level):
    return level["adventure"]


def solution_cells(level):
    return {(row, level["solution"][row]) for row in range(va.SIZE)}


class AdventureDataTests(unittest.TestCase):
    def test_all_levels_validate_without_errors(self):
        results, errors, warnings = va.validate(DATA)
        self.assertEqual(len(results), 25)
        self.assertEqual(errors, [])
        self.assertEqual(warnings, [])

    def test_par_uses_walk_budget(self):
        for level in DATA["levels"]:
            walkable = va.walkable_cells(adventure(level).get("obstacles") or [])
            minimum = va.min_steps(adventure(level)["start"], solution_cells(level), walkable)
            self.assertIsNotNone(minimum, level["id"])
            self.assertEqual(adventure(level)["par"], va.par_from(minimum), level["id"])

    def test_every_solution_cell_and_reward_is_reachable(self):
        for level in DATA["levels"]:
            walkable = va.walkable_cells(adventure(level).get("obstacles") or [])
            reached = va.reachable(adventure(level)["start"], walkable)
            for cell in solution_cells(level):
                self.assertIn(cell, reached, (level["id"], cell))
            for chest in adventure(level).get("chests") or []:
                self.assertIn(tuple(chest["cell"]), reached, level["id"])

    def test_items_and_chests_avoid_solution_cells(self):
        for level in DATA["levels"]:
            cells = solution_cells(level)
            for item in adventure(level).get("items") or []:
                self.assertNotIn(tuple(item["cell"]), cells, level["id"])
            for chest in adventure(level).get("chests") or []:
                self.assertNotIn(tuple(chest["cell"]), cells, level["id"])

    def test_opening_levels_stay_clean(self):
        for level in DATA["levels"][:5]:
            for key in va.UNIMPLEMENTED_KEYS:
                self.assertFalse(adventure(level).get(key), (level["id"], key))

    def test_obstacles_start_at_level_six(self):
        for level in DATA["levels"][:5]:
            self.assertFalse(adventure(level).get("obstacles"), level["id"])
        self.assertTrue(any(adventure(level).get("obstacles") for level in DATA["levels"][5:10]))

    def test_lives_follow_pacing_table(self):
        for level in DATA["levels"]:
            expected = 3 if level["id"] <= 10 else 2
            self.assertEqual(adventure(level)["lives"], expected, level["id"])

    def test_board_preview_renders_six_rows(self):
        for level in DATA["levels"]:
            adv = copy.deepcopy(adventure(level))
            adv.setdefault("items", [])
            adv.setdefault("chests", [])
            adv.setdefault("obstacles", [])
            adv["chests"] = [{"cell": chest["cell"], "puzzle": chest.get("puzzle", {}).get("type"),
                              "reward": chest.get("reward", "hint")}
                             for chest in adv["chests"]]
            lines = va.render_board(level, adv)
            self.assertEqual(len(lines), 6, level["id"])


class ValidatorGuardTests(unittest.TestCase):
    """破坏数据后校验工具必须报错，防止它变成永远说“通过”的工具。"""

    def test_obstacle_on_solution_cell_is_an_error(self):
        broken = copy.deepcopy(DATA)
        level = broken["levels"][0]
        solution_cell = [0, level["solution"][0]]
        level["adventure"]["obstacles"] = [solution_cell]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("障碍" in message for message in errors), errors)

    def test_wrong_par_is_reported(self):
        broken = copy.deepcopy(DATA)
        broken["levels"][0]["adventure"]["par"] = 99
        _, errors, warnings = va.validate(broken)
        self.assertEqual(errors, [])
        self.assertTrue(any("par" in message for message in warnings), warnings)

    def test_unknown_item_type_is_an_error(self):
        broken = copy.deepcopy(DATA)
        broken["levels"][5]["adventure"]["items"] = [{"type": "banana", "cell": [0, 2]}]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("banana" in message for message in errors), errors)

    def test_item_on_solution_cell_is_an_error(self):
        broken = copy.deepcopy(DATA)
        level = broken["levels"][0]
        level["adventure"]["items"] = [{"type": "apple", "cell": [1, level["solution"][1]]}]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("正解格" in message for message in errors), errors)

    def test_unreachable_chest_is_an_error(self):
        broken = copy.deepcopy(DATA)
        level = broken["levels"][2]
        solution = solution_cells(level)
        walls = [list(cell) for cell in solution if cell != (5, 0)]
        level["adventure"]["obstacles"] = walls
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("到不了" in message for message in errors), errors)

    def test_obstacle_on_chest_cell_is_an_error(self):
        broken = copy.deepcopy(DATA)
        level = broken["levels"][5]
        chest_cell = list(level["adventure"]["chests"][0]["cell"])
        level["adventure"]["obstacles"] = [chest_cell]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("宝箱与障碍同格" in message for message in errors), errors)

    def test_unsolvable_rune_order_is_an_error(self):
        """符文必须按顺序踩，顺序写成「必须先踩后面的」就该报错。"""
        broken = copy.deepcopy(DATA)
        # 第 8 关的 (1,0) 只能从 (2,0) 进去，把 (1,0) 排在第一就永远踩不到
        broken["levels"][7]["adventure"]["chests"][0]["puzzle"]["order"] = \
            [[1, 0], [2, 0], [2, 2], [1, 3]]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("符文顺序走不通" in message for message in errors), errors)

    def test_stele_answer_must_share_chest_cell(self):
        broken = copy.deepcopy(DATA)
        chest = broken["levels"][8]["adventure"]["chests"][0]
        chest["puzzle"]["answer"] = [5, 5]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("埋藏格应与宝箱同格" in message for message in errors), errors)

    def test_stele_on_solution_cell_is_an_error(self):
        broken = copy.deepcopy(DATA)
        level = broken["levels"][8]
        level["adventure"]["chests"][0]["puzzle"]["clue"]["cell"] = [0, level["solution"][0]]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("石碑立在正解格" in message for message in errors), errors)

    def test_rune_on_obstacle_is_an_error(self):
        broken = copy.deepcopy(DATA)
        level = broken["levels"][7]
        level["adventure"]["chests"][0]["puzzle"]["order"][0] = list(level["adventure"]["obstacles"][0])
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("与障碍同格" in message for message in errors), errors)

    def test_patrol_path_must_step_between_neighbours(self):
        broken = copy.deepcopy(DATA)
        broken["levels"][10]["adventure"]["patrols"] = [
            {"path": [[5, 2], [5, 5]], "speed": 1.5}]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("不相邻" in message for message in errors), errors)

    def test_patrol_end_on_solution_cell_is_an_error(self):
        broken = copy.deepcopy(DATA)
        level = broken["levels"][10]
        solution_cell = [0, level["solution"][0]]
        # 端点落在正解格上；中间补一格让路径本身合法
        level["adventure"]["patrols"] = [
            {"path": [solution_cell, [0, solution_cell[1] + 1]], "speed": 1.5}]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("正解格" in message for message in errors), errors)

    def test_patrol_through_obstacle_is_an_error(self):
        broken = copy.deepcopy(DATA)
        level = broken["levels"][5]
        obstacle = list(level["adventure"]["obstacles"][0])
        neighbour = [obstacle[0], obstacle[1] + 1 if obstacle[1] < 5 else obstacle[1] - 1]
        level["adventure"]["patrols"] = [{"path": [obstacle, neighbour], "speed": 1.5}]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("经过障碍" in message for message in errors), errors)

    def test_patrol_needs_two_cells(self):
        broken = copy.deepcopy(DATA)
        broken["levels"][10]["adventure"]["patrols"] = [{"path": [[5, 2]], "speed": 1.0}]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("至少要 2 个格子" in message for message in errors), errors)

    def test_patrol_speed_must_be_sane(self):
        broken = copy.deepcopy(DATA)
        broken["levels"][10]["adventure"]["patrols"] = [
            {"path": [[5, 2], [5, 3]], "speed": 0}]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("speed" in message for message in errors), errors)

    def test_box_on_solution_cell_is_an_error(self):
        broken = copy.deepcopy(DATA)
        level = broken["levels"][12]
        level["adventure"]["boxes"] = [{"cell": [0, level["solution"][0]]}]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("正解格" in message for message in errors), errors)

    def test_push_chest_without_box_is_an_error(self):
        broken = copy.deepcopy(DATA)
        broken["levels"][12]["adventure"]["boxes"] = []
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("却没有任何木箱" in message for message in errors), errors)

    def test_push_puzzle_must_not_carry_box_field(self):
        broken = copy.deepcopy(DATA)
        puzzle = broken["levels"][12]["adventure"]["chests"][0]["puzzle"]
        puzzle["box"] = [2, 3]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("不再使用 box 字段" in message for message in errors), errors)


class BoxPushTests(unittest.TestCase):
    """推箱状态搜索：木箱整格推，推不进障碍、别的箱子和棋盘外。"""

    ALL_CELLS = {(row, col) for row in range(va.SIZE) for col in range(va.SIZE)}

    def test_box_can_be_pushed_along_an_open_row(self):
        states = va.box_push_states((0, 0), [(0, 1)], self.ALL_CELLS, set())
        self.assertTrue(any((0, 5) in state[1] for state in states), "木箱应能沿整行推到最右")

    def test_box_cannot_leave_the_board(self):
        walkable = {(row, 0) for row in range(va.SIZE)}
        states = va.box_push_states((5, 0), [(3, 0)], walkable, set())
        rows = {cell[0] for state in states for cell in state[1]}
        self.assertEqual(rows, {0, 1, 2, 3})

    def test_box_stops_at_an_obstacle(self):
        walkable = self.ALL_CELLS - {(1, 0)}
        states = va.box_push_states((5, 0), [(2, 0)], walkable, set())
        cells = {cell for state in states for cell in state[1]}
        self.assertNotIn((0, 0), cells, "障碍后面的格子木箱过不去")


class PuzzlePacingTests(unittest.TestCase):
    """第 3~10 关该出现哪种开箱谜题，节奏表别被后面的改动冲掉。"""

    def puzzle_types(self, level):
        return [chest.get("puzzle", {}).get("type") for chest in adventure(level).get("chests") or []]

    def test_rule_chests_before_level_six(self):
        for level in DATA["levels"][2:5]:
            self.assertEqual(self.puzzle_types(level), ["rule"], level["id"])

    def test_runes_start_at_level_six(self):
        for level in DATA["levels"][5:8]:
            self.assertIn("runes", self.puzzle_types(level), level["id"])
        for level in DATA["levels"][:5]:
            self.assertNotIn("runes", self.puzzle_types(level), level["id"])

    def test_stele_starts_at_level_nine(self):
        for level in DATA["levels"][:8]:
            self.assertNotIn("stele", self.puzzle_types(level), level["id"])
        self.assertIn("stele", self.puzzle_types(DATA["levels"][8]), 9)
        self.assertIn("stele", self.puzzle_types(DATA["levels"][9]), 10)

    def test_second_chest_only_from_level_ten(self):
        for level in DATA["levels"][:9]:
            self.assertLessEqual(len(adventure(level).get("chests") or []), 1, level["id"])
        self.assertEqual(len(adventure(DATA["levels"][9]).get("chests") or []), 2)

    def test_patrols_start_at_level_eleven(self):
        for level in DATA["levels"][:10]:
            self.assertFalse(adventure(level).get("patrols"), level["id"])
        for level in DATA["levels"][10:15]:
            self.assertTrue(adventure(level).get("patrols"), level["id"])

    def test_push_starts_at_level_thirteen(self):
        for level in DATA["levels"][:12]:
            self.assertNotIn("push", self.puzzle_types(level), level["id"])
            self.assertFalse(adventure(level).get("boxes"), level["id"])
        self.assertIn("push", self.puzzle_types(DATA["levels"][12]), 13)
        self.assertTrue(adventure(DATA["levels"][12]).get("boxes"), 13)

    def test_boxes_only_where_a_plate_exists(self):
        for level in DATA["levels"]:
            boxes = adventure(level).get("boxes") or []
            plates = [chest["puzzle"].get("plate")
                      for chest in adventure(level).get("chests") or []
                      if chest.get("puzzle", {}).get("type") == "push"]
            self.assertEqual(bool(boxes), bool(plates), level["id"])

    def test_patrol_ends_stay_off_solution_cells(self):
        for level in DATA["levels"]:
            cells = solution_cells(level)
            for index, patrol in enumerate(adventure(level).get("patrols") or []):
                path = [va.as_cell(cell) for cell in patrol["path"]]
                for end in (path[0], path[-1]):
                    self.assertNotIn(end, cells, (level["id"], index))


if __name__ == "__main__":
    unittest.main()

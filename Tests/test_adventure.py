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


def gates_of(level):
    """把 JSON 里的 gates 补上区域格，变成 BFS 用的形状（与 check_adventure 输出一致）。"""
    regions = level["regions"]
    gates = []
    for gate in adventure(level).get("gates") or []:
        cells = [(row, col) for row in range(va.SIZE) for col in range(va.SIZE)
                 if regions[row][col] == gate["region"]]
        gates.append({"region": gate["region"],
                      "switch": va.as_cell(gate["switch"]),
                      "cells": cells})
    return gates


class AdventureDataTests(unittest.TestCase):
    def test_all_levels_validate_without_errors(self):
        results, errors, warnings = va.validate(DATA)
        self.assertEqual(len(results), 25)
        self.assertEqual(errors, [])
        self.assertEqual(warnings, [])

    def test_par_uses_walk_budget(self):
        for level in DATA["levels"]:
            walkable = va.walkable_cells(adventure(level).get("obstacles") or [])
            ice = {va.as_cell(cell) for cell in adventure(level).get("ice") or []}
            bridges = {va.as_cell(cell) for cell in adventure(level).get("bridges") or []}
            # 三星要求集齐宝箱，所以 par 的目标不只是 6 个正解格，
            # 还要包含宝箱谜题里必须踩到的格子（见 chest_route_cells）
            targets = solution_cells(level) | set(va.chest_route_cells(adventure(level).get("chests")))
            minimum = va.min_steps(adventure(level)["start"], targets,
                                   walkable, ice, bridges, gates_of(level))
            self.assertIsNotNone(minimum, level["id"])
            self.assertEqual(adventure(level)["par"], va.par_from(minimum), level["id"])

    def test_every_solution_cell_and_reward_is_reachable(self):
        for level in DATA["levels"]:
            walkable = va.walkable_cells(adventure(level).get("obstacles") or [])
            ice = {va.as_cell(cell) for cell in adventure(level).get("ice") or []}
            bridges = {va.as_cell(cell) for cell in adventure(level).get("bridges") or []}
            reached = va.reachable(adventure(level)["start"], walkable, ice, bridges, gates_of(level))
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

    def test_box_on_ice_is_an_error(self):
        broken = copy.deepcopy(DATA)
        level = broken["levels"][15]
        ice = level["adventure"]["ice"][0]
        level["adventure"]["boxes"] = [{"cell": list(ice)}]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("冰面上，冰上推不动箱子" in message for message in errors), errors)

    def test_item_on_ice_is_an_error(self):
        broken = copy.deepcopy(DATA)
        level = broken["levels"][15]
        ice = level["adventure"]["ice"][0]
        level["adventure"]["items"] = [{"type": "apple", "cell": list(ice)}]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("落在冰面" in message for message in errors), errors)

    def test_chest_on_ice_is_an_error(self):
        broken = copy.deepcopy(DATA)
        level = broken["levels"][15]
        ice = level["adventure"]["ice"][0]
        level["adventure"]["chests"][0]["cell"] = list(ice)
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("宝箱落在冰面" in message for message in errors), errors)

    def test_ice_on_obstacle_is_an_error(self):
        broken = copy.deepcopy(DATA)
        level = broken["levels"][5]
        obstacle = level["adventure"]["obstacles"][0]
        level["adventure"]["ice"] = [list(obstacle)]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("冰面与障碍同格" in message for message in errors), errors)

    def test_lone_ice_cell_is_reported_as_a_hint(self):
        broken = copy.deepcopy(DATA)
        level = broken["levels"][15]
        # 把冰面换成一格孤冰：滑进去马上就滑出来，等于没配
        level["adventure"]["ice"] = [[3, 3]]
        _, errors, warnings = va.validate(broken)
        self.assertEqual(errors, [])
        self.assertTrue(any("单格冰" in message for message in warnings), warnings)


class GateTests(unittest.TestCase):
    """色区围栏：封住整个色区，踩区域外的开关后永久打开，不能手动关上。"""

    def test_only_level_21_has_a_gate(self):
        for level in DATA["levels"][:20]:
            self.assertFalse(adventure(level).get("gates"), level["id"])
        self.assertTrue(adventure(DATA["levels"][20]).get("gates"), 21)

    def test_gate_switch_sits_outside_the_region(self):
        level = DATA["levels"][20]
        gate = adventure(level)["gates"][0]
        row, col = gate["switch"]
        self.assertNotEqual(level["regions"][row][col], gate["region"],
                            "开关在自己围住的区域里，门关着时根本踩不到")

    def test_gate_surrounds_a_solution_cell(self):
        # 围栏是唯一允许盖住正解格的阻碍，围不住正解格就只是把地板涂成装饰
        level = DATA["levels"][20]
        gate = adventure(level)["gates"][0]
        solution = level["solution"]
        inside = [row for row in range(va.SIZE)
                  if level["regions"][row][solution[row]] == gate["region"]]
        self.assertTrue(inside, "围栏没有围住任何正解格")

    def test_gate_costs_steps(self):
        # 开门这一步要真的写进最短路径，否则围栏等于白放
        level = DATA["levels"][20]
        adv = adventure(level)
        walkable = va.walkable_cells(adv.get("obstacles") or [])
        targets = [(row, level["solution"][row]) for row in range(va.SIZE)]
        gates = va.check_adventure(copy.deepcopy(level), [], [])["gates"]
        with_gate = va.min_steps(adv["start"], targets, walkable,
                                 frozenset(), frozenset(), gates)
        without = va.min_steps(adv["start"], targets, walkable)
        self.assertGreater(with_gate, without, "绕去踩开关没有产生任何步数代价")

    def test_closed_gate_blocks_region_cells(self):
        gates = [{"region": 2, "switch": (3, 3), "cells": [(1, 4), (1, 5), (2, 4), (2, 5)]}]
        _, region_bits, region_of = va.gate_tables(gates)
        self.assertTrue(va.gate_blocked((2, 4), region_of, region_bits, 0))
        self.assertFalse(va.gate_blocked((3, 3), region_of, region_bits, 0),
                         "开关本身不能被自己的门挡住")
        self.assertFalse(va.gate_blocked((2, 4), region_of, region_bits, 1))

    def test_switch_inside_region_is_an_error(self):
        broken = copy.deepcopy(DATA)
        level = broken["levels"][20]
        gate = adventure(level)["gates"][0]
        cell = next((r, c) for r in range(va.SIZE) for c in range(va.SIZE)
                    if level["regions"][r][c] == gate["region"])
        gate["switch"] = list(cell)
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("在围栏区域内" in message for message in errors), errors)

    def test_start_inside_region_is_an_error(self):
        broken = copy.deepcopy(DATA)
        level = broken["levels"][20]
        start = adventure(level)["start"]
        gate = adventure(level)["gates"][0]
        gate["region"] = level["regions"][start[0]][start[1]]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("起点在围栏区域内" in message for message in errors), errors)

    def test_two_gates_on_one_region_is_an_error(self):
        broken = copy.deepcopy(DATA)
        level = broken["levels"][20]
        gates = adventure(level)["gates"]
        gates.append({"region": gates[0]["region"], "switch": [4, 4]})
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("围住同一个色区" in message for message in errors), errors)

    def test_patrol_inside_region_is_an_error(self):
        # 门是动态的、巡逻路线是静态的：让鬼进区域会出现"开门前撞墙、开门后突然进来"
        broken = copy.deepcopy(DATA)
        level = broken["levels"][20]
        adventure(level)["patrols"] = [{"path": [[1, 4], [1, 5]], "speed": 1.5}]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("进入围栏区域" in message for message in errors), errors)

    def test_gate_with_ice_is_an_error(self):
        broken = copy.deepcopy(DATA)
        level = broken["levels"][20]
        adventure(level)["ice"] = [[3, 4], [3, 5]]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("色区围栏与冰面" in message for message in errors), errors)

    def test_gate_with_bridge_is_allowed(self):
        """围栏与塌陷木桥可以同关：两个状态掩码在搜索里本来就并存。"""
        data = copy.deepcopy(DATA)
        level = data["levels"][20]
        adventure(level)["bridges"] = [[4, 4]]
        _, errors, _ = va.validate(data)
        self.assertFalse(any("色区围栏与塌陷木桥" in message for message in errors), errors)

    def test_switch_on_obstacle_is_an_error(self):
        broken = copy.deepcopy(DATA)
        level = broken["levels"][20]
        adventure(level)["obstacles"] = [[4, 4]]
        adventure(level)["gates"][0]["switch"] = [4, 4]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("switch" in message and "障碍同格" in message for message in errors), errors)

    def test_level_25_gates_are_ordered(self):
        """25 关两道围栏是有序依赖：后一道的开关在前一道区域内，必须先开前一道。"""
        level = DATA["levels"][24]
        gates = gates_of(level)
        self.assertEqual(len(gates), 2)
        first, second = gates
        row, col = second["switch"]
        self.assertEqual(level["regions"][row][col], first["region"],
                         "25 关两道围栏之间没有依赖，只是两道并列的门")
        self.assertNotEqual(level["regions"][row][col], second["region"],
                            "第二道围栏的开关在自己区域里，永远踩不到")

    def test_ordered_gate_blocks_the_second_region(self):
        """只开前一道时，后一道围住的区域仍然进不去。"""
        level = DATA["levels"][24]
        gates = gates_of(level)
        adv = adventure(level)
        walkable = va.walkable_cells(adv.get("obstacles") or [])
        regions = level["regions"]
        second_cells = {(r, c) for r in range(va.SIZE) for c in range(va.SIZE)
                        if regions[r][c] == gates[1]["region"]}
        # 把第一道门的开关挪到棋盘外（永远踩不到），第二道门也就永远开不了
        closed_first = [{**gates[0], "switch": (-9, -9)}, gates[1]]
        reached = va.reachable(adv["start"], walkable, frozenset(), frozenset(), closed_first)
        self.assertFalse(second_cells & reached, "第一道门没开，第二道围栏的区域却已经进得去")


class ChainChestTests(unittest.TestCase):
    """组合宝箱：`puzzle.then` 串起来的多段谜题。"""

    def test_level_22_chest_is_a_two_stage_chain(self):
        level = DATA["levels"][21]
        chests = adventure(level)["chests"]
        self.assertEqual(len(chests), 1)
        chain = [stage.get("type") for stage in va.puzzle_stages(chests[0]["puzzle"])]
        self.assertEqual(chain, ["stele", "runes"])

    def test_every_chest_has_at_least_one_stage(self):
        for level in DATA["levels"]:
            for chest in adventure(level).get("chests") or []:
                stages = list(va.puzzle_stages(chest.get("puzzle") or {}))
                self.assertGreaterEqual(len(stages), 1, level["id"])
                self.assertLessEqual(len(stages), va.MAX_PUZZLE_STAGES, level["id"])

    def test_level_22_chest_sits_inside_the_gate(self):
        """设计文档要求：宝箱埋在被围栏围住的色区里，先开门才拿得到。"""
        level = DATA["levels"][21]
        chest = tuple(adventure(level)["chests"][0]["cell"])
        gate = adventure(level)["gates"][0]
        self.assertEqual(level["regions"][chest[0]][chest[1]], gate["region"])

    def test_levels_23_to_25_use_chains(self):
        """最后三关是「综合运用」：每关都要有多段链，并且链上不许重复前面的谜题类型。"""
        expected = {
            23: ["rule", "runes"],
            24: ["stele", "rule"],
            25: ["stele", "runes", "rule"],
        }
        for level_id, chain in expected.items():
            level = DATA["levels"][level_id - 1]
            chest = adventure(level)["chests"][0]
            stages = [stage.get("type") for stage in va.puzzle_stages(chest["puzzle"])]
            self.assertEqual(stages, chain, level_id)

    def test_stele_is_never_the_second_stage(self):
        """石碑段要求宝箱处于「埋着」状态，放第二段会让宝箱凭空消失。"""
        for level in DATA["levels"]:
            for chest in adventure(level).get("chests") or []:
                stages = list(va.puzzle_stages(chest.get("puzzle") or {}))
                for index, stage in enumerate(stages):
                    if stage.get("type") == "stele":
                        self.assertEqual(index, 0, "第 %s 关的石碑段不在第一段" % level["id"])

    def test_third_star_needs_every_chest(self):
        """par 必须把宝箱路线算进去，否则「步数达标 + 集齐」不可能同时满足。"""
        level = DATA["levels"][4]
        walkable = va.walkable_cells(adventure(level).get("obstacles") or [])
        gates = gates_of(level)
        solution_only = va.min_steps(adventure(level)["start"], solution_cells(level),
                                     walkable, frozenset(), frozenset(), gates)
        with_chest = va.min_steps(
            adventure(level)["start"],
            solution_cells(level) | set(va.chest_route_cells(adventure(level).get("chests"))),
            walkable, frozenset(), frozenset(), gates)
        self.assertGreater(with_chest, solution_only, "宝箱不在正解路线上，par 应该变大")
        self.assertLessEqual(with_chest, adventure(level)["par"])

    def test_too_many_stages_is_an_error(self):
        broken = copy.deepcopy(DATA)
        chest = broken["levels"][21]["adventure"]["chests"][0]
        # 现在两段，再挂两层就四段，超过上限 3
        third = {"type": "rule", "target": "row", "index": 0}
        third["then"] = {"type": "rule", "target": "row", "index": 1}
        chest["puzzle"]["then"]["then"] = third
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("最多" in message for message in errors), errors)

    def test_second_stage_rune_on_obstacle_is_an_error(self):
        """链式后续段的符文格也要进冲突检查，不能只看第一段。"""
        broken = copy.deepcopy(DATA)
        level = broken["levels"][21]
        level["adventure"]["obstacles"] = [[2, 4]]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("then.order[0]" in message and "障碍" in message
                            for message in errors), errors)

    def test_second_stage_rune_on_solution_cell_is_an_error(self):
        broken = copy.deepcopy(DATA)
        level = broken["levels"][21]
        level["adventure"]["chests"][0]["puzzle"]["then"]["order"] = [[0, 4], [2, 5]]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("then.order[0]" in message and "正解格" in message
                            for message in errors), errors)


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


class IceSlideTests(unittest.TestCase):
    """冰面滑行语义：踩上去就停不住，滑出冰面才落脚，撞墙就停在最后一块冰上。"""

    ALL_CELLS = {(row, col) for row in range(va.SIZE) for col in range(va.SIZE)}

    def test_plain_floor_costs_one_step(self):
        self.assertEqual(va.slide_step((0, 0), (0, 1), self.ALL_CELLS), ((0, 1), 1))

    def test_slide_leaves_ice_on_the_next_floor(self):
        ice = {(0, 1), (0, 2), (0, 3)}
        self.assertEqual(va.slide_step((0, 0), (0, 1), self.ALL_CELLS, ice), ((0, 4), 4))

    def test_slide_stops_on_last_ice_cell_at_the_wall(self):
        ice = {(0, 4), (0, 5)}
        self.assertEqual(va.slide_step((0, 3), (0, 1), self.ALL_CELLS, ice), ((0, 5), 2))

    def test_slide_stops_before_an_obstacle(self):
        ice = {(2, 1), (2, 2)}
        walkable = self.ALL_CELLS - {(2, 3)}
        self.assertEqual(va.slide_step((2, 0), (0, 1), walkable, ice), ((2, 2), 2))

    def test_blocked_step_returns_none(self):
        walkable = self.ALL_CELLS - {(0, 1)}
        self.assertIsNone(va.slide_step((0, 0), (0, 1), walkable))

    def test_ice_does_not_give_free_steps(self):
        """滑过去也要按跨过的格数记账，否则冰面会变成白赚步数的捷径。"""
        walkable = {(0, col) for col in range(va.SIZE)}
        ice = {(0, 1), (0, 2), (0, 3)}
        self.assertEqual(va.min_steps((0, 0), [(0, 5)], walkable), 5)
        self.assertEqual(va.min_steps((0, 0), [(0, 5)], walkable, ice), 5)

    def test_ice_on_solution_cell_is_an_error(self):
        """冰面盖住正解格就没法站定放小马，这一格等于被封死。"""
        broken = copy.deepcopy(DATA)
        level = broken["levels"][15]
        row = 0
        col = level["solution"][row]
        adventure(level)["ice"] = [[row, col], [row + 1, col]]
        _, errors, _ = va.validate(broken)
        self.assertTrue(any("冰面" in message and "正解格" in message for message in errors), errors)


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

    def test_ice_starts_at_level_sixteen(self):
        for level in DATA["levels"][:15]:
            self.assertFalse(adventure(level).get("ice"), level["id"])
        for level in DATA["levels"][15:17]:
            self.assertTrue(adventure(level).get("ice"), level["id"])

    def test_level_seventeen_ice_slides_far(self):
        """17 关的冰面要真滑得起来：至少存在一条一次滑 3 格以上的路线。

        不能拿「把冰面当墙就走不到正解格」当判据——那只在冰面压住正解格时成立，
        正是我们要禁掉的情况。这里改成直接量滑行距离。
        """
        level = DATA["levels"][16]
        adv = adventure(level)
        ice = {va.as_cell(cell) for cell in adv["ice"]}
        walkable = va.walkable_cells(adv.get("obstacles") or [])
        solution = set(solution_cells(level))
        self.assertFalse(ice & solution, "17 关冰面压住正解格")
        longest = 0
        for cell in ice:
            for direction in va.DIRECTIONS:
                entry = (cell[0] - direction[0], cell[1] - direction[1])
                if entry not in walkable or entry in ice:
                    continue
                step = va.slide_step(entry, direction, walkable, ice)
                if step:
                    longest = max(longest, step[1])
        self.assertGreaterEqual(longest, 3, "17 关的冰面滑不起来，等于没有")


class BridgeTests(unittest.TestCase):
    """塌陷木桥：按格计步，跨过去就变坑，塌了的桥不能再进。"""

    def test_bridge_costs_one_step_like_floor(self):
        walkable = {(0, col) for col in range(va.SIZE)}
        self.assertEqual(va.min_steps((0, 0), [(0, 5)], walkable), 5)
        self.assertEqual(va.min_steps((0, 0), [(0, 5)], walkable, frozenset(), {(0, 2)}), 5)

    def test_bridge_cell_is_crossable(self):
        walkable = {(0, col) for col in range(va.SIZE)}
        reached = va.reachable((0, 0), walkable, frozenset(), {(0, 2)})
        self.assertIn((0, 2), reached)
        self.assertIn((0, 5), reached)

    def test_used_bridge_blocks_reentry(self):
        index = {(0, 1): 1}
        self.assertFalse(va.bridge_step_blocked((0, 1), index, 0))
        self.assertTrue(va.bridge_step_blocked((0, 1), index, 1))

    def test_collapse_never_helps_the_player(self):
        """同样的地图，木桥会让最短步数不变或变长，绝不会变短。"""
        walkable = va.walkable_cells([(2, 2), (2, 3)])
        targets = [(0, 0), (5, 5)]
        flat = va.min_steps((5, 0), targets, walkable)
        bridged = va.min_steps((5, 0), targets, walkable, frozenset(), {(4, 4)})
        self.assertIsNotNone(bridged)
        self.assertGreaterEqual(bridged, flat)


class BridgeGuardTests(unittest.TestCase):
    """木桥的数据守卫：不压正解格，不和冰面/木箱/巡逻路线打架。"""

    def check(self, level):
        errors, warnings = [], []
        va.check_adventure(level, errors, warnings)
        return errors

    def level_18(self):
        return copy.deepcopy(DATA["levels"][17])

    def test_bridge_on_solution_cell_is_an_error(self):
        level = self.level_18()
        adventure(level)["bridges"] = [[0, 4]]
        errors = self.check(level)
        self.assertTrue(any("木桥" in message and "正解格" in message for message in errors), errors)

    def test_bridge_with_ice_is_an_error(self):
        level = self.level_18()
        adventure(level)["bridges"] = [[0, 5]]
        adventure(level)["ice"] = [[5, 5]]
        errors = self.check(level)
        self.assertTrue(any("冰面与塌陷木桥" in message for message in errors), errors)

    def test_bridge_with_box_is_an_error(self):
        level = self.level_18()
        adventure(level)["bridges"] = [[0, 5]]
        adventure(level)["boxes"] = [{"cell": [2, 4]}]
        errors = self.check(level)
        self.assertTrue(any("塌陷木桥与木箱" in message for message in errors), errors)

    def test_patrol_path_over_bridge_is_an_error(self):
        level = self.level_18()
        adventure(level)["bridges"] = [[0, 5]]
        adventure(level)["patrols"] = [{"path": [[0, 5], [1, 5], [2, 5]], "speed": 1.5}]
        errors = self.check(level)
        self.assertTrue(any("经过木桥" in message for message in errors), errors)


class BridgePacingTests(unittest.TestCase):
    def test_bridges_start_at_level_eighteen(self):
        for level in DATA["levels"][:17]:
            self.assertFalse(adventure(level).get("bridges"), level["id"])
        for level in DATA["levels"][17:20]:
            self.assertTrue(adventure(level).get("bridges"), level["id"])

    def test_bridge_levels_need_the_bridge(self):
        """把木桥当成墙就通不了关，说明桥不是装饰。"""
        for level in DATA["levels"][17:20]:
            adv = adventure(level)
            bridges = {va.as_cell(cell) for cell in adv["bridges"]}
            closed = va.walkable_cells(adv.get("obstacles") or []) - bridges
            reached = va.reachable(adv["start"], closed)
            self.assertTrue(set(solution_cells(level)) - reached, level["id"])


if __name__ == "__main__":
    unittest.main()

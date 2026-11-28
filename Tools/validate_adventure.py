"""校验《小马逻辑》冒险版关卡数据。

对 `Content/Levels/levels.json` 的每一关做两层检查：

1. 基础谜题不变量：六色区形状、每色 3~9 格且四向连通、空开局、720 种列排列穷举下唯一解。
2. 冒险层：起点、生命、道具、宝箱的位置与类型；障碍类机关是否落在正解格上；
   从起点出发的可达性（广度优先搜索，冰面按"踩上去就滑"的语义展开）；
   以及按“走遍 6 个正解格的最短步数 × 1.3 向上取整”算出的标准步数 par。

用法：
    python Tools/validate_adventure.py                 只报告，不改文件
    python Tools/validate_adventure.py --level 7       单独看第 7 关的棋盘预览
    python Tools/validate_adventure.py --write-par     把算出的 par 写回 levels.json
    python Tools/validate_adventure.py --quiet         只输出错误

退出码：0 表示全部通过，1 表示有问题。
"""

from __future__ import annotations

import argparse
import heapq
import itertools
import json
import math
import sys
from collections import deque
from pathlib import Path

SIZE = 6
CELL_COUNT = SIZE * SIZE
PAR_FACTOR = 1.3

ROOT = Path(__file__).resolve().parents[1]
LEVELS_PATH = ROOT / "Content" / "Levels" / "levels.json"

ITEM_TYPES = ("hint", "apple", "carrot", "clover")
ITEM_SYMBOL = {"hint": "h", "apple": "a", "carrot": "c", "clover": "f"}
PUZZLE_TYPES = ("rule", "runes", "stele", "push")
# 组合宝箱（puzzle.then 链）最多 3 段：再多玩家记不住顺序，也说明这一关该拆了
MAX_PUZZLE_STAGES = 3
UNIMPLEMENTED_KEYS = ("steles",)
DIRECTIONS = ((1, 0), (-1, 0), (0, 1), (0, -1))
INF = float("inf")


def puzzle_stages(puzzle):
    """按顺序产出 `puzzle` 与它 then 链上的每一段（组合宝箱）。"""
    stage = puzzle
    count = 0
    while isinstance(stage, dict) and count < MAX_PUZZLE_STAGES:
        yield stage
        stage = stage.get("then")
        count += 1


def chest_route_cells(chests):
    """宝箱谜题里玩家必须踩到的格子（含每一段）。

    三星要看「集齐本关宝箱」，所以标准步数也得把这条路线算进去；
    推箱段按「玩家自己站到压力板上」近似，手数没法在「进入格子」的度量里精确表达。
    """
    cells = []
    for chest in chests or []:
        if not isinstance(chest, dict):
            continue
        chest_cell = as_cell(chest.get("cell"))
        if isinstance(chest_cell, tuple):
            cells.append(chest_cell)
        for stage in puzzle_stages(chest.get("puzzle") or {}):
            kind = stage.get("type")
            if kind == "runes":
                cells += [as_cell(cell) for cell in stage.get("order") or []]
            elif kind == "stele":
                clue = stage.get("clue") or {}
                if isinstance(clue, dict) and "cell" in clue:
                    cells.append(as_cell(clue["cell"]))
            elif kind == "push":
                cells.append(as_cell(stage.get("plate")))
    return [cell for cell in cells if isinstance(cell, tuple)]


def load_levels(path=LEVELS_PATH):
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    return data


def neighbours(row, col):
    for dr, dc in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        nr, nc = row + dr, col + dc
        if 0 <= nr < SIZE and 0 <= nc < SIZE:
            yield nr, nc


def parse_cell(value, where, errors):
    """把 [行, 列] 解析成元组，非法时记错误并返回 None。"""
    if not isinstance(value, (list, tuple)) or len(value) != 2:
        errors.append("%s：格子应写成 [行, 列]，实际为 %r" % (where, value))
        return None
    row, col = value
    if not isinstance(row, int) or not isinstance(col, int) or isinstance(row, bool):
        errors.append("%s：行列必须是整数，实际为 %r" % (where, value))
        return None
    if not (0 <= row < SIZE and 0 <= col < SIZE):
        errors.append("%s：格子 %r 越界（行列应在 0~5）" % (where, (row, col)))
        return None
    return row, col


def parse_cell_list(value, where, errors):
    out = []
    if value is None:
        return out
    if not isinstance(value, list):
        errors.append("%s：应为格子数组，实际为 %r" % (where, value))
        return out
    for index, item in enumerate(value):
        cell = parse_cell(item, "%s[%d]" % (where, index), errors)
        if cell is not None:
            out.append(cell)
    return out


# --------------------------------------------------------------------------
# 基础谜题
# --------------------------------------------------------------------------

def check_puzzle(level, errors):
    level_id = level.get("id")
    regions = level.get("regions")
    if not isinstance(regions, list) or len(regions) != SIZE:
        errors.append("regions 应为 %d 行" % SIZE)
        return False
    for index, row in enumerate(regions):
        if not isinstance(row, list) or len(row) != SIZE:
            errors.append("regions[%d] 应有 %d 列" % (index, SIZE))
            return False

    ok = True
    colours = {value for row in regions for value in row}
    if colours != set(range(SIZE)):
        errors.append("色区编号应为 0~%d，实际为 %s" % (SIZE - 1, sorted(colours)))
        ok = False

    for colour in range(SIZE):
        cells = {(r, c) for r in range(SIZE) for c in range(SIZE) if regions[r][c] == colour}
        if not 3 <= len(cells) <= 9:
            errors.append("色区 %d 有 %d 格，应在 3~9 格之间" % (colour, len(cells)))
            ok = False
        if cells:
            reached = {next(iter(cells))}
            todo = list(reached)
            while todo:
                row, col = todo.pop()
                for other in neighbours(row, col):
                    if other in cells and other not in reached:
                        reached.add(other)
                        todo.append(other)
            if reached != cells:
                errors.append("色区 %d 不是四向连通" % colour)
                ok = False

    solution = level.get("solution")
    if not isinstance(solution, list) or len(solution) != SIZE:
        errors.append("solution 应有 %d 个列号" % SIZE)
        return False
    if sorted(solution) != list(range(SIZE)):
        errors.append("solution 不是 0~%d 的排列：%r" % (SIZE - 1, solution))
        return False
    for row in range(1, SIZE):
        if abs(solution[row] - solution[row - 1]) <= 1:
            errors.append("solution 第 %d、%d 行的小马八邻域相邻" % (row, row + 1))
            ok = False
    if len({regions[r][solution[r]] for r in range(SIZE)}) != SIZE:
        errors.append("solution 的小马没有覆盖全部 6 个色区")
        ok = False

    givens = level.get("givens")
    if givens:
        errors.append("givens 应为空（全部空开局），实际有 %d 个预设" % len(givens))
        ok = False

    answers = []
    for cols in itertools.permutations(range(SIZE)):
        if any(abs(cols[r] - cols[r - 1]) <= 1 for r in range(1, SIZE)):
            continue
        if len({regions[r][cols[r]] for r in range(SIZE)}) != SIZE:
            continue
        answers.append(cols)
    if len(answers) != 1:
        errors.append("解不唯一：穷举得到 %d 个解" % len(answers))
        ok = False
    elif list(answers[0]) != list(solution):
        errors.append("solution 与穷举结果不一致：%r" % (answers[0],))
        ok = False

    return ok and not errors


# --------------------------------------------------------------------------
# 冒险层
# --------------------------------------------------------------------------

def check_adventure(level, errors, warnings):
    level_id = level.get("id")
    adv = level.get("adventure")
    if not isinstance(adv, dict):
        errors.append("缺少 adventure 字段")
        return None

    out = {"items": [], "chests": [], "obstacles": [], "chests_cells": set(), "patrols": []}

    start = parse_cell(adv.get("start"), "adventure.start", errors)
    out["start"] = start
    if start is None:
        errors.append("adventure.start 缺失或非法")

    par = adv.get("par")
    if not isinstance(par, int) or isinstance(par, bool) or par <= 0:
        errors.append("adventure.par 应为正整数，实际为 %r" % (par,))
    out["par"] = par if isinstance(par, int) else None

    lives = adv.get("lives")
    if not isinstance(lives, int) or isinstance(lives, bool) or not 1 <= lives <= 5:
        errors.append("adventure.lives 应为 1~5，实际为 %r（生命上限 5）" % (lives,))

    solution = level.get("solution") or []
    solution_cells = {(r, solution[r]) for r in range(SIZE) if r < len(solution)}

    items = adv.get("items") or []
    if not isinstance(items, list):
        errors.append("adventure.items 应为数组")
        items = []
    seen_items = set()
    for index, item in enumerate(items):
        where = "adventure.items[%d]" % index
        if not isinstance(item, dict):
            errors.append("%s 应为对象" % where)
            continue
        kind = item.get("type")
        if kind not in ITEM_TYPES:
            errors.append("%s.type 应为 %s 之一，实际为 %r" % (where, "/".join(ITEM_TYPES), kind))
        cell = parse_cell(item.get("cell"), where + ".cell", errors)
        if cell is None:
            continue
        if cell in solution_cells:
            errors.append("%s 放在正解格 %r 上，会挡住宅解的证明路线" % (where, cell))
        if cell in seen_items:
            errors.append("%s 与另一件道具同格 %r" % (where, cell))
        seen_items.add(cell)
        out["items"].append((kind, cell))

    chests = adv.get("chests") or []
    if not isinstance(chests, list):
        errors.append("adventure.chests 应为数组")
        chests = []
    if len(chests) > 2:
        errors.append("宝箱最多 2 个，实际 %d 个" % len(chests))
    for index, chest in enumerate(chests):
        where = "adventure.chests[%d]" % index
        if not isinstance(chest, dict):
            errors.append("%s 应为对象" % where)
            continue
        cell = parse_cell(chest.get("cell"), where + ".cell", errors)
        if cell is None:
            continue
        if cell in solution_cells:
            errors.append("%s 放在正解格 %r 上" % (where, cell))
        if cell in seen_items:
            errors.append("%s 与道具同格 %r" % (where, cell))
        if cell in out["chests_cells"]:
            errors.append("%s 与另一个宝箱同格 %r" % (where, cell))
        out["chests_cells"].add(cell)

        reward = chest.get("reward", "hint")
        if reward not in ITEM_TYPES:
            errors.append("%s.reward 应为 %s 之一，实际为 %r"
                          % (where, "/".join(ITEM_TYPES), reward))

        puzzle = chest.get("puzzle")
        if not isinstance(puzzle, dict):
            errors.append("%s 缺少 puzzle（宝箱必须有开箱谜题）" % where)
            continue
        check_puzzle_def(puzzle, where + ".puzzle", errors, solution_cells)
        out["chests"].append({"cell": cell, "puzzle": puzzle.get("type"),
                              "chain": [stage.get("type") for stage in puzzle_stages(puzzle)],
                              "reward": reward, "puzzle_def": puzzle})

    obstacles = parse_cell_list(adv.get("obstacles"), "adventure.obstacles", errors)
    for cell in obstacles:
        if cell in solution_cells:
            errors.append("障碍 %r 压在正解格上，关卡不可通关" % (cell,))
        if cell == out["start"]:
            errors.append("主角起点 %r 与障碍同格" % (cell,))
    out["obstacles"] = obstacles
    if len(set(obstacles)) != len(obstacles):
        errors.append("adventure.obstacles 有重复格子")
    obstacle_set = set(obstacles)
    for kind, cell in out["items"]:
        if cell in obstacle_set:
            errors.append("道具 %s 与障碍同格 (%d,%d)" % (kind, cell[0], cell[1]))
    for cell in out["chests_cells"]:
        if cell in obstacle_set:
            errors.append("宝箱与障碍同格 (%d,%d)" % cell)

    # 冰面：能走进去，但踩上就站不住，所以不能放需要"停下来操作"的东西
    ice = parse_cell_list(adv.get("ice"), "adventure.ice", errors)
    if len(set(ice)) != len(ice):
        errors.append("adventure.ice 有重复格子")
    ice_set = set(ice)
    out["ice"] = ice
    if ice_set & obstacle_set:
        errors.append("冰面与障碍同格：%s"
                      % "、".join("(%d,%d)" % cell for cell in sorted(ice_set & obstacle_set)))
    # 正解格是要站定放小马的地方，冰面一踩就滑走，等于把这个正解格封死了
    for cell in sorted(ice_set & solution_cells):
        errors.append("冰面 (%d,%d) 压在正解格上：踩上去会一直滑走，放不了小马" % cell)
    for kind, cell in out["items"]:
        if cell in ice_set:
            errors.append("道具 %s 落在冰面 (%d,%d) 上，主角滑过去就捡不到"
                          % (kind, cell[0], cell[1]))
    for cell in out["chests_cells"]:
        if cell in ice_set:
            errors.append("宝箱落在冰面 (%d,%d) 上，主角停不下来打不开" % cell)
    if ice_set and out["start"] in ice_set:
        warnings.append("主角起点在冰面上：一进关按方向就会直接滑出去")
    if ice_set:
        # 孤立的单格冰没有意义：滑进去马上就滑出来，等于没有
        for cell in sorted(ice_set):
            if not any(other in ice_set for other in neighbours(*cell)):
                warnings.append("冰面 (%d,%d) 周围没有别的冰格，单格冰起不到滑行作用" % cell)

    # 塌陷木桥：初始可通行，走过一次（离开那一格）就塌成坑、不能再进
    bridges = parse_cell_list(adv.get("bridges"), "adventure.bridges", errors)
    if len(set(bridges)) != len(bridges):
        errors.append("adventure.bridges 有重复格子")
    bridge_set = set(bridges)
    out["bridges"] = bridges
    if bridge_set:
        if bridge_set & obstacle_set:
            errors.append("木桥与障碍同格：%s"
                          % "、".join("(%d,%d)" % cell for cell in sorted(bridge_set & obstacle_set)))
        for kind, cell in out["items"]:
            if cell in bridge_set:
                errors.append("道具 %s 落在木桥 (%d,%d) 上，桥一塌道具就掉进坑里"
                              % (kind, cell[0], cell[1]))
        for cell in out["chests_cells"]:
            if cell in bridge_set:
                errors.append("宝箱落在木桥 (%d,%d) 上，桥塌了就再也打不开" % cell)
        for cell in sorted(bridge_set):
            if cell in solution_cells:
                errors.append("木桥 (%d,%d) 在正解格上：桥要放小马，塌了就没法完成" % cell)
            if cell == out["start"]:
                errors.append("木桥 (%d,%d) 就是主角起点：一进关桥就塌了" % cell)
        if ice_set:
            errors.append("冰面与塌陷木桥不要放在同一关：滑行冲上木桥会把桥踏塌再回不去，规则解释不清")

    # 巡逻捣蛋鬼：路线逐格相邻，停留端点不能压在正解格上（策划案 4 节）
    patrols = adv.get("patrols") or []
    if not isinstance(patrols, list):
        errors.append("adventure.patrols 应为数组")
        patrols = []
    for index, patrol in enumerate(patrols):
        where = "adventure.patrols[%d]" % index
        if not isinstance(patrol, dict):
            errors.append("%s 应为对象" % where)
            continue
        path = parse_cell_list(patrol.get("path"), where + ".path", errors)
        if len(path) < 2:
            errors.append("%s.path 至少要 2 个格子（要能来回走），实际 %d 个"
                          % (where, len(path)))
            continue
        speed = patrol.get("speed", 1.5)
        if isinstance(speed, bool) or not isinstance(speed, (int, float)) or not 0.2 <= speed <= 8:
            errors.append("%s.speed 应在 0.2~8 之间（单位：格/秒），实际为 %r" % (where, speed))
        for step in range(len(path) - 1):
            here, there = path[step], path[step + 1]
            if abs(here[0] - there[0]) + abs(here[1] - there[1]) != 1:
                errors.append("%s.path 第 %d 段 %r -> %r 不相邻，只能上下左右走一格"
                              % (where, step, here, there))
        for cell in path:
            if cell in obstacle_set:
                errors.append("%s.path 经过障碍 %r，捣蛋鬼会被卡住" % (where, cell))
            if cell in bridge_set:
                errors.append("%s.path 经过木桥 %r：桥塌后捣蛋鬼会站在坑上，"
                              "也不该让它替玩家把桥走塌" % (where, cell))
        # 只检查两端的停留点：中途允许穿过正解格
        for label, cell in (("起点", path[0]), ("终点", path[-1])):
            if cell in solution_cells:
                errors.append("%s.path %s %r 是正解格，捣蛋鬼会长时间压在那一格"
                              % (where, label, cell))
            if cell == out["start"]:
                warnings.append("%s.path %s 与主角起点同格，一进关就会挨撞" % (where, label))
        out["patrols"].append({"path": path, "speed": float(speed)})

    # 推箱木箱：只记初始格；压力板来自宝箱的 puzzle.plate
    boxes_raw = adv.get("boxes") or []
    if not isinstance(boxes_raw, list):
        errors.append("adventure.boxes 应为数组")
        boxes_raw = []
    boxes = []
    for index, box in enumerate(boxes_raw):
        where = "adventure.boxes[%d]" % index
        if not isinstance(box, dict):
            errors.append("%s 应为对象" % where)
            continue
        cell = parse_cell(box.get("cell"), where + ".cell", errors)
        if cell is None:
            continue
        if cell in boxes:
            errors.append("%s 与另一只木箱同格 %r" % (where, cell))
        if cell in solution_cells:
            errors.append("%s 的初始格 %r 是正解格，木箱挡着就放不了小马" % (where, cell))
        if cell in obstacle_set:
            errors.append("%s 的初始格 %r 与障碍同格" % (where, cell))
        if cell in out["chests_cells"]:
            errors.append("%s 的初始格 %r 与宝箱同格" % (where, cell))
        if cell == out["start"]:
            errors.append("%s 的初始格 %r 就是主角起点，一进关就被卡住" % (where, cell))
        if cell in ice_set:
            errors.append("%s 的初始格 %r 在冰面上，冰上推不动箱子" % (where, cell))
        if cell in bridge_set:
            errors.append("%s 的初始格 %r 在木桥上，箱子推上桥会把桥压塌" % (where, cell))
        for item_kind, item_cell in out["items"]:
            if cell == item_cell:
                errors.append("%s 的初始格 %r 与道具 %s 同格" % (where, cell, item_kind))
        boxes.append(cell)
    out["boxes"] = boxes
    if boxes and ice_set:
        errors.append("冰面与木箱不要放在同一关：滑行时主角不推箱，两套规则会互相打架")
    if boxes and bridge_set:
        errors.append("塌陷木桥与木箱不要放在同一关：箱子推上桥算不算踩塌，规则还没定义")

    plates = []
    for index, chest in enumerate(chests):
        if not isinstance(chest, dict):
            continue
        puzzle = chest.get("puzzle") or {}
        if not isinstance(puzzle, dict) or puzzle.get("type") != "push":
            continue
        plate = as_cell(puzzle.get("plate"))
        if not isinstance(plate, tuple):
            continue
        where = "adventure.chests[%d].puzzle.plate" % index
        if plate in boxes:
            errors.append("%s 与木箱初始格同格 %r，开局就压住了" % (where, plate))
        if plate in obstacle_set:
            errors.append("%s 与障碍同格 %r" % (where, plate))
        if plate in out["chests_cells"]:
            errors.append("%s 与宝箱同格 %r" % (where, plate))
        if plate in ice_set:
            errors.append("%s 在冰面上 %r，木箱压上去也停不住" % (where, plate))
        if plate in bridge_set:
            errors.append("%s 在木桥上 %r，木箱压上去会把桥压塌" % (where, plate))
        for item_kind, item_cell in out["items"]:
            if plate == item_cell:
                errors.append("%s 与道具 %s 同格 %r，木箱压上去就拿不到了"
                              % (where, item_kind, plate))
        plates.append(plate)
    out["plates"] = plates
    if plates and not boxes:
        errors.append("有推箱宝箱（压力板 %s）却没有任何木箱" %
                      "、".join("(%d,%d)" % cell for cell in plates))
    if boxes and not plates:
        warnings.append("配了木箱但没有推箱宝箱的压力板，木箱只能推着玩")

    # 色区围栏：封住整个色区，踩下区域外的开关后永久打开（不可再关）。
    # 它是唯一允许盖住正解格的阻碍，所以这里不报「区域内有正解格」。
    gates_raw = adv.get("gates") or []
    if not isinstance(gates_raw, list):
        errors.append("adventure.gates 应为数组")
        gates_raw = []
    regions = level.get("regions") or []
    gates = []
    gate_region_cells = set()
    seen_gate_regions = set()
    seen_gate_switches = set()
    for index, gate in enumerate(gates_raw):
        where = "adventure.gates[%d]" % index
        if not isinstance(gate, dict):
            errors.append("%s 应为对象" % where)
            continue
        region = gate.get("region")
        if isinstance(region, bool) or not isinstance(region, int) or not 0 <= region < SIZE:
            errors.append("%s.region 应为 0~5 的色区编号，实际为 %r" % (where, region))
            continue
        if len(regions) != SIZE or any(len(row) != SIZE for row in regions):
            errors.append("%s：关卡 regions 结构不合法，无法确定围栏范围" % where)
            continue
        region_cells = {(r, c) for r in range(SIZE) for c in range(SIZE)
                        if regions[r][c] == region}
        if not region_cells:
            errors.append("%s.region = %d 在棋盘上没有对应的色区" % (where, region))
            continue
        if region in seen_gate_regions:
            errors.append("%s 与另一道围栏围住同一个色区 %d" % (where, region))
        seen_gate_regions.add(region)
        switch = parse_cell(gate.get("switch"), where + ".switch", errors)
        if switch is None:
            continue
        if switch in region_cells:
            errors.append("%s.switch %r 在围栏区域内：门关着时主角进不去，永远踩不到开关" % (where, switch))
        if switch in seen_gate_switches:
            errors.append("%s.switch %r 与另一道围栏的开关同格" % (where, switch))
        seen_gate_switches.add(switch)
        if switch in obstacle_set:
            errors.append("%s.switch %r 与障碍同格" % (where, switch))
        if switch in bridge_set:
            errors.append("%s.switch %r 在塌陷木桥上：桥一塌开关就踩不到了" % (where, switch))
        if switch in ice_set:
            errors.append("%s.switch %r 在冰面上：滑行经过不算踩，也停不住" % (where, switch))
        if switch in out["chests_cells"]:
            errors.append("%s.switch %r 与宝箱同格" % (where, switch))
        if switch in boxes:
            errors.append("%s.switch %r 与木箱同格" % (where, switch))
        for item_kind, item_cell in out["items"]:
            if switch == item_cell:
                errors.append("%s.switch %r 与道具 %s 同格" % (where, switch, item_kind))
        if switch == out["start"]:
            warnings.append("%s.switch 就是主角起点：一进关围栏就开了，这道围栏等于没有" % where)
        gate_region_cells |= region_cells
        gates.append({"region": region, "switch": switch, "cells": sorted(region_cells)})
    out["gates"] = gates
    out["gate_region_cells"] = gate_region_cells

    if gates:
        # 冰面与木箱都不能和围栏同关：前者是「滑行会不会冲过关着的门」没定义，
        # 后者是推箱状态里再叠一层开门状态、搜索维度兜不住。
        # 塌陷木桥则没有这个问题——桥不改变格子通不通（只是塌了变坑），
        # 搜索里 `used`（已塌桥）与 `opened`（已开围栏）两个掩码本来就并存。
        if ice_set:
            errors.append("色区围栏与冰面不要放在同一关：滑行会不会冲过关闭的围栏，规则还没定义")
        if boxes:
            errors.append("色区围栏与木箱不要放在同一关：推箱状态里还要再叠一层开门状态，实现前容易算错")
        # 捣蛋鬼的路线是静态数据，门是动态的；让它被门挡住会出现「开门前撞墙、开门后突然进来」
        for index, patrol in enumerate(out["patrols"]):
            inside = sorted(cell for cell in patrol["path"] if cell in gate_region_cells)
            if inside:
                errors.append("adventure.patrols[%d].path 进入围栏区域 %s：开门前捣蛋鬼会被门挡住，"
                              "开门后突然能进来，玩家没法预判"
                              % (index, "、".join("(%d,%d)" % cell for cell in inside)))
        # 开关格必须站得住：关门时它是唯一能改变局势的格子
        if out["start"] in gate_region_cells:
            errors.append("主角起点在围栏区域内：一进关就被关在里面")

    used = set()
    for key in UNIMPLEMENTED_KEYS:
        value = adv.get(key)
        if value:
            used.add(key)
    if used:
        warnings.append("用到尚未实现的机关 %s，par 按无障碍最短路径给出，实现后需复核"
                        % "、".join(sorted(used)))

    # 符文格与石碑格：必须能站上去，且不能和障碍/宝箱/道具/正解格撞在一起
    puzzle_cells = []
    for index, chest in enumerate(chests):
        if not isinstance(chest, dict):
            continue
        puzzle = chest.get("puzzle") or {}
        # 组合宝箱：then 链上的每一段都要收进格子检查，否则后续段的符文/石碑
        # 会和障碍、宝箱、道具撞在一起而没人发现
        for depth, stage in enumerate(puzzle_stages(puzzle)):
            where = "adventure.chests[%d].puzzle%s" % (index, ".then" * depth)
            kind = stage.get("type")
            if kind == "runes":
                for position, cell_value in enumerate(stage.get("order") or []):
                    puzzle_cells.append(("符文", as_cell(cell_value),
                                         "%s.order[%d]" % (where, position)))
            elif kind == "stele":
                clue = stage.get("clue") or {}
                if isinstance(clue, dict) and "cell" in clue:
                    puzzle_cells.append(("石碑", as_cell(clue["cell"]), where + ".clue.cell"))
                answer = as_cell(stage.get("answer"))
                chest_cell = as_cell(chest.get("cell"))
                if answer is not None and chest_cell is not None and answer != chest_cell:
                    errors.append("%s.answer 是 %r，但宝箱 cell 是 %r；埋藏格应与宝箱同格"
                                  % (where, answer, chest_cell))
    seen_puzzle = set()
    for kind, cell, where in puzzle_cells:
        if not isinstance(cell, tuple):
            continue
        if cell in obstacle_set:
            errors.append("%s %s 与障碍同格 (%d,%d)" % (where, kind, cell[0], cell[1]))
        if cell in out["chests_cells"]:
            errors.append("%s %s 与宝箱同格 (%d,%d)" % (where, kind, cell[0], cell[1]))
        for item_kind, item_cell in out["items"]:
            if cell == item_cell:
                errors.append("%s %s 与道具 %s 同格 (%d,%d)"
                              % (where, kind, item_kind, cell[0], cell[1]))
        if kind == "石碑" and cell in solution_cells:
            errors.append("%s 石碑立在正解格 (%d,%d) 上，那一格要放小马" % (where, cell[0], cell[1]))
        if cell in seen_puzzle:
            errors.append("%s 与另一处符文/石碑同格 (%d,%d)" % (where, cell[0], cell[1]))
        seen_puzzle.add(cell)
    if ice_set and puzzle_cells:
        errors.append("冰面关暂不支持符文/石碑宝箱：滑行会一路滑过符文，踩不准")
    # 塌陷木桥与符文/石碑宝箱可以同关：runes_solvable 已经带上「已塌桥掩码」，
    # 只要存在一条不塌掉必经桥的顺序就算合法；塌错了玩家自己按 Z 撤销。
    out["puzzle_cells"] = puzzle_cells
    out["chests_raw"] = chests

    # 宝箱可完成性：三星的第三颗星是「集齐本关宝箱」，一个走不到或开不了的宝箱
    # 等于这一关永远拿不到三星。这里按「围栏都能打开」算一次全图可达，
    # 把宝箱格与每一段的触发格都走一遍。
    if chests:
        walk_all = walkable_cells(out["obstacles"])
        reach_all = reachable(out["start"], walk_all, ice_set, bridge_set, gates)
        for index, chest in enumerate(chests):
            if not isinstance(chest, dict):
                continue
            chest_where = "adventure.chests[%d]" % index
            checks = [(chest_where + ".cell", as_cell(chest.get("cell")), "宝箱格")]
            for depth, stage in enumerate(puzzle_stages(chest.get("puzzle") or {})):
                stage_where = "adventure.chests[%d].puzzle%s" % (index, ".then" * depth)
                kind = stage.get("type")
                if kind == "runes":
                    for position, cell_value in enumerate(stage.get("order") or []):
                        checks.append(("%s.order[%d]" % (stage_where, position),
                                       as_cell(cell_value), "符文格"))
                elif kind == "stele":
                    clue = stage.get("clue") or {}
                    if isinstance(clue, dict) and "cell" in clue:
                        checks.append((stage_where + ".clue.cell", as_cell(clue["cell"]), "石碑格"))
                elif kind == "push":
                    checks.append((stage_where + ".plate", as_cell(stage.get("plate")), "压力板"))
            for cell_where, cell, label in checks:
                if not isinstance(cell, tuple):
                    continue
                if cell not in reach_all:
                    errors.append("%s %s %r 从起点走不到（按围栏全部打开计算），这个宝箱开不了"
                                  % (cell_where, label, cell))

    return out


def check_puzzle_def(puzzle, where, errors, solution_cells, depth=0):
    kind = puzzle.get("type")
    if kind not in PUZZLE_TYPES:
        errors.append("%s.type 应为 %s 之一，实际为 %r"
                      % (where, "/".join(PUZZLE_TYPES), kind))
        return
    if kind == "rule":
        target = puzzle.get("target")
        if target not in ("row", "col", "column", "region"):
            errors.append("%s.target 应为 row/col/region，实际为 %r" % (where, target))
        index = puzzle.get("index")
        if not isinstance(index, int) or not 0 <= index < SIZE:
            errors.append("%s.index 应为 0~%d，实际为 %r" % (where, SIZE - 1, index))
    elif kind == "runes":
        order = puzzle.get("order")
        if not isinstance(order, list) or not 2 <= len(order) <= 4:
            errors.append("%s.order 应为 2~4 个符文格" % where)
            return
        seen = []
        for position, cell_value in enumerate(order):
            cell = parse_cell(cell_value, "%s.order[%d]" % (where, position), errors)
            if cell is None:
                continue
            if cell in solution_cells:
                errors.append("%s.order[%d] 在正解格 %r 上，会把符文踩成小马" % (where, position, cell))
            if cell in seen:
                errors.append("%s.order 有重复格 %r" % (where, cell))
            seen.append(cell)
    elif kind == "stele":
        clue = puzzle.get("clue")
        if not isinstance(clue, dict) or not isinstance(clue.get("text"), str):
            errors.append("%s 需要 clue.text 文本" % where)
        answer = parse_cell(puzzle.get("answer"), where + ".answer", errors)
        if answer is not None and answer in solution_cells:
            errors.append("%s.answer 是正解格 %r，挖宝会踩到小马" % (where, answer))
    elif kind == "push":
        # 压力板记在宝箱上；木箱只记自己的初始格，写在 adventure.boxes 里
        plate = parse_cell(puzzle.get("plate"), where + ".plate", errors)
        if plate is not None and plate in solution_cells:
            errors.append("%s.plate 在正解格 %r 上，木箱压上去就放不了小马" % (where, plate))
        if "box" in puzzle:
            errors.append("%s 不再使用 box 字段，木箱统一写在 adventure.boxes 里" % where)
    then = puzzle.get("then")
    if then is not None:
        if depth + 1 >= MAX_PUZZLE_STAGES:
            errors.append("%s.then 已经到第 %d 段，组合宝箱最多 %d 段：再多玩家记不住顺序"
                          % (where, depth + 2, MAX_PUZZLE_STAGES))
        elif not isinstance(then, dict):
            errors.append("%s.then 应为嵌套谜题对象" % where)
        else:
            check_puzzle_def(then, where + ".then", errors, solution_cells, depth + 1)


# --------------------------------------------------------------------------
# 可达性与标准步数
# --------------------------------------------------------------------------

def as_cell(value):
    """把 [行, 列] 统一成元组，方便做集合与字典的键。"""
    if isinstance(value, (list, tuple)) and len(value) == 2:
        return tuple(value)
    return value


def walkable_cells(obstacles):
    blocked = {as_cell(cell) for cell in obstacles}
    return {(r, c) for r in range(SIZE) for c in range(SIZE)} - blocked


def slide_step(cell, direction, walkable, ice=frozenset()):
    """从 cell 沿 direction 走一步的真实落点，以及这一步跨过了几格。

    普通地板走一格；一踩上冰面就停不下来，只能一路滑：
    - 滑出冰面：落在冰后面第一块普通地板上（游戏里也是滑过去才停）；
    - 撞上墙或障碍：停在最后一块冰上，那一格就成了"冰上落脚点"。
    滑不动（前面直接被挡住）返回 None。
    """
    dr, dc = direction
    nxt = (cell[0] + dr, cell[1] + dc)
    if nxt not in walkable:
        return None
    if nxt not in ice:
        return nxt, 1
    current = nxt
    cost = 1
    while True:
        ahead = (current[0] + dr, current[1] + dc)
        if ahead not in walkable:
            return current, cost
        if ahead not in ice:
            return ahead, cost + 1
        current = ahead
        cost += 1


def bridge_step_blocked(cell, bridge_index, used):
    """这一格是不是"已经塌掉的木桥"。used 是已用掉的桥的位掩码。"""
    bit = bridge_index.get(cell)
    return bit is not None and bool(used & bit)


def gate_tables(gates):
    """把围栏整理成 BFS 用的三张表：开关格->位、色区->位、格->色区。"""
    switch_bits = {}
    region_bits = {}
    region_of = {}
    for index, gate in enumerate(gates or []):
        bit = 1 << index
        switch = as_cell(gate["switch"])
        switch_bits[switch] = bit
        region_bits[gate["region"]] = bit
        for cell in gate.get("cells") or []:
            region_of[as_cell(cell)] = gate["region"]
    return switch_bits, region_bits, region_of


def gate_blocked(cell, region_of, region_bits, opened):
    """这一格是否被一道还没打开的围栏挡住。opened 是已开围栏的位掩码。"""
    bit = region_bits.get(region_of.get(cell))
    return bit is not None and not (opened & bit)


def reachable(start, walkable, ice=frozenset(), bridges=frozenset(), gates=()):
    """能站到的格子集合。

    有木桥时状态要带上"哪几座桥已经用掉"：桥塌了就永久不能再进，
    所以同一个格子可能在"桥还在"和"桥已塌"两种历史下有不同的可达性。
    围栏同理，状态再叠一层"哪几道围栏已经打开"——门只能在开关格打开，且一开就不再关。
    """
    start = as_cell(start)
    if start is None or start not in walkable:
        return set()
    bridge_index = {as_cell(cell): 1 << index for index, cell in enumerate(sorted(bridges))}
    switch_bits, region_bits, region_of = gate_tables(gates)
    start_opened = switch_bits.get(start, 0)
    seen = {(start, 0, start_opened)}
    reached = {start}
    todo = deque([(start, 0, start_opened)])
    while todo:
        cell, used, opened = todo.popleft()
        for direction in DIRECTIONS:
            nxt = (cell[0] + direction[0], cell[1] + direction[1])
            if bridge_step_blocked(nxt, bridge_index, used):
                continue
            if gate_blocked(nxt, region_of, region_bits, opened):
                continue
            step = slide_step(cell, direction, walkable, ice)
            if step is None:
                continue
            other = step[0]
            # 离开桥格的那一刻桥就塌了，之后的搜索里它变成坑
            new_used = used | bridge_index.get(cell, 0)
            # 踩到开关格就开门；滑行经过开关格同样算踩到
            new_opened = opened | switch_bits.get(nxt, 0) | switch_bits.get(other, 0)
            key = (other, new_used, new_opened)
            if key in seen:
                continue
            seen.add(key)
            reached.add(other)
            todo.append(key)
    return reached


def min_steps(start, targets, walkable, ice=frozenset(), bridges=frozenset(), gates=()):
    """从起点走遍全部 targets 的最少“进入新格子”次数。

    状态 = (当前格, 已踩过的目标集合, 已塌的桥集合, 已开的围栏集合)。没有冰和桥时每步
    都是 1 格，退化成普通广度优先；有冰时一步可能滑出好几格，代价等于跨过的格数，所以用
    Dijkstra 按代价展开。
    """
    start = as_cell(start)
    if start is None or start not in walkable:
        return None
    targets = [as_cell(cell) for cell in targets]
    if not targets:
        return 0
    index_of = {cell: bit for bit, cell in enumerate(targets)}
    if any(cell not in walkable for cell in targets):
        return None
    bridge_index = {as_cell(cell): 1 << index for index, cell in enumerate(sorted(bridges))}
    switch_bits, region_bits, region_of = gate_tables(gates)
    full = (1 << len(targets)) - 1
    start_mask = 1 << index_of[start] if start in index_of else 0
    if start_mask == full:
        return 0
    start_opened = switch_bits.get(start, 0)

    best = {(start, start_mask, 0, start_opened): 0}
    heap = [(0, start, start_mask, 0, start_opened)]
    best_full = INF
    while heap:
        cost, cell, mask, used, opened = heapq.heappop(heap)
        if cost > best.get((cell, mask, used, opened), INF):
            continue
        if cost >= best_full:
            break
        for direction in DIRECTIONS:
            nxt = (cell[0] + direction[0], cell[1] + direction[1])
            if bridge_step_blocked(nxt, bridge_index, used):
                continue
            if gate_blocked(nxt, region_of, region_bits, opened):
                continue
            step = slide_step(cell, direction, walkable, ice)
            if step is None:
                continue
            other, step_cost = step
            new_used = used | bridge_index.get(cell, 0)
            new_opened = opened | switch_bits.get(nxt, 0) | switch_bits.get(other, 0)
            new_mask = mask | (1 << index_of[other]) if other in index_of else mask
            new_cost = cost + step_cost
            if new_mask == full:
                best_full = min(best_full, new_cost)
                continue
            key = (other, new_mask, new_used, new_opened)
            if new_cost < best.get(key, INF):
                best[key] = new_cost
                heapq.heappush(heap, (new_cost, other, new_mask, new_used, new_opened))
    return None if best_full == INF else best_full


def par_from(minimum):
    return int(math.ceil(minimum * PAR_FACTOR))


def box_push_states(start, boxes, walkable, box_blockers):
    """推箱状态搜索：状态 = (主角格, 排序后的木箱格元组)。

    木箱整格推动：主角走进箱子那一格，箱子往同方向挪一格；挪不动（出界、
    撞障碍、撞别的箱子、压在道具/宝箱上）就走不过去。
    返回全部可达状态，同时供「正解格能不能走到」与「木箱能不能压到压力板」使用。
    """
    start = as_cell(start)
    if start is None or start not in walkable:
        return set()
    initial = tuple(sorted(as_cell(box) for box in boxes))
    seen = {(start, initial)}
    todo = deque([(start, initial)])
    while todo:
        player, state_boxes = todo.popleft()
        for nxt in neighbours(*player):
            if nxt in state_boxes:
                index = state_boxes.index(nxt)
                step = (nxt[0] - player[0], nxt[1] - player[1])
                target = (nxt[0] + step[0], nxt[1] + step[1])
                if target in state_boxes or target not in walkable or target in box_blockers:
                    continue
                moved = list(state_boxes)
                moved[index] = target
                key = (nxt, tuple(sorted(moved)))
            elif nxt in walkable:
                key = (nxt, state_boxes)
            else:
                continue
            if key in seen:
                continue
            seen.add(key)
            todo.append(key)
    return seen


def runes_solvable(start, order, walkable, bridges=frozenset()):
    """符文必须按顺序踩，踩到「还没轮到」的符文会全部熄灭。

    所以除了可达性，还要确认存在一条路：能依次点亮每个符文，
    且途中不会被迫踩到后面才该踩的符文。

    有塌陷木桥时状态再带上「哪几座桥已经用掉」：桥塌了就永久不能再进，
    同一格在「桥还在」和「桥已塌」两种历史下的可行性不同，必须分开记。
    """
    start = as_cell(start)
    order = [as_cell(cell) for cell in order]
    if start is None or start not in walkable or not order:
        return False
    index_of = {cell: index for index, cell in enumerate(order)}
    bridge_index = {as_cell(cell): 1 << index for index, cell in enumerate(sorted(bridges))}
    progress = 0
    if start in index_of:
        if index_of[start] != 0:
            return False
        progress = 1
    full = len(order)
    if progress == full:
        return True
    seen = {(start, progress, 0)}
    todo = deque([(start, progress, 0)])
    while todo:
        cell, prog, used = todo.popleft()
        for other in neighbours(*cell):
            if other not in walkable:
                continue
            if bridge_step_blocked(other, bridge_index, used):
                continue
            # 离开桥格的那一刻桥就塌了
            new_used = used | bridge_index.get(cell, 0)
            new_prog = prog
            if other in index_of:
                index = index_of[other]
                if index == prog:
                    new_prog = prog + 1
                elif index > prog:
                    continue  # 踩到还没轮到的符文，必然重来，这条路不可行
            if new_prog == full:
                return True
            if (other, new_prog, new_used) not in seen:
                seen.add((other, new_prog, new_used))
                todo.append((other, new_prog, new_used))
    return False


# --------------------------------------------------------------------------
# 棋盘预览
# --------------------------------------------------------------------------

def render_board(level, adv):
    regions = level["regions"]
    solution = level.get("solution") or []
    solution_cells = {(r, solution[r]) for r in range(SIZE) if r < len(solution)}
    chests = {as_cell(chest["cell"]): chest.get("puzzle") for chest in (adv["chests"] if adv else [])}
    items = {as_cell(cell): kind for kind, cell in (adv["items"] if adv else [])}
    obstacles = {as_cell(cell) for cell in (adv["obstacles"] if adv else [])}
    ice_cells = {as_cell(cell) for cell in (adv.get("ice") or [])} if adv else set()
    bridge_cells = {as_cell(cell) for cell in (adv.get("bridges") or [])} if adv else set()
    patrol_ends = set()
    patrol_paths = set()
    for patrol in (adv.get("patrols") or [] if adv else []):
        # 既接受 check_adventure 归一化后的 (row, col)，也接受 JSON 里的 [row, col]
        path = [as_cell(cell) for cell in patrol.get("path") or []]
        path = [cell for cell in path if cell is not None]
        if not path:
            continue
        patrol_ends.update({path[0], path[-1]})
        patrol_paths.update(path)
    start = as_cell(adv["start"]) if adv else None

    rune_cells = set()
    stele_cells = set()
    for chest in (adv["chests"] if adv else []):
        # check_adventure 输出的 chest 里 puzzle 是类型字符串，完整定义在 puzzle_def
        puzzle = chest.get("puzzle_def") or chest.get("puzzle")
        if not isinstance(puzzle, dict):
            continue
        # 组合宝箱：then 链上每一段的符文/石碑都要画出来，否则预览会漏掉后半段
        for stage in puzzle_stages(puzzle):
            if stage.get("type") == "runes":
                rune_cells |= {as_cell(cell) for cell in stage.get("order") or []}
            elif stage.get("type") == "stele":
                clue = stage.get("clue")
                if isinstance(clue, dict) and "cell" in clue:
                    stele_cells.add(as_cell(clue["cell"]))

    # adv 可能是 check_adventure 归一化后的结果，也可能是原始 JSON；两种形态都要认
    boxes = set()
    for box in (adv.get("boxes") or []) if adv else []:
        cell = as_cell(box.get("cell")) if isinstance(box, dict) else as_cell(box)
        if isinstance(cell, tuple):
            boxes.add(cell)
    plates = set()
    for chest in (adv["chests"] if adv else []):
        puzzle = chest.get("puzzle_def") or chest.get("puzzle") or {}
        if isinstance(puzzle, dict) and puzzle.get("type") == "push":
            plate = as_cell(puzzle.get("plate"))
            if isinstance(plate, tuple):
                plates.add(plate)

    gate_switches = set()
    gate_cells = set()
    for gate in (adv.get("gates") or []) if adv else []:
        if not isinstance(gate, dict):
            continue
        switch = as_cell(gate.get("switch"))
        if isinstance(switch, tuple):
            gate_switches.add(switch)
        for cell in gate.get("cells") or []:
            cell = as_cell(cell)
            if isinstance(cell, tuple):
                gate_cells.add(cell)

    lines = []
    for row in range(SIZE):
        region_line = []
        symbol_line = []
        for col in range(SIZE):
            cell = (row, col)
            region_line.append(str(regions[row][col]))
            symbol = "."
            # 围栏区域先铺底色，正解格、道具、宝箱等会盖在它上面
            if cell in gate_cells:
                symbol = "|"
            if cell in ice_cells:
                symbol = "~"
            if cell in bridge_cells:
                symbol = "="
            if cell in solution_cells:
                symbol = "*"
            if cell in items:
                symbol = ITEM_SYMBOL.get(items[cell], "?")
            if cell in rune_cells:
                symbol = "r"
            if cell in stele_cells:
                symbol = "T"
            if cell in chests:
                symbol = "C"
            if cell in obstacles:
                symbol = "#"
            if cell in plates:
                symbol = "P"
            if cell in gate_switches:
                symbol = "G"
            if cell in boxes:
                symbol = "B"
            if cell in patrol_paths:
                symbol = "o"
            if cell in patrol_ends:
                symbol = "p"
            if cell == start:
                symbol = "S"
            symbol_line.append(symbol)
        lines.append("    %s        %s" % (" ".join(region_line), " ".join(symbol_line)))
    return lines


def describe(level, adv, minimum, computed_par):
    lines = []
    lines.append("第 %02d 关   开局生命 %s   最短 %s 步   par %s（现为 %s）"
                 % (level.get("id", 0),
                    (level.get("adventure") or {}).get("lives", "?"),
                    "无法到达" if minimum is None else minimum,
                    "?" if computed_par is None else computed_par,
                    (level.get("adventure") or {}).get("par", "?")))
    lines.append("    色区布局      符号：* 正解  S 起点  C 宝箱  h/a/c 道具  # 障碍  r 符文  T 石碑  p 巡逻端点  o 巡逻路线  B 木箱  P 压力板  ~ 冰面  = 木桥  | 围栏内  G 围栏开关")
    for line in render_board(level, adv):
        lines.append(line)
    if adv:
        items = "、".join("%s(%d,%d)" % (kind, cell[0], cell[1]) for kind, cell in adv["items"])
        lines.append("    道具：%s" % (items or "无"))
        chests = "、".join("%s@(%d,%d)" % ("->".join(str(part) for part in chest.get("chain") or [chest["puzzle"]]),
                                          chest["cell"][0], chest["cell"][1])
                          for chest in adv["chests"])
        lines.append("    宝箱：%s" % (chests or "无"))
        patrols = "、".join("%s 速%.1f" % ("->".join("(%d,%d)" % c for c in patrol["path"]),
                                         patrol["speed"])
                           for patrol in adv.get("patrols") or [])
        lines.append("    巡逻：%s" % (patrols or "无"))
        boxes = "、".join("(%d,%d)" % cell for cell in adv.get("boxes") or [])
        plates = "、".join("(%d,%d)" % cell for cell in adv.get("plates") or [])
        if boxes or plates:
            lines.append("    推箱：木箱 %s   压力板 %s" % (boxes or "无", plates or "无"))
        ice = "、".join("(%d,%d)" % tuple(cell) for cell in adv.get("ice") or [])
        if ice:
            lines.append("    冰面：%s" % ice)
        bridges = "、".join("(%d,%d)" % tuple(cell) for cell in adv.get("bridges") or [])
        if bridges:
            lines.append("    木桥：%s" % bridges)
        gates = "、".join("色区 %d 开关(%d,%d)" % (gate["region"], gate["switch"][0], gate["switch"][1])
                          for gate in adv.get("gates") or [])
        if gates:
            lines.append("    围栏：%s" % gates)
    return lines


# --------------------------------------------------------------------------
# 主流程
# --------------------------------------------------------------------------

def validate(data, verbose=True):
    errors = []
    warnings = []
    results = []

    levels = data.get("levels") or []
    if len(levels) != 25:
        errors.append("关卡数应为 25，实际 %d" % len(levels))
    for expected_id, level in enumerate(levels, 1):
        if level.get("id") != expected_id:
            errors.append("关卡顺序错乱：第 %d 个对象的 id 是 %r" % (expected_id, level.get("id")))

    seen_signatures = set()
    for level in levels:
        level_errors = []
        level_warnings = []
        check_puzzle(level, level_errors)
        signature = _signature(level.get("regions"))
        if signature is not None:
            if signature in seen_signatures:
                level_errors.append("与前面的关卡是同一布局的旋转/镜像/换色")
            seen_signatures.add(signature)

        adv = check_adventure(level, level_errors, level_warnings)

        minimum = None
        computed_par = None
        if adv and adv["start"] is not None:
            walkable = walkable_cells(adv["obstacles"])
            ice = set(adv.get("ice") or [])
            bridges = set(adv.get("bridges") or [])
            gates = adv.get("gates") or []
            reached = reachable(adv["start"], walkable, ice, bridges, gates)
            boxes = adv.get("boxes") or []
            if boxes:
                # 有木箱时不能按「静态障碍」算可达性：木箱是可移动的，
                # 要把 (主角格, 箱子格) 作为状态搜一遍才知道哪些格真的能站上去。
                box_blockers = (set(as_cell(cell) for cell in adv["chests_cells"])
                                | {as_cell(cell) for _, cell in adv["items"]})
                states = box_push_states(adv["start"], boxes, walkable, box_blockers)
                reached = {state[0] for state in states}
                for plate in adv.get("plates") or []:
                    if not any(plate in state[1] for state in states):
                        level_errors.append("木箱推不到压力板 (%d,%d)" % plate)
            solution = level.get("solution") or []
            targets = [(r, solution[r]) for r in range(SIZE) if r < len(solution)]
            missing = [cell for cell in targets if cell not in reached]
            if missing:
                level_errors.append("起点到不了正解格 %s" % ", ".join(
                    "(%d,%d)" % cell for cell in missing))
            for cell in list(adv["chests_cells"]) + [cell for _, cell in adv["items"]]:
                if cell not in reached:
                    level_errors.append("起点到不了 (%d,%d)" % cell)
            for kind, cell, where in (adv.get("puzzle_cells") or []):
                if isinstance(cell, tuple) and cell not in reached:
                    level_errors.append("%s 的 %s 在 (%d,%d)，起点走不过去"
                                        % (where, kind, cell[0], cell[1]))
            # 符文顺序必须真的能按顺序走完（踩错就全灭），链式宝箱每一段都要查
            for index, chest in enumerate(adv.get("chests_raw") or []):
                if not isinstance(chest, dict):
                    continue
                for depth, stage in enumerate(puzzle_stages(chest.get("puzzle") or {})):
                    if stage.get("type") != "runes":
                        continue
                    order = [as_cell(cell) for cell in stage.get("order") or []]
                    if order and not runes_solvable(adv["start"], order, walkable, bridges):
                        level_errors.append("adventure.chests[%d].puzzle%sthen 的符文顺序走不通："
                                            "必须先踩后面才该踩的符文才能到达" % (index, ".then" * depth))

            # 三星的第三颗星是「集齐本关宝箱」，所以标准路线也必须把宝箱谜题里
            # 必须踩到的格子算进去，否则「步数达标」和「集齐」两项互相打架，三星永远拿不到。
            puzzle_targets = list(targets) + chest_route_cells(adv.get("chests_raw"))
            if not missing:
                minimum = min_steps(adv["start"], puzzle_targets, walkable, ice, bridges, gates)
                if minimum is not None:
                    computed_par = par_from(minimum)
                    if isinstance(adv["par"], int) and adv["par"] != computed_par:
                        level_warnings.append("par 现为 %d，按 1.3 倍应为 %d（最短 %d 步）"
                                              % (adv["par"], computed_par, minimum))

        entry = {
            "id": level.get("id"),
            "errors": level_errors,
            "warnings": level_warnings,
            "minimum": minimum,
            "par": computed_par,
            "current_par": (level.get("adventure") or {}).get("par"),
        }
        results.append(entry)
        errors.extend("第 %02d 关：%s" % (entry["id"], message) for message in level_errors)
        warnings.extend("第 %02d 关：%s" % (entry["id"], message) for message in level_warnings)

    return results, errors, warnings


def _signature(regions):
    if not isinstance(regions, list) or len(regions) != SIZE:
        return None
    if any(not isinstance(row, list) or len(row) != SIZE for row in regions):
        return None
    variants = []
    grid = [row[:] for row in regions]
    for _ in range(4):
        for rows in (grid, [list(reversed(row)) for row in grid]):
            labels = {}
            variants.append(tuple(labels.setdefault(value, len(labels))
                                  for row in rows for value in row))
        grid = [list(row) for row in zip(*grid[::-1])]
    return min(variants)


def dump_like_original(data, original_text):
    """按原文件的缩进、换行符与结尾换行写回，避免整份文件的无意义 diff。"""
    newline = "\r\n" if "\r\n" in original_text else "\n"
    text = json.dumps(data, indent=2, ensure_ascii=False)
    if newline != "\n":
        text = text.replace("\n", newline)
    if original_text.endswith(newline):
        text += newline
    return text


def write_pars(data, results, path=LEVELS_PATH):
    path = Path(path)
    with open(path, "r", encoding="utf-8", newline="") as handle:
        original_text = handle.read()
    by_id = {level["id"]: level for level in data["levels"]}
    changed = []
    for entry in results:
        if entry["errors"] or entry["par"] is None:
            continue
        level = by_id.get(entry["id"])
        if level is None:
            continue
        adventure = level.setdefault("adventure", {})
        if adventure.get("par") != entry["par"]:
            changed.append((entry["id"], adventure.get("par"), entry["par"]))
            adventure["par"] = entry["par"]
    if changed:
        with open(path, "w", encoding="utf-8", newline="") as handle:
            handle.write(dump_like_original(data, original_text))
    return changed


def main(argv=None):
    parser = argparse.ArgumentParser(description="校验小马逻辑冒险版关卡数据")
    parser.add_argument("--level", type=int, help="只看某一关（1~25），并打印棋盘预览")
    parser.add_argument("--write-par", action="store_true", help="把算出的 par 写回 levels.json")
    parser.add_argument("--quiet", action="store_true", help="只输出问题，不打印每关概览")
    parser.add_argument("--path", default=str(LEVELS_PATH), help="关卡文件路径")
    args = parser.parse_args(argv)

    data = load_levels(args.path)
    results, errors, warnings = validate(data)

    if args.level:
        chosen = [entry for entry in results if entry["id"] == args.level]
        if not chosen:
            print("没有第 %d 关" % args.level)
            return 1
        level = data["levels"][args.level - 1]
        adv = None
        for candidate in [level]:
            adv_errors = []
            adv_warnings = []
            adv = check_adventure(candidate, adv_errors, adv_warnings)
            for message in adv_errors:
                print("错误：" + message)
            for message in adv_warnings:
                print("提示：" + message)
        for line in describe(level, adv, chosen[0]["minimum"], chosen[0]["par"]):
            print(line)
        print("")
        if chosen[0]["errors"]:
            for message in chosen[0]["errors"]:
                print("错误：" + message)
            return 1
        print("通过")
        return 0

    if not args.quiet:
        header = "%-4s %-6s %-6s %-6s %s" % ("关", "最短", "算出", "现有", "状态")
        print(header)
        print("-" * len(header))
        for entry in results:
            state = "错误" if entry["errors"] else ("par 待更新" if entry["warnings"] else "通过")
            print("%-4d %-6s %-6s %-6s %s" % (
                entry["id"],
                "-" if entry["minimum"] is None else entry["minimum"],
                "-" if entry["par"] is None else entry["par"],
                "-" if entry["current_par"] is None else entry["current_par"],
                state))
        print("")

    if args.write_par:
        changed = write_pars(data, results, args.path)
        if changed:
            print("已写入 %d 关的 par：" % len(changed))
            for level_id, before, after in changed:
                print("  第 %02d 关  %s -> %d" % (level_id, "-" if before is None else before, after))
        else:
            print("par 已是最新，无需写入")
        print("")

    for message in errors:
        print("错误：" + message)
    if not args.quiet:
        for message in warnings:
            print("提示：" + message)

    if errors:
        print("\n共 %d 处错误，%d 处提示" % (len(errors), len(warnings)))
        return 1
    print("\n全部 %d 关通过，%d 处提示" % (len(results), len(warnings)))
    return 0


if __name__ == "__main__":
    sys.exit(main())

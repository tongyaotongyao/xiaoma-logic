"""校验《小马逻辑》冒险版关卡数据。

对 `Content/Levels/levels.json` 的每一关做两层检查：

1. 基础谜题不变量：六色区形状、每色 3~9 格且四向连通、空开局、720 种列排列穷举下唯一解。
2. 冒险层：起点、生命、道具、宝箱的位置与类型；障碍类机关是否落在正解格上；
   从起点出发的可达性（广度优先搜索）；以及按“走遍 6 个正解格的最短步数 × 1.3 向上取整”
   算出的标准步数 par。

用法：
    python Tools/validate_adventure.py                 只报告，不改文件
    python Tools/validate_adventure.py --level 7       单独看第 7 关的棋盘预览
    python Tools/validate_adventure.py --write-par     把算出的 par 写回 levels.json
    python Tools/validate_adventure.py --quiet         只输出错误

退出码：0 表示全部通过，1 表示有问题。
"""

from __future__ import annotations

import argparse
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
UNIMPLEMENTED_KEYS = ("ice", "bridges", "gates", "steles", "boxes")


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
        # 只检查两端的停留点：中途允许穿过正解格
        for label, cell in (("起点", path[0]), ("终点", path[-1])):
            if cell in solution_cells:
                errors.append("%s.path %s %r 是正解格，捣蛋鬼会长时间压在那一格"
                              % (where, label, cell))
            if cell == out["start"]:
                warnings.append("%s.path %s 与主角起点同格，一进关就会挨撞" % (where, label))
        out["patrols"].append({"path": path, "speed": float(speed)})

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
        where = "adventure.chests[%d].puzzle" % index
        kind = puzzle.get("type")
        if kind == "runes":
            for position, cell_value in enumerate(puzzle.get("order") or []):
                puzzle_cells.append(("符文", as_cell(cell_value),
                                     "%s.order[%d]" % (where, position)))
        elif kind == "stele":
            clue = puzzle.get("clue") or {}
            if isinstance(clue, dict) and "cell" in clue:
                puzzle_cells.append(("石碑", as_cell(clue["cell"]), where + ".clue.cell"))
            answer = as_cell(puzzle.get("answer"))
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
    out["puzzle_cells"] = puzzle_cells
    out["chests_raw"] = chests

    return out


def check_puzzle_def(puzzle, where, errors, solution_cells):
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
        box = parse_cell(puzzle.get("box"), where + ".box", errors)
        plate = parse_cell(puzzle.get("plate"), where + ".plate", errors)
        for label, cell in (("box", box), ("plate", plate)):
            if cell is not None and cell in solution_cells:
                errors.append("%s.%s 在正解格 %r 上" % (where, label, cell))
    then = puzzle.get("then")
    if then is not None:
        if not isinstance(then, dict):
            errors.append("%s.then 应为嵌套谜题对象" % where)
        else:
            check_puzzle_def(then, where + ".then", errors, solution_cells)


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


def reachable(start, walkable):
    start = as_cell(start)
    if start is None or start not in walkable:
        return set()
    seen = {start}
    todo = deque([start])
    while todo:
        row, col = todo.popleft()
        for other in neighbours(row, col):
            if other in walkable and other not in seen:
                seen.add(other)
                todo.append(other)
    return seen


def min_steps(start, targets, walkable):
    """从起点走遍全部 targets 的最少“进入新格子”次数。

    状态 = (当前格, 已踩过的目标集合)，每走一格算一步，广度优先保证最短。
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
    full = (1 << len(targets)) - 1
    start_mask = 1 << index_of[start] if start in index_of else 0
    if start_mask == full:
        return 0
    seen = {(start, start_mask)}
    todo = deque([(start, start_mask, 0)])
    while todo:
        cell, mask, steps = todo.popleft()
        for other in neighbours(*cell):
            if other not in walkable:
                continue
            new_mask = mask | (1 << index_of[other]) if other in index_of else mask
            if new_mask == full:
                return steps + 1
            key = (other, new_mask)
            if key not in seen:
                seen.add(key)
                todo.append((other, new_mask, steps + 1))
    return None


def par_from(minimum):
    return int(math.ceil(minimum * PAR_FACTOR))


def runes_solvable(start, order, walkable):
    """符文必须按顺序踩，踩到「还没轮到」的符文会全部熄灭。

    所以除了可达性，还要确认存在一条路：能依次点亮每个符文，
    且途中不会被迫踩到后面才该踩的符文。
    """
    start = as_cell(start)
    order = [as_cell(cell) for cell in order]
    if start is None or start not in walkable or not order:
        return False
    index_of = {cell: index for index, cell in enumerate(order)}
    progress = 0
    if start in index_of:
        if index_of[start] != 0:
            return False
        progress = 1
    full = len(order)
    if progress == full:
        return True
    seen = {(start, progress)}
    todo = deque([(start, progress)])
    while todo:
        cell, prog = todo.popleft()
        for other in neighbours(*cell):
            if other not in walkable:
                continue
            new_prog = prog
            if other in index_of:
                index = index_of[other]
                if index == prog:
                    new_prog = prog + 1
                elif index > prog:
                    continue  # 踩到还没轮到的符文，必然重来，这条路不可行
            if new_prog == full:
                return True
            if (other, new_prog) not in seen:
                seen.add((other, new_prog))
                todo.append((other, new_prog))
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
        if puzzle.get("type") == "runes":
            rune_cells |= {as_cell(cell) for cell in puzzle.get("order") or []}
        elif puzzle.get("type") == "stele":
            clue = puzzle.get("clue")
            if isinstance(clue, dict) and "cell" in clue:
                stele_cells.add(as_cell(clue["cell"]))

    lines = []
    for row in range(SIZE):
        region_line = []
        symbol_line = []
        for col in range(SIZE):
            cell = (row, col)
            region_line.append(str(regions[row][col]))
            symbol = "."
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
    lines.append("    色区布局      符号：* 正解  S 起点  C 宝箱  h/a/c 道具  # 障碍  r 符文  T 石碑  p 巡逻端点  o 巡逻路线")
    for line in render_board(level, adv):
        lines.append(line)
    if adv:
        items = "、".join("%s(%d,%d)" % (kind, cell[0], cell[1]) for kind, cell in adv["items"])
        lines.append("    道具：%s" % (items or "无"))
        chests = "、".join("%s@(%d,%d)" % (chest["puzzle"], chest["cell"][0], chest["cell"][1])
                          for chest in adv["chests"])
        lines.append("    宝箱：%s" % (chests or "无"))
        patrols = "、".join("%s 速%.1f" % ("->".join("(%d,%d)" % c for c in patrol["path"]),
                                         patrol["speed"])
                           for patrol in adv.get("patrols") or [])
        lines.append("    巡逻：%s" % (patrols or "无"))
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
            reached = reachable(adv["start"], walkable)
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
            # 符文顺序必须真的能按顺序走完（踩错就全灭）
            for index, chest in enumerate(adv.get("chests_raw") or []):
                puzzle = (chest.get("puzzle") or {}) if isinstance(chest, dict) else {}
                if puzzle.get("type") != "runes":
                    continue
                order = [as_cell(cell) for cell in puzzle.get("order") or []]
                if order and not runes_solvable(adv["start"], order, walkable):
                    level_errors.append("adventure.chests[%d] 的符文顺序走不通："
                                        "必须先踩后面才该踩的符文才能到达" % index)
            if not missing:
                minimum = min_steps(adv["start"], targets, walkable)
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

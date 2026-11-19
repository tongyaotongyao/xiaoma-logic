"""Generate fixed, uniquely solvable 6x6 Xiaoma Logic boards.

The solution counter is exhaustive: every column permutation with the
eight-neighbour restriction is checked against the six colour regions.
"""

from __future__ import annotations

import itertools
import json
import random
from rate_levels import analyse
from pathlib import Path

SIZE = 6
COUNT = 25
SEED = 20261116
ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "Content" / "Levels" / "levels.json"


def all_placements() -> list[tuple[int, ...]]:
    return [
        p
        for p in itertools.permutations(range(SIZE))
        if all(abs(p[row] - p[row - 1]) > 1 for row in range(1, SIZE))
    ]


PLACEMENTS = all_placements()


def valid_solutions(regions: list[list[int]], givens: list[list[int]]) -> list[tuple[int, ...]]:
    required = {row: col for row, col in givens}
    return [
        cols
        for cols in PLACEMENTS
        if all(cols[row] == col for row, col in required.items())
        and len({regions[row][col] for row, col in enumerate(cols)}) == SIZE
    ]


def make_regions(solution: tuple[int, ...], rng: random.Random) -> list[list[int]]:
    growth_bias = rng.uniform(0.04, 0.16)
    regions = [[-1] * SIZE for _ in range(SIZE)]
    cells = [[(row, solution[row])] for row in range(SIZE)]
    for colour, group in enumerate(cells):
        row, col = group[0]
        regions[row][col] = colour

    while any(-1 in row for row in regions):
        frontier: list[tuple[float, int, int, int]] = []
        for colour, group in enumerate(cells):
            for row, col in group:
                for dr, dc in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nr, nc = row + dr, col + dc
                    if 0 <= nr < SIZE and 0 <= nc < SIZE and regions[nr][nc] == -1:
                        frontier.append((rng.random() + len(group) * growth_bias, colour, nr, nc))
        if not frontier:
            raise RuntimeError("Region growth unexpectedly stalled")
        _, colour, row, col = min(frontier)
        regions[row][col] = colour
        cells[colour].append((row, col))
    return regions


def connected(regions, colour):
    cells = {(r, c) for r in range(SIZE) for c in range(SIZE) if regions[r][c] == colour}
    if not cells:
        return False
    reached = {next(iter(cells))}
    todo = list(reached)
    while todo:
        r, c = todo.pop()
        for other in ((r-1,c),(r+1,c),(r,c-1),(r,c+1)):
            if other in cells and other not in reached:
                reached.add(other)
                todo.append(other)
    return reached == cells


def difficulty(regions):
    # Fixed row-order backtracking work. A reproducible proxy, not a human rating.
    nodes = 0
    def search(row, columns, colours, previous):
        nonlocal nodes
        nodes += 1
        if row == SIZE:
            return
        for col in range(SIZE):
            colour = regions[row][col]
            if col not in columns and colour not in colours and abs(col-previous) > 1:
                search(row+1, columns | {col}, colours | {colour}, col)
    search(0, set(), set(), -10)
    return nodes


def signature(regions):
    # Reject rotations/reflections and colour relabellings of the same puzzle.
    variants = []
    grid = regions
    for _ in range(4):
        for rows in (grid, [list(reversed(row)) for row in grid]):
            labels = {}
            variants.append(tuple(labels.setdefault(v, len(labels)) for row in rows for v in row))
        grid = [list(row) for row in zip(*grid[::-1])]
    return min(variants)


def main():
    import math
    rng = random.Random(SEED)
    retained = json.loads(OUTPUT.read_text())['levels'][10:]
    candidates = []
    seen = {signature(b['regions']) for b in retained}
    for restart in range(800):
        solution = rng.choice(PLACEMENTS)
        regions = make_regions(solution, rng)
        def score(grid):
            sizes = [sum(row.count(c) for row in grid) for c in range(SIZE)]
            penalty = sum(max(0, 3-n)*3 + max(0,n-9)*3 for n in sizes)
            answers = valid_solutions(grid, [])
            return (len(answers)-1)*2 + penalty + sum((n-6)**2 for n in sizes)*0.015, answers, sizes
        value, answers, sizes = score(regions)
        for step in range(700):
            if len(answers) == 1 and min(sizes) >= 3 and max(sizes) <= 9:
                key = signature(regions)
                if key not in seen:
                    seen.add(key)
                    reasoning = analyse(regions)
                    if reasoning['solved'] and reasoning['score'] <= 24:
                        candidates.append(dict(regions=[row[:] for row in regions], givens=[],
                            solution=list(answers[0]), solutionsBeforeGivens=1,
                            regionSizes=sizes[:], searchNodes=difficulty(regions),
                            logicScore=reasoning['score'], logicSteps=reasoning['steps']))
                        print(f"Accepted {len(candidates)}; logicScore={reasoning['score']}", flush=True)
                break
            r, c = rng.randrange(SIZE), rng.randrange(SIZE)
            old = regions[r][c]
            if solution[old] == c and old == r:
                continue
            neighbours = [(nr,nc) for nr,nc in ((r-1,c),(r+1,c),(r,c-1),(r,c+1))
                          if 0 <= nr < SIZE and 0 <= nc < SIZE and regions[nr][nc] != old]
            if not neighbours:
                continue
            nr,nc = rng.choice(neighbours)
            regions[r][c] = regions[nr][nc]
            if not connected(regions, old):
                regions[r][c] = old
                continue
            new_value, new_answers, new_sizes = score(regions)
            temperature = 0.7 * (1-step/700) + 0.06
            if new_value <= value or rng.random() < math.exp((value-new_value)/temperature):
                value, answers, sizes = new_value, new_answers, new_sizes
            else:
                regions[r][c] = old
        if len(candidates) >= 60:
            break
    if len(candidates) < 10:
        raise RuntimeError(f'Only {len(candidates)} qualifying boards; no data written')
    candidates.sort(key=lambda b: (b['logicScore'], len(b['logicSteps'])))
    levels = [dict(id=i+1, **candidates[i]) for i in range(10)] + retained
    for level in levels[:10]:
        level['logicOverlapSteps'] = sum(step['rule'] == 'overlap' for step in level.pop('logicSteps'))
    payload = dict(formatVersion=4, size=SIZE, colours=SIZE, levels=levels)
    OUTPUT.write_text(json.dumps(payload, indent=2)+'\n', encoding='utf-8')
    for b in levels:
        print(f"Level {b['id']:02}: sizes={b['regionSizes']}, searchNodes={b['searchNodes']}")


if __name__ == '__main__':
    main()

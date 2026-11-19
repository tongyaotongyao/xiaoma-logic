"""Generate fixed, uniquely solvable 6x6 Xiaoma Logic boards.

The solution counter is exhaustive: every column permutation with the
eight-neighbour restriction is checked against the six colour regions.
"""

from __future__ import annotations

import itertools
import json
import random
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
    growth_bias = rng.uniform(-0.08, 0.04)
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


def make_candidate(rng: random.Random) -> dict | None:
    solution = rng.choice(PLACEMENTS)
    regions = make_regions(solution, rng)
    solutions_without_givens = valid_solutions(regions, [])
    if solution not in solutions_without_givens:
        raise AssertionError("Generator lost its seed solution")

    # A board must be unique with no pre-filled ponies.
    givens: list[list[int]] = []
    if len(solutions_without_givens) != 1:
        return None
    return {
        "regions": regions,
        "givens": sorted(givens),
        "solution": list(solution),
        "solutionsBeforeGivens": len(solutions_without_givens),
    }


def main() -> None:
    rng = random.Random(SEED)
    seen: set[str] = set()
    candidates: list[dict] = []
    for _ in range(50000):
        board = make_candidate(rng)
        if board is None:
            continue
        key = json.dumps(board["regions"], separators=(",", ":"))
        if key in seen:
            continue
        seen.add(key)
        candidates.append(board)
        if len(candidates) >= 150:
            break
    if len(candidates) < COUNT:
        raise RuntimeError(f"Only found {len(candidates)} valid boards")

    # Put boards with smaller, more constraining regions first (difficulty heuristic).
    candidates.sort(key=lambda b: sum(len([v for row in b["regions"] for v in row if v == c]) ** 2 for c in range(SIZE)) * -1)
    sampled = [candidates[int(i * (len(candidates) - 1) / (COUNT - 1))] for i in range(COUNT)]
    levels = []
    for number, board in enumerate(sampled, 1):
        levels.append({"id": number, **board})
    payload = {"formatVersion": 2, "size": SIZE, "colours": SIZE, "levels": levels}
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {len(levels)} unique-solution boards to {OUTPUT}")
    for board in levels:
        print(f"{board['id']:02d}: clues={len(board['givens'])}, candidates_before_clues={board['solutionsBeforeGivens']}")


if __name__ == "__main__":
    main()

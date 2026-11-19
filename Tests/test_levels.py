"""Independent exhaustive check of the shipped level data."""

import itertools
import json
import unittest
from pathlib import Path


class LevelDataTests(unittest.TestCase):
    def test_all_25_levels_have_one_valid_solution(self):
        path = Path(__file__).resolve().parents[1] / "Content" / "Levels" / "levels.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        self.assertEqual(data["size"], 6)
        self.assertEqual(len(data["levels"]), 25)
        seen = set()
        for expected_id, level in enumerate(data["levels"], 1):
            self.assertEqual(level["id"], expected_id)
            regions = level["regions"]
            self.assertEqual(len(regions), 6)
            self.assertTrue(all(len(row) == 6 for row in regions))
            self.assertEqual(set(sum(regions, [])), set(range(6)))
            for colour in range(6):
                cells = {(r, c) for r in range(6) for c in range(6)
                         if regions[r][c] == colour}
                self.assertGreaterEqual(len(cells), 3, (expected_id, colour))
                self.assertLessEqual(len(cells), 9, (expected_id, colour))
                reached = {next(iter(cells))}
                frontier = list(reached)
                while frontier:
                    row, col = frontier.pop()
                    for other in ((row - 1, col), (row + 1, col),
                                  (row, col - 1), (row, col + 1)):
                        if other in cells and other not in reached:
                            reached.add(other)
                            frontier.append(other)
                self.assertEqual(reached, cells, (expected_id, colour))
            variants = []
            grid = regions
            for _ in range(4):
                for rows in (grid, [list(reversed(row)) for row in grid]):
                    labels = {}
                    variants.append(tuple(labels.setdefault(v, len(labels)) for row in rows for v in row))
                grid = [list(row) for row in zip(*grid[::-1])]
            signature = min(variants)
            self.assertNotIn(signature, seen)
            seen.add(signature)
            self.assertEqual(level["givens"], [], expected_id)
            givens = {row: col for row, col in level["givens"]}
            self.assertEqual(len(givens), len(level["givens"]))
            self.assertTrue(all(0 <= row < 6 and 0 <= col < 6
                                and level["solution"][row] == col
                                for row, col in level["givens"]))
            answers = []
            for cols in itertools.permutations(range(6)):
                if any(abs(cols[r] - cols[r - 1]) <= 1 for r in range(1, 6)):
                    continue
                if any(cols[r] != c for r, c in givens.items()):
                    continue
                if len({regions[r][c] for r, c in enumerate(cols)}) == 6:
                    answers.append(cols)
            self.assertEqual(answers, [tuple(level["solution"])], level["id"])

    def test_intro_levels_have_sound_deduction_traces(self):
        from Tools.rate_levels import analyse
        data = json.loads((Path(__file__).resolve().parents[1] / 'Content/Levels/levels.json').read_text())
        for level in data['levels'][:10]:
            regions = level['regions']
            result = analyse(regions)  # No answer passed to the analyser.
            self.assertTrue(result['solved'], level['id'])
            self.assertLessEqual(result['score'], 24)
            if level['id'] <= 2:
                self.assertLessEqual(result['score'], 10)
            self.assertEqual(result['placements'], [(r,c) for r,c in enumerate(level['solution'])])
            allowed = {(r,c) for r in range(6) for c in range(6)}
            def clash(a,b):
                return a != b and (a[0]==b[0] or a[1]==b[1]
                    or regions[a[0]][a[1]]==regions[b[0]][b[1]]
                    or max(abs(a[0]-b[0]),abs(a[1]-b[1]))<=1)
            for step in result['steps']:
                kind, number = step['group'].split()
                number = int(number)
                group = {(r,c) for r in range(6) for c in range(6)
                    if (kind=='row' and r==number-1) or (kind=='column' and c==number-1)
                    or (kind=='colour' and regions[r][c]==number)}
                options = allowed & group
                self.assertEqual(set(map(tuple,step['candidates'])), options)
                if step['rule']=='single':
                    self.assertEqual(len(options),1)
                for cell in map(tuple, step['removed']):
                    self.assertIn(cell,allowed)
                    self.assertTrue(all(clash(cell,option) for option in options))
                allowed -= set(map(tuple,step['removed']))
            self.assertEqual(allowed,set(result['placements']))


if __name__ == "__main__":
    unittest.main()

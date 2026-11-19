"""Rate boards by deductions, without looking at the stored solution."""
from __future__ import annotations
import json
from pathlib import Path


def analyse(regions):
    cells = {(r, c) for r in range(6) for c in range(6)}
    groups = []
    for r in range(6):
        groups.append((f'row {r+1}', {(r,c) for c in range(6)}))
    for c in range(6):
        groups.append((f'column {c+1}', {(r,c) for r in range(6)}))
    for colour in range(6):
        groups.append((f'colour {colour}', {(r,c) for r in range(6) for c in range(6) if regions[r][c] == colour}))
    def conflicts(a,b):
        return a != b and (a[0] == b[0] or a[1] == b[1]
            or regions[a[0]][a[1]] == regions[b[0]][b[1]]
            or max(abs(a[0]-b[0]),abs(a[1]-b[1])) <= 1)
    steps = []
    placed = set()
    while True:
        if any(not (cells & group) for _,group in groups):
            return dict(solved=False, steps=steps, placements=sorted(placed), score=9999)
        progress = False
        for label,group in groups:
            options = cells & group
            if len(options) == 1:
                pony = next(iter(options))
                if pony not in placed:
                    placed.add(pony)
                    removed = {cell for cell in cells if conflicts(cell,pony)}
                    steps.append(dict(rule='single', group=label, candidates=[pony], removed=sorted(removed), pony=pony))
                    cells -= removed
                    progress = True
                    break
        if len(placed) == 6:
            locks = [s for s in steps if s['rule'] == 'overlap']
            return dict(solved=True, steps=steps, placements=sorted(placed),
                score=sum(2+len(s['candidates']) for s in locks), overlapSteps=len(locks))
        if progress:
            continue
        for label,group in groups:
            options = cells & group
            removed = {cell for cell in cells-group if all(conflicts(cell,other) for other in options)}
            if removed:
                steps.append(dict(rule='overlap', group=label, candidates=sorted(options), removed=sorted(removed)))
                cells -= removed
                progress = True
                break
        if not progress:
            return dict(solved=False, steps=steps, placements=sorted(placed), score=9999)


if __name__ == '__main__':
    root = Path(__file__).resolve().parents[1]
    for level in json.loads((root/'Content/Levels/levels.json').read_text())['levels']:
        result = analyse(level['regions'])
        print(level['id'], result['solved'], result['score'], len(result['steps']))

# Circle Checker

This module classifies the relationship between two circles based on the distance between their centers.

## Input format

Each input string is parsed as:

`x1 y1 r1 x2 y2 r2`

Where:
- `(x1, y1)` is the center of circle 1
- `r1` is the radius of circle 1
- `(x2, y2)` is the center of circle 2
- `r2` is the radius of circle 2

Example:

`0 0 1 2 0 1`

means:
- circle 1: center `(0,0)`, radius `1`
- circle 2: center `(2,0)`, radius `1`

## Core logic

The code computes the center-to-center distance:

`d = sqrt((x2 - x1)^2 + (y2 - y1)^2)`

Then it compares `d` against the two important geometric thresholds:

- `r1 + r2` : the largest possible distance for two circles to still overlap or touch externally
- `|r1 - r2|` : the smallest possible distance when one circle is fully inside the other

## Relationship rules

1. TouchingExternal
   - `d == r1 + r2`
   - The circles just touch externally at one point.

2. Disjoint outside
   - `d > r1 + r2`
   - The circles are completely separate.

3. Concentric
   - `d == 0 && r1 == r2`
   - Both circles share the same center and are identical.

4. TouchingInternal
   - `d == |r1 - r2|`
   - One circle is internally tangent to the other: they touch at a single point, but one circle lies inside the other.

5. Intersecting
   - `d < r1 + r2 && d > |r1 - r2|`
   - The circles overlap, but neither circle is fully contained inside the other.

6. Disjoint inside
   - `d < |r1 - r2|`
   - One circle is completely contained inside the other.

7. Undefined
   - Any edge case not covered by the above rules.

## Example cases

- `0 0 1 3 0 1` -> DisjointOutside
- `0 0 1 2 0 1` -> TouchingExternal
- `0 0 1 0 0 1` -> Concentric
- `0 0 2 1 0 2` -> Intersecting
- `0 0 2 1 0 1` -> TouchingInternal
- `0 0 2 0 0 1` -> DisjointInside

## Where the logic lives

The classification is implemented in:

- `internal/circle-checker/checker.hpp`

The unit tests are in:

- `internal/circle-checker/checker_test.cpp`

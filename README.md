# Milestone 1 — Sequential Baseline: Robust Point-in-Polygon

## Purpose

This branch contains the sequential baseline and spatial indexing foundation for the Parallel Point-in-Polygon project. The goal for Milestone 1 is correctness and an efficient pruning structure that reduces the number of expensive point-in-polygon checks.

## Key Files
- `milestone1_robust.cpp` — Primary sequential implementation. Implements a robust ray-casting point-in-polygon (PIP) algorithm and builds a quadtree / bounding-box index for polygons.
- `parallel-point-in-polygon.md` — Project problem statement, design rationale, and milestone descriptions.
- `PDC_Project_Milestone_0.pdf` — Early project notes and baseline report.

## What Milestone 1 Achieves
- Correct and robust ray-casting PIP that handles boundary cases (points on edges/vertices).
- Spatial filtering using polygon minimum bounding boxes (MBBs) and a quadtree to prune candidates.
- Synthetic dataset generation for both uniform and highly clustered (urban) point distributions.
- A reproducible sequential baseline for later parallel speedups.

## How to Build & Run (Sequential)

Requirements:
- g++ (C++17 or later)

Build and run locally:

```bash
# from repository root
g++ -O3 -std=c++17 milestone1_robust.cpp -o milestone1_robust
./milestone1_robust --points=1000000
```

The program prints classification counts and timing. Use smaller point counts to test quickly.

## Notes for Reviewers / Instructor
- Focus review on `milestone1_robust.cpp` to see the ray-casting implementation and the quadtree insertion/query logic.
- The `parallel-point-in-polygon.md` file contains the project motivation and algorithmic choices that guided later milestones.
- Milestone 1 provides the correctness foundation used (unchanged) by Milestone 2 and 3; correctness is validated by matching outputs across implementations.

## Next Steps (Milestone Handoff)
- Use this branch to understand the geometric primitives and indexing.
- For parallelization details, switch to `Milestone-2`.

*Last updated: May 2026*
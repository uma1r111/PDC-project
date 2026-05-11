# Milestone 2 — Shared-Memory Parallelization (OpenMP)

## Purpose

This branch implements shared-memory parallelization of the point-in-polygon classification pipeline. Milestone 2 focuses on efficient multithreading, scheduling strategies to handle spatial skew, and validating that parallelization retains correctness from Milestone 1.

## Key Files
- milestone2_corrected.cpp — OpenMP-parallel implementation with three scheduling strategies: static, dynamic, and guided.
- milestone2_corrected.ipynb — Notebook comparing scheduling strategies and presenting performance plots.
- spatial_skew.csv — Dataset used to demonstrate performance under clustered point distributions.
- parallel-point-in-polygon.md — Project design notes and algorithmic rationale.

## What Milestone 2 Achieves
- Correct parallel point classification using OpenMP pragmas
- Three scheduling modes to compare load distribution and overheads:
  - static – fixed chunk assignment (low overhead, poor for skew)
  - dynamic – work-stealing with adjustable chunk size (robust under skew)
  - guided – hybrid approach with decreasing chunk sizes
- Domain decomposition and grouping to improve cache locality
- Empirical evaluation demonstrating improved throughput on multi-core systems

## How to Build & Run (OpenMP)

Requirements:
- g++ supporting OpenMP (e.g., g++ with -fopenmp)

Build and run with 4 threads:

`ash
# from repository root
g++ -O3 -std=c++17 -fopenmp milestone2_corrected.cpp -o milestone2_corrected
export OMP_NUM_THREADS=4
./milestone2_corrected --schedule=dynamic --chunk=256 --points=10000000
`

Or on Windows (PowerShell):

`powershell
=4
.\milestone2_corrected.exe --schedule=dynamic --chunk=256 --points=10000000
`

## Scheduling Parameters
- --schedule accepts static, dynamic, or guided.
- --chunk controls chunk size for dynamic and static scheduling; recommended empirical default is 256 for this workload.

## Notes for Reviewers / Instructor
- Inspect milestone2_corrected.cpp to see how the OpenMP loops are structured and how the scheduling argument is passed through to pragmas.
- The notebook milestone2_corrected.ipynb contains the experiments and plots comparing the three strategies under uniform and skewed point distributions.
- Milestone 2 builds directly on the correctness of Milestone 1; the PIP and indexing code is reused without modification for correctness.

## Next Steps
- Run Milestone-3 for distributed execution (MPI + OpenMP hybrid).
- Consider fine-tuning chunk size and thread binding for NUMA/multi-socket machines.

*Last updated: May 2026*

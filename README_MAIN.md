# Parallel Point-in-Polygon Classification for Large-Scale Geospatial Data

## Project Overview

This repository contains a **three-milestone progressive implementation** of a parallel and distributed point-in-polygon classification system. The project tackles the challenge of efficiently classifying **millions of spatial points** against complex polygon datasets using modern parallel computing techniques.

**Problem Context:** Geospatial systems routinely need to determine if GPS points (events, transactions, sensor readings) fall within polygonal regions (administrative boundaries, geofences, service zones). At scale (10M–100M points), this becomes computationally intensive, requiring:
- Efficient geometric algorithms (ray-casting)
- Spatial indexing (quadtree/R-tree)
- Load balancing under skewed distributions
- Distributed computing (MPI)

---

## Repository Structure

The project is organized into **three separate branches**, each representing a completed milestone:

### 📍 **Milestone 1: Sequential Baseline with Spatial Indexing**
**Branch:** `Milestone-1`

Establishes the geometric foundation:
- Robust ray-casting point-in-polygon algorithm
- Quadtree spatial indexing over polygon bounding boxes
- Synthetic dataset generation (uniform + clustered distributions)
- Performance baseline for sequential execution

**Key files:**
- `milestone1_robust.cpp` – Core geometric algorithms
- `PDC_Project_Milestone_1--Correct.ipynb` – Analysis & visualization

---

### 🔄 **Milestone 2: Parallel Point Classification and Load Balancing**
**Branch:** `Milestone-2`

Introduces shared-memory parallelism:
- OpenMP multithreading with 3 scheduling strategies:
  - **Static:** Fixed work distribution (baseline)
  - **Dynamic:** Work-stealing with adaptive chunks
  - **Guided:** Hybrid approach with decreasing chunk sizes
- Spatial domain decomposition via grid tiling
- Load balancing for urban clusters (spatial skew)
- Cache-friendly thread organization

**Key files:**
- `milestone2_corrected.cpp` – OpenMP parallel implementation
- `milestone2_corrected.ipynb` – Scheduling strategy comparison
- `spatial_skew.csv` – Performance metrics under skewed data

---

### 🚀 **Milestone 3: Distributed Execution and MPI Batch Processing**
**Branch:** `Milestone-3` ← **← Current branch for cluster testing**

Scales beyond single machines with MPI + OpenMP hybrid:
- **Two communication strategies:**
  - **Replicate Mode:** All ranks receive all polygons (simple, broadcast overhead)
  - **Shard Mode:** Polygons spatially partitioned across ranks (complex, reduces broadcast)
- MPI result aggregation via `MPI_Reduce()`
- Docker containerization for cluster simulation
- Multi-container networking via SSH (distributed cluster)
- Tested at 10M and 100M point scales

**Key files:**
- `milestone3_mpi.cpp` – MPI + OpenMP hybrid implementation
- `Dockerfile` – Ubuntu 22.04 with OpenMPI, SSH, and build tools
- `docker-compose.yml` – Multi-container cluster definition
- `CLUSTER_TEST_RESULTS.md` – Performance results

**Branch-specific README:** See [Milestone-3 Documentation](Milestone-3-README.md) for detailed implementation guide.

---

## Quick Start

### Prerequisites
- **For local development:** GCC/G++, OpenMPI, OpenMP
- **For Docker:** Docker & Docker Compose

### Build & Run Milestone 3 (MPI + OpenMP)

#### Option 1: Native (requires OpenMPI installation)
```bash
cd Milestone-3  # Switch to Milestone-3 branch
g++ -O3 -fopenmp milestone3_mpi.cpp -o milestone3_mpi
./milestone3_mpi --mode=replicate --points=10000000 --omp-threads=4
```

#### Option 2: Docker (Single Container - Local IPC)
```bash
docker compose build
docker compose run --rm mpi mpirun --allow-run-as-root -np 4 \
  /app/milestone3_mpi --mode=replicate --points=10000000 --omp-threads=4
```

#### Option 3: Docker (Multi-Container - Network Simulation)
```bash
docker compose up -d rank0 rank1 rank2 rank3
docker compose logs -f rank0
# Results appear in log output
```

---

## Milestone Progression

| Milestone | Focus | Parallelism | Scale Tested |
|-----------|-------|-------------|--------------|
| **M1** | Correctness & indexing | Sequential baseline | 1M–10M points |
| **M2** | Shared-memory parallelism | OpenMP (4–16 threads) | 10M points |
| **M3** | Distributed execution | MPI (4 ranks) + OpenMP (4 threads) | 10M–100M points |

---

## Key Achievements

✅ **Geometric Correctness** – Handles edge cases (points on boundaries, multi-polygons, holes)

✅ **Spatial Indexing** – Quadtree over 2000 synthetic polygons reduces unnecessary checks

✅ **Load Balancing** – Dynamic scheduling handles urban clusters (10× density variation)

✅ **Scalability** – Processes 100M points in ~8.6 seconds (multi-container cluster)

✅ **Cluster Simulation** – Docker multi-container setup mimics distributed systems with SSH-based MPI communication

---

## Instructor Notes

For a complete understanding of the project:

1. **Start here:** Read `parallel-point-in-polygon.md` for problem statement and design rationale
2. **Milestone 1:** Switch to `Milestone-1` branch and review geometric algorithms
3. **Milestone 2:** Switch to `Milestone-2` branch, compare OpenMP scheduling strategies
4. **Milestone 3:** Stay on `Milestone-3` branch (current) for distributed execution demo
   - Review `milestone3_mpi.cpp` for MPI architecture
   - Check `CLUSTER_TEST_RESULTS.md` for performance validation
   - Run Docker multi-container test to see cluster in action

---

## Project Statistics

- **Total implementation:** ~2000 lines of C++
- **Algorithms:** Ray-casting PIP, Quadtree indexing, Grid partitioning
- **Parallelization:** OpenMP (M2), MPI + OpenMP hybrid (M3)
- **Test dataset:** 2000 polygons, 10M–100M points
- **Performance:** 68M points classified in 8.6 seconds (multi-container, 16 total threads)

---

## File Navigation

| File/Directory | Purpose |
|---|---|
| `README.md` (this file) | Project overview & navigation |
| `parallel-point-in-polygon.md` | Detailed problem statement & design |
| `Milestone-1/` | Sequential baseline (M1 branch) |
| `Milestone-2/` | OpenMP parallelization (M2 branch) |
| `Milestone-3/` | MPI distributed execution (M3 branch) |
| `.docker*` | Docker containerization (M3) |
| `.git/` | Git repository with full commit history |

---

## Contact & Attribution

**Student:** Saad Thaplawala (ID: 27172)

**Assigned Milestones:**
- Milestone 2: Multithreaded point classification via OpenMP
- Milestone 3: MPI result aggregation and distributed execution

**Course:** Parallel & Distributed Computing (PDC)

---

*Last Updated: May 2026*
*For detailed M3 documentation, see the Milestone-3 branch README*

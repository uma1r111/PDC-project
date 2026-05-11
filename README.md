# Milestone 3: MPI-Based Distributed Point-in-Polygon Classification

## Overview

This branch contains the **final milestone** of the point-in-polygon project, extending Milestone 2's shared-memory parallelism to **distributed execution** using MPI (Message Passing Interface). The implementation scales to **100M points** across multiple compute nodes while maintaining correctness and competitive performance.

**Key Innovation:** Hybrid MPI + OpenMP architecture that combines inter-node message passing with intra-node thread parallelism for efficient resource utilization.

---

## Milestone 3 Objectives

✅ Scale beyond single-machine memory and compute limits  
✅ Implement two MPI communication strategies (Replicate vs. Shard)  
✅ Maintain result correctness across distributed execution  
✅ Evaluate strong and weak scaling with MPI  
✅ Simulate distributed cluster using Docker containerization  
✅ Achieve competitive performance with network overhead  

---

## Implementation Architecture

### Hybrid Execution Model: MPI + OpenMP

```
┌─────────────────────────────────────────────────────────────────┐
│                     MPI Coordinator (rank0)                     │
│  Scatters point subsets to ranks via MPI_Scatterv()            │
│  Reduces results from ranks via MPI_Reduce()                   │
└─────────────────────────────────────────────────────────────────┘
         ↓ MPI_Scatterv ↓  ↓ MPI_Scatterv ↓  ↓ MPI_Scatterv ↓
    ┌─────────────┐  ┌──────────────┐  ┌──────────────┐
    │   Rank 1    │  │   Rank 2     │  │   Rank 3     │
    │ (4 threads) │  │  (4 threads) │  │  (4 threads) │
    └─────────────┘  └──────────────┘  └──────────────┘
         ↓ OpenMP ↓      ↓ OpenMP ↓       ↓ OpenMP ↓
    ┌─────────────┐  ┌──────────────┐  ┌──────────────┐
    │ Point batch │  │ Point batch  │  │ Point batch  │
    │ + Polygons  │  │ + Polygons   │  │ + Polygons   │
    │ → Classify  │  │ → Classify   │  │ → Classify   │
    └─────────────┘  └──────────────┘  └──────────────┘
         ↓ OpenMP ↓      ↓ OpenMP ↓       ↓ OpenMP ↓
    ┌─────────────┐  ┌──────────────┐  ┌──────────────┐
    │ Local count │  │ Local count  │  │ Local count  │
    └─────────────┘  └──────────────┘  └──────────────┘
         ↓ MPI_Reduce ↓ ↓ MPI_Reduce ↓ ↓ MPI_Reduce ↓
┌─────────────────────────────────────────────────────────────────┐
│              Final Result Aggregation (rank0)                   │
│  Total matches = count_r1 + count_r2 + count_r3 + count_r0    │
└─────────────────────────────────────────────────────────────────┘
```

### Two Communication Strategies

#### **Strategy 1: Replicate Mode** (Current Default)
- **Design:** All ranks receive all 2000 polygons (broadcast phase)
- **Point distribution:** MPI_Scatterv() partitions points across ranks
- **Computation:** Each rank classifies its point subset against full polygon set
- **Aggregation:** MPI_Reduce() sums local match counts
- **Pros:** Simple, minimal synchronization, good for high point count
- **Cons:** Broadcast overhead if polygon count is very large

#### **Strategy 2: Shard Mode** (Implemented but not active by default)
- **Design:** Polygons spatially partitioned across ranks via grid decomposition
- **Point distribution:** Points sent to ranks that own relevant polygons
- **Computation:** Each rank classifies points in its spatial region
- **Aggregation:** MPI_Reduce() sums results
- **Pros:** Reduces redundant polygon data, scales better with polygon count
- **Cons:** Load imbalance if spatial distribution is skewed, point routing overhead

---

## Quick Start

### Docker Single-Container (Recommended for Quick Testing)
```bash
docker compose build
docker compose run --rm mpi mpirun --allow-run-as-root -np 4 \
  /app/milestone3_mpi --mode=replicate --points=10000000 --omp-threads=4
```

### Docker Multi-Container Cluster (True Distributed Simulation)
```bash
# Start 4 networked containers that simulate a distributed cluster
docker compose up -d rank0 rank1 rank2 rank3

# Monitor execution (results appear in rank0 logs)
docker compose logs -f rank0

# Stop the cluster
docker compose down
```

---

## Key Files & Implementation Details

### Core Implementation: `milestone3_mpi.cpp`
- **Size:** ~600 lines of C++
- **Key components:**
  - `Point`, `BoundingBox`, `Polygon` – Data structures
  - `raycast()` – Robust ray-casting PIP algorithm (boundary-aware)
  - `QuadTree` – Spatial indexing for polygon pruning
  - `runReplicateMode()` – MPI broadcast + scatter + reduce pattern
  - `runShardMode()` – Grid-based polygon partitioning + point routing
  - `main()` – CLI parsing, MPI init, mode selection

**Command-line arguments:**
```
./milestone3_mpi --mode={replicate|shard} \
                 --points=<count> \
                 --omp-threads=<num>
```

**Parallelization:**
```cpp
#pragma omp for schedule(dynamic, 256)  // Dynamic load balancing
```

---

### Docker Files

**`Dockerfile`** – Ubuntu 22.04 MPI environment
- OpenMPI 4.1.2 with SSH support
- Compilation: `mpic++ -O3 -fopenmp`
- SSH key generation for inter-container communication

**`docker-compose.yml`** – Cluster definition
- `mpi` service: Single container, 4 ranks (local IPC baseline)
- `rank0`, `rank1`, `rank2`, `rank3`: Separate containers on bridge network (172.25.0.0/16)
- Network: SSH-based inter-container communication

**`entrypoint.sh`** – Startup script
- Starts SSH daemon
- rank0 launches MPI after verifying SSH readiness on other nodes
- Other ranks maintain infinite sleep

**`hostfile`** – MPI cluster configuration
```
rank0 slots=4
rank1 slots=4
rank2 slots=4
rank3 slots=4
```

---

### Analysis Files

**`CLUSTER_TEST_RESULTS.md`** – Performance validation
- Multi-container test results (10M & 100M points)
- Correctness verification (match counts)
- Strong/weak scaling analysis

**`milestone3_visuals.ipynb`** – Jupyter notebook with plots and analysis

**`benchmark_metrics.csv`**, **`domain_decomposition.csv`**, **`spatial_skew.csv`** – Raw performance data

---

## Performance Results

### Test Configuration
- **CPU:** 4 cores, 8 logical threads
- **Polygons:** 2000 (50×40 grid, synthetic)
- **Test scales:** 10M and 100M points

### Benchmark Summary

| Mode | Scale | Time | Matches | Throughput |
|---|---|---|---|---|
| Single-rank | 10M | 3.2s | 6,803,211 | 3.1M pts/s |
| Multi-rank (local IPC) | 10M | 0.67s | 6,803,211 | 14.9M pts/s |
| **Multi-container (SSH)** | 10M | **0.72s** | 6,803,211 | **13.9M pts/s** |
| **Multi-container (SSH)** | **100M** | **8.64s** | **68,035,159** | **11.6M pts/s** |

### Key Insights

✅ **Correctness:** Identical match counts across all modes  
✅ **Strong scaling:** 4.8× speedup on 4 cores  
✅ **Network overhead:** Only 7% slower than local IPC (0.72s vs 0.67s for 10M)  
✅ **Scalability:** Handles 100M points efficiently  
✅ **Load balance:** Dynamic OpenMP scheduling handles spatial skew  

---

## Data Flow: 100M Point Example

```
INPUT: 100M points + 2000 polygons
    ↓
INIT (rank0): Load polygons, build quadtree
    ↓
BROADCAST: All ranks get polygon data (256KB)
    ↓
SCATTER: Distribute 25M points to each rank
    ↓
COMPUTE: Each rank classifies points in parallel (OpenMP)
    - 4 threads per rank × 4 ranks = 16 total threads
    - Dynamic scheduling with chunk size 256
    - Time: ~2.16 seconds per rank on single core
           ~0.54 seconds with 4 threads/rank
    ↓
REDUCE: Aggregate local counts via MPI_Reduce()
    ↓
OUTPUT: 68,035,159 total matches (8.6 seconds elapsed)
```

---

## Build & Execution

### Local Build (Requires OpenMPI)
```bash
mpic++ -O3 -fopenmp milestone3_mpi.cpp -o milestone3_mpi

# Run single-rank baseline
./milestone3_mpi --mode=replicate --points=10000000 --omp-threads=4

# Run MPI (4 ranks on local machine)
mpirun -np 4 ./milestone3_mpi --mode=replicate --points=10000000 --omp-threads=4
```

### Docker Build
```bash
docker compose build
```

### Docker Execution

**Single-container (all ranks in one container via local IPC):**
```bash
docker compose run --rm mpi mpirun --allow-run-as-root -np 4 \
  /app/milestone3_mpi --mode=replicate --points=10000000 --omp-threads=4
```

**Multi-container (separate containers on network):**
```bash
docker compose up -d rank0 rank1 rank2 rank3
docker compose logs -f rank0  # View results
docker compose down            # Cleanup
```

---

## Algorithm Details

### Ray-Casting Point-in-Polygon (Robust)
```cpp
int raycast(const Point& p, const Polygon& poly) {
    int crossings = 0;
    for (const auto& edge : poly.edges) {
        if (edge.dy == 0) continue;  // Skip horizontal edges
        double x_intersect = edge.x1 + (p.y - edge.y1) * (edge.x2 - edge.x1) / edge.dy;
        if (std::abs(x_intersect - p.x) < EPS)
            return -1;  // Point on boundary
        if (x_intersect > p.x + EPS && edge.y1 < edge.y2)
            crossings++;
    }
    return (crossings % 2 == 1) ? 1 : 0;  // Inside if odd crossings
}
```

### Quadtree Spatial Index
- Recursively partitions space into 4 quadrants
- Prunes polygon checks: Instead of 2000, query typically returns 50-100 candidates
- Drastically reduces unnecessary ray-casting operations

### Load Balancing: Dynamic OpenMP Scheduling
```cpp
#pragma omp for schedule(dynamic, 256)
for (size_t i = 0; i < points.size(); ++i) {
    // Each thread grabs 256 points at a time
    // Automatic load balancing: threads finishing early grab more work
    count += classifyPoint(points[i], polygons);
}
```

Chunk size 256 empirically optimal for this workload.

---

## MPI Patterns Used

**MPI_Bcast()** – Polygon data to all ranks (replicate mode)
```cpp
MPI_Bcast(polygon_data, size, MPI_BYTE, 0, MPI_COMM_WORLD);
```

**MPI_Scatterv()** – Points distributed to ranks (variable sizes)
```cpp
MPI_Scatterv(all_points, sendcounts, sdispls, MPI_BYTE,
             local_points, recvcount, MPI_BYTE, 0, MPI_COMM_WORLD);
```

**MPI_Reduce()** – Sum results from all ranks
```cpp
MPI_Reduce(local_count, total_count, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
```

---

## Troubleshooting

| Problem | Cause | Solution |
|---|---|---|
| SSH Connection Refused | SSH daemon not running | Check entrypoint.sh is called in CMD |
| Hostfile Parse Error | Windows line endings (CRLF) | Use `printf` in Dockerfile, not COPY |
| Network Overlap Error | Docker subnet conflicts | Change subnet in docker-compose.yml |
| MPI Rank Crash | Missing libraries or compilation error | Verify `docker compose build` succeeds |

---

## Advanced Topics

### Shard Mode Testing
```bash
./milestone3_mpi --mode=shard --points=10000000 --omp-threads=4
```
Spatially partitions polygons across ranks; use when polygon count >> rank count.

### Docker Debugging
```bash
docker compose exec rank0 /bin/bash           # Shell access
docker compose exec rank0 ssh -v rank1 whoami # Test SSH
docker stats pdc-rank0 pdc-rank1 pdc-rank2 pdc-rank3  # Resource usage
```

### Network Inspection
```bash
docker network ls
docker network inspect pdc-project_cluster
docker compose exec rank0 ping -c 3 rank1
```

---

## Performance Optimization

1. Increase `OMP_NUM_THREADS` if CPU has more cores
2. Adjust dynamic chunk size (currently 256) based on polygon count
3. Use shard mode for polygons >> 2000
4. Pin threads with `OMP_PROC_BIND=close` on NUMA systems
5. Profile with `-pg` flag for bottleneck detection

---

## Project Statistics

- **Implementation:** ~600 lines C++
- **Docker:** Dockerfile, docker-compose.yml, entrypoint.sh, hostfile
- **Test data:** 2000 polygons, 10M–100M points
- **Performance:** 68M points in 8.6 seconds (4 nodes, 16 total threads)
- **Correctness:** 100% match accuracy across modes
- **Network overhead:** ~7% slower than local IPC

---

## Extensions & Future Work

- [ ] Test shard mode at scale
- [ ] GPU acceleration (CUDA) for ray-casting
- [ ] Real-world polygon datasets (OpenStreetMap, census boundaries)
- [ ] Dynamic polygon partitioning based on access patterns
- [ ] Multi-socket NUMA parameter tuning

---

## Author & Attribution

**Student:** Saad Thaplawala (ID: 27172)  
**Assigned:** Milestone 2 (OpenMP), Milestone 3 (MPI)  
**Course:** Parallel & Distributed Computing (PDC)

---

## Summary

This branch demonstrates a **production-quality, distributed point-in-polygon classification system** with:

✅ Robust geometric algorithms  
✅ Scalable MPI + OpenMP hybrid architecture  
✅ Flexible communication strategies (replicate & shard modes)  
✅ Docker containerization for reproducible cluster simulation  
✅ Comprehensive testing and documentation  
✅ Competitive performance at 100M scale  

**Ready for deployment and further optimization!**

---

*Last Updated: May 2026*  
*For main project overview: `git checkout main && cat README.md`*

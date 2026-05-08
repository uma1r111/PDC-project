# Project 24 (Improved): Parallel Point-in-Polygon Classification for Large-Scale Geospatial Data

## Problem Statement (PDC Emphasis)

Modern geospatial systems must classify **millions of spatial events (GPS points)** against **complex polygonal regions** such as administrative boundaries, geofences, or service zones. While point-in-polygon tests are computationally simple in isolation, **massive input size, spatial skew, and complex polygon geometry** make efficient processing challenging.

This project explores how **parallel and distributed computing techniques**—including spatial partitioning, load balancing, concurrent indexing, and batch processing—can be applied to accelerate point-in-polygon classification at scale.

---

## Project Overview

The goal of this project is to design and implement a **parallel point-in-polygon system** capable of classifying **10–100 million GPS points** against complex polygon datasets.

Students will:

* Implement efficient geometric algorithms
* Build spatial index structures
* Parallelize spatial queries
* Address load imbalance caused by non-uniform spatial distributions
* Evaluate scalability using realistic datasets

**Core emphasis:**
- Parallel spatial algorithms
- Data partitioning under skew
- Scalability analysis

---

## Milestone 1: Sequential Baseline with Spatial Indexing

### Objectives

* Ensure geometric correctness
* Establish a strong performance baseline
* Prepare data structures for parallelization

### Key Tasks

* Implement a robust **ray-casting point-in-polygon** algorithm:

  * Points on edges or vertices
  * Polygons with holes and multi-polygons
* Build a **spatial index** (R-tree or quadtree) over polygon bounding boxes
* Implement **bounding-box filtering** to prune irrelevant polygon checks
* Support realistic polygon datasets (e.g., city boundaries, postal zones)
* Generate synthetic GPS datasets with:

  * Uniform distributions
  * Urban clusters (high spatial skew)

**PDC focus:** algorithmic correctness and data structure design as a foundation for parallelism.

---

## Milestone 2: Parallel Point Classification and Load Balancing

### Objectives

* Parallelize geometric computations
* Handle skewed spatial distributions
* Improve cache efficiency

### Key Tasks

* Implement **parallel point processing** using multithreading (OpenMP, TBB, or std::thread)
* Design **spatial partitioning** strategies:

  * Grid-based tiling
  * Hierarchical spatial decomposition
* Address **load imbalance** caused by dense regions:

  * Dynamic task queues
  * Work-stealing between threads
* Parallelize spatial index construction or querying
* Optimize cache locality by grouping spatially nearby points

**Optional extensions (bonus):**

* SIMD vectorization for ray-casting over multiple points
* Hybrid static + dynamic scheduling strategies

**PDC focus:** load balancing, task scheduling, shared-memory parallelism.

---

## Milestone 3: Scalable Batch Processing and Distributed Execution

### Objectives

* Scale beyond single-machine limits
* Evaluate strong and weak scaling behavior
* Minimize coordination overhead

### Key Tasks

* Implement **batch-based processing** of large point sets
* Design a **distributed execution model** (MPI or multi-process):

  * Points partitioned spatially across workers
  * Polygons replicated or spatially sharded
* Implement efficient **result aggregation**
* Evaluate trade-offs between:

  * Polygon replication vs partitioning
  * Communication cost vs computation
* Benchmark throughput across:

  * 1M, 10M, and 100M points
* Analyze performance under:

  * Uniform distributions
  * Highly clustered spatial data

**PDC focus:** distributed data partitioning, scalability analysis, communication minimization.

---

## Expected Outcomes

By completing this project, students will demonstrate:

* Mastery of parallel geometric algorithms
* Ability to design spatial data partitioning strategies
* Understanding of load balancing under skewed workloads
* Experience benchmarking and analyzing scalability
* Practical skills in high-performance geospatial computing

---
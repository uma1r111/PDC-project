#!/bin/bash

# Start SSH daemon
service ssh start

# If this is rank0, wait for other nodes and run MPI
if [ "$RANK" = "0" ]; then
    echo "Waiting for SSH on other nodes..."
    sleep 3
    mpirun --allow-run-as-root -np 4 -hostfile /app/hostfile /app/milestone3_mpi --mode=replicate --points=10000000 --omp-threads=4
    mpirun --allow-run-as-root -np 4 -hostfile /app/hostfile /app/milestone3_mpi --mode=replicate --points=100000000 --omp-threads=4
else
    # Keep container alive
    sleep infinity
fi

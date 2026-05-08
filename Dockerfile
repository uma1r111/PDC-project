FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    g++ \
    make \
    openmpi-bin \
    libopenmpi-dev \
    openssh-server \
    openssh-client \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY milestone3_mpi.cpp ./
COPY entrypoint.sh ./

RUN printf "rank0 slots=4\nrank1 slots=4\nrank2 slots=4\nrank3 slots=4\n" > ./hostfile && \
    chmod +x ./entrypoint.sh && \
    mpic++ -O3 -fopenmp milestone3_mpi.cpp -o milestone3_mpi

# Set up SSH for MPI cluster communication
RUN mkdir -p /var/run/sshd && \
    mkdir -p /root/.ssh && \
    ssh-keygen -t rsa -N "" -f /root/.ssh/id_rsa && \
    cat /root/.ssh/id_rsa.pub >> /root/.ssh/authorized_keys && \
    chmod 600 /root/.ssh/authorized_keys && \
    chmod 644 /root/.ssh/id_rsa.pub && \
    echo "StrictHostKeyChecking no" >> /root/.ssh/config && \
    echo "UserKnownHostsFile=/dev/null" >> /root/.ssh/config

ENV OMP_NUM_THREADS=4

# Start SSH daemon and keep container alive
CMD ["/app/entrypoint.sh"]
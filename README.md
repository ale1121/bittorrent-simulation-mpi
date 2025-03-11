# BitTorrent Protocol Simulation with MPI

This project simulates a simplified BitTorrent protocol using MPI. It demonstrates key aspects of the protocol, including file sharing, peer-to-peer communication, and centralized tracking, while using hash-based chunk verification.

## Features
- Simulates a tracker and multiple peers in a distributed environment
- Implements file swarm management and chunk verification using hash comparisons
- Demonstrates communication and synchronization with MPI message passing

## Usage

Build:

    make

Clean up:

    make clean

Run:

    mpirun [--oversubscribe] -np num_processes ./mpi-bittorrent

- `num_processes`: total number of processes (1 tracker + N peers)
- `--oversubscibe`: allows more processes than the available number of cores

### Input Files
Each peer requires an input file named `in<rank>.txt` (e.g., `in1.txt` for peer 1). Input files specify the files owned and the files desired by each peer. See the `test` folder for examples.

### Output
After downloading each file, peers generate files named `client<rank>_<file_name>`, containing the hashes of downloaded file chunks.

## Components

### Tracker
The tracker manages file swarms and responds to client requests. It:
- Maintains a list of files and their associated peers
- Tracks peer statuses and signals all clients to shut down after all files are downloaded

### Client
Each client has two threads:
1. **Download Thread**: Sends file requests, manages chunk downloads, and requests updates from the tracker
2. **Upload Thread**: Responds to chunk requests from other peers

<br>

See [IMPLEMENTATION.md](IMPLEMENTATION.md) for detailed information about the program's architecture and implementation.
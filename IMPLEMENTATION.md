# BitTorrent Protocol Simulation with MPI - Implementation

Note: the program doesn't make a distinction between seeds and peers. Any mention of _peer_ refers to any client that owns chunks of a file.

## Message Types

Below are the tags used to identify all types of messages used:

`SIZE` - Transmits the size of the next message.
<br>
`SWARM_REQUEST` - Sent by a client to the tracker, requesting the list of peers and ordered hashes for a file. Contains the file name. The tracker responds with the requested data, using the same tag.
<br>
`SWARM_UPDATE` - Similar to _SWARM_REQUEST_, except only requestinig the list of peers for the file.
<br>
`CHUNK_REQUEST` - Sent by a client to a peer, requesting a chunk of a file. Contains the chunk hash.
<br>
`CHUNK_REPLY` - Sent as a response to a _CHUNK_REQUEST_. Contains __ACK__ (1) if the peer owns the requested chunk, or __NACK__ (0) otherwise.
<br>
`CLIENT_FINISHED` - Sent by a client to the tracker, signaling that it has finished downloading its files.
<br>
`ALL_FINISHED` - Sent by the tracker to all clients, signaling them to shut down.

## Client

Clients begin by reading the input file, constructing the following structures:
- `files` - vector of File structures, containing the name and ordered hashes of each file; this is sent to and saved by the tracker
- `owned_chunks` - unordered map containing all the hashes for owned segments; this is used by the client throughout the program, allowing for efficient lookup by the upload thread
- `needed_files` - vector of strings containing the names of the files which the client needs to download

After sending the files vector to the tracker, clients wait to receive a broadcast from it, then begin downloading and uploading files.

### Download

Clients download files one by one, performing the following steps:
1. Send a _SWARM_REQUEST_ for the current file to the tracker.
2. After receiving the list of peers and ordered chunk hashes, for each chunk:
    - Choose a random peer, excluding itself, to vary the peers from which chunks are requested
    - Send a _CHUNK_REQUEST_ to the selected peer
    - If the response is _ACK_, save the hash to owned_chunks and move on to the next chunk, otherwise repeat the same steps
    - Send a _SWARM_UPDATE_ to the tracker after every 10 chunks downloaded
3. Print all the hashes to a file

After downloading all files, the client sends a _CLIENT_FINISHED_ message to the tracker, then stops the download thread.

### Upload

The upload thread continously receives _CHUNK_REQUEST_ messages, doing the following:
1. If the message comes from another client:
    - search for the requested hash in owned_chunks
    - if the chunk is owned, send back a _CHUNK_REPLY_ containing an _ACK_, or a _NACK_ otherwise
2. If the message comes from the tracker:
    - this is the _ALL_FINISHED_ notification, meaning that the thread will exit the loop and shut down (since this thread only receives chunk requests, the _ALL_FINISHED_ tag actually has the same value as _CHUNK_REQUEST_)

## Tracker

The tracker first receives the files vectors from all the clients. It stores the data in an unordered map, using the file name as the key and a File structure as the value - this contains the file's peers and ordered hashes. The map structure allows the tracker to easily look up files requested by clients.

After completing this, it broadcasts a message to all clients, notifying them to begin downloading and uploading files.

The tracker keeps a counter of active clients. It continously receives messages, processing them depending on their type:
1. _SWARM_REQUEST_: sends the data about the requested file back to the client and adds the client to the file's swarm
2. _SWARM_UPDATE_: sends the vector of peers for the specified file back to the client
3. _CLIENT_FINISHED_: decrements the active_clients counter. If the counter reaches 0, exits the loop

After all clients finished downloading their files (the counter reaches 0), the tracker sends the _ALL_FINISHED_ message to each client.

## Files

The `File` structure contains a file's name, a vector of hashes and a vector of peers.

For simplicity, the same structure is used:
- by clients, for sending data about their files to the tracker; in this step, the peers vector is empty
- by the tracker, to store the files data and to send file information to clients
- by clients, to store the information about the file currently being downloaded

To also simplify transmission, this structure can be sent as a string which contains all its data.<br>
The sender serializes the File into a string, sends the size of the string, then the string itself (as a vector of chars). The receiver then deserializes the string, reconstructing the File structure. The same happens for a vector of Files.

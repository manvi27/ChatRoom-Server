# ChatRoom

## Overview

A distributed chat server system supporting multiple chat rooms with different message ordering guarantees. The system consists of servers and clients, where each client connects to one server, and servers communicate with each other using UDP-based multicast. 
### Features

Server Capabilities:

- Supports multiple chat rooms (minimum 10)
- Handles up to 250 clients total
- Implements three ordering modes:
- Unordered multicast
  - FIFO multicast
  - Totally ordered multicast
- Static membership configuration
- Verbose logging option (-v)

Client Features:

- Connection to single server
- Command-line interface
- Nickname support
- Room management


### Building and Running

Prerequisites

    C++ compiler
    Make build system
    UDP socket support

### Compilation

`
bash
make
`

### Running the Server

bash
./chatserver <config_file> <server_index> [-v] [-o <ordering_mode>]

Ordering modes:

    unordered (default)
    fifo
    total

### Running the Client

bash
./chatclient <server_address:port>

#### Configuration File Format
Without Proxy

text
`
127.0.0.1:5000
127.0.0.1:5001
127.0.0.1:5002
`
With Proxy

text
`
127.0.0.1:8000,127.0.0.1:5000
127.0.0.1:8001,127.0.0.1:5001
127.0.0.1:8002,127.0.0.1:5002
`
### Testing

Automated Testing
Use the provided stress tester:

bash
`
./stresstest -o <ordering_mode> -m <messages> -c <clients> -g <groups> <config_file>
`

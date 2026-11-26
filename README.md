# Virtual Superscope
A distributed system project simulating satellite collaboration for taking
super-resolution images.

Author:
```
Minh Hieu Nguyen
Akshay Krishnan Anilkumar Manjusha
Vaishnave Kunnuvakkam Vinjimoor
Enrique Job Calderon Olalde
```

## Overview

## Setup
First, clone this repo with all its submodule:
```bash
git clone --recursive https://github.com/hieuristic/superscope.git
```
If you already clone but not recursively, you can run:
```bash
git submodule update --init --recursive
```

We use justfile script to simply the compilation. To compile either `viewer` or
`superscope`, you can run
```
just compile <target>
```
where `<target>` can be replaced with `viewer` or `superscope`.

Similarly, to run either the `viewer` or `superscope`, you can run:
```
just run <target>
```

To setup the container run (this is for MacOS only)
```
brew install container
container system start
just build container

# running sattelite server
just run container <name> <id>

# or using the launcher
just run launcher
>13  # this will now launch 13 satellite container

```

## Architecture

```
                      ┌──────────────────────────────────────────┐
                      │           client_main (Client)           │
                      │  Discovery (UDP) ──► TCP (framing/socket)│
                      └────────────────────┬─────────────────────┘
                                           │ 1. WhoIsLeader (UDP)
                                           │ 2. JobRequest / Panorama (TCP)
                                           ▼
┌────────────────────────────────────────────────────────────────────────────┐
│                        server_main  ──►  ServerNode                        │
│                                                                            │
│   ┌──────────────┐    ┌──────────────┐    ┌───────────────┐                │
│   │  Discovery   │───►│  ITopology   │◄───│FailureDetector│                │
│   │ (UDP bcast)  │    │(RingTopology)│    │ (UDP 500ms HB)│                │
│   └──────────────┘    └──────┬───────┘    └───────┬───────┘                │
│                              │ next_hop()         │ on_fail()              │
│                              ▼                    ▼                        │
│                       ┌────────────────────────────┐                       │
│                       │     Election (LCR/TCP)     │                       │
│                       └──────┬──────────────┬──────┘                       │
│                   winner=me  │              │  winner=other                │
│                              ▼              ▼                              │
│               ┌──────────────────┐    ┌──────────────────────────┐         │
│               │    LeaderCtx     │    │        WorkerCtx         │         │
│               │  Sequencer       │    │  HoldBackQueue (mcast)   │         │
│               │   └─► UDP mcast ─┼───►│   └─► Sensor.capture()   │         │
│               │  Client TCP srv  │    │        └─► AggPipeline   │         │
│               │   └─► Stitcher   │◄───┼── TCP tile (upstream_hop)│         │
│               └──────────────────┘    └──────────────────────────┘         │
└────────────────────────────────────────────────────────────────────────────┘
         ▲                                                      ▲
         └────────── all networking built on: ──────────────────┘
                 socket.h  •  framing.h  •  messages.h  •  types.h
```

## Leader election

Satellites form a logical ring sorted by numeric node ID. Each node forwards its
own ID at election start; the largest ID that completes the ring is announced
as leader and that announcement is forwarded once around the ring. Elections
run over the existing TCP peer connections, and the singleton swarm elects
itself without sending a network message.

The implementation expects stable, unique, ordered numeric IDs (the current
`NodeId` type is 32-bit). For real UUIDs, define a consistent ordering before
using LCR. To run the election protocol simulation test, build and run
`test_election` from the generated build directory.

## Heartbeats and failure detection

Each server sends a UDP heartbeat directly to every other active member every
500 ms. A peer is declared failed after 1.5 seconds without a heartbeat; the
failure callback removes it from the ring and starts a new election. The
failure detector test (`test_failure`) checks live heartbeat exchange and
failure notification between two local UDP endpoints.

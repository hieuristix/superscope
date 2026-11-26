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

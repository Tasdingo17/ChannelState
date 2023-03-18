# ChannelState

This tool estimates state of network channel between two hosts.

By 'channel state' we assume:
- available bandwidth
- packet loss parcentage
- round trip time (RTT)
- jitter 

## Prerequisites

TODO

## Build

cmake required

```
mkdir build
cd build
cmake -S ../ -B .
make
```

## Run

Tool is launched on 2 hosts and it continuisly estimates channel state.

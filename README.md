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
Run with superuser priveledges.

FIXME: if in build/ directory after building

On receiver:
```
./launch_chest -R
```

On sender:
```
./launch_chest -S <receiver ip>
```
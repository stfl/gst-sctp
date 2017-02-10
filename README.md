GStreamer SCTP Plugin

sctpsrc
sctpsink

depends on: libusrsctp github.com/sctplab/usrsctp

## Build // Install

build **libusrsctp** with cmake

```bash
mkdir buildusrsctp
cd buildusrsctp
cmake ../usrsctp/usrsctplib
cmake --build .

sudo make install
```

build **examples** with *meson.build* and *ninja*

```bash
mkdir build
meson build
cd build
ninja

sudo ninja install
```

## Run

```bash
export LD_LIBRARY_PATH=/path/to/buildusrsctp:${LD_LIBRARY_PATH}
sudo LD_LIBRARY_PATH=/path/to/buildusrsctp ./echo_server
sudo LD_LIBRARY_PATH=/path/to/buildusrsctp ./client 127.0.0.1 7
```

when *installed* with make install

```bash
sudo LD_LIBRARY_PATH=/usr/local/lib/ ./client 127.0.0.1 7
sudo LD_LIBRARY_PATH=/usr/local/lib/ ./echo_server
```

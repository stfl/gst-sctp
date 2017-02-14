GStreamer SCTP Plugin

sctpsrc
sctpsink

depends on: libusrsctp github.com/sctplab/usrsctp

## Build // Install

build **libusrsctp** with cmake

```bash
mkdir usrsctp/build
cd usrsctp/build/
cmake ../usrsctplib
cmake --build .

# sudo make install
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
export LD_LIBRARY_PATH=/home/slendl/Projects/gst/gst-sctp/usrsctp/build:${LD_LIBRARY_PATH}
export GST_PLUGIN_PATH=/home/slendl/Projects/gst/gst-sctp/build

sudo LD_LIBRARY_PATH=/path/to/usrsctp/build ./echo_server
sudo LD_LIBRARY_PATH=/path/to/usrsctp/build ./client 127.0.0.1 7

sudo -E LD_LIBRARY_PATH=/home/slendl/Projects/gst/gst-sctp/usrsctp/build \
   GST_DEBUG="3,basesink:4,sctpsink:6" \
   ./build/rtpsctp/rtpsctpsend 2>&1
```

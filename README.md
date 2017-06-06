GStreamer SCTP Plugin

**sctpsrc**
**sctpsink**

depends on: libusrsctp github.com/sctplab/usrsctp

# Build // Install

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

# Run

```bash
./run build/rtpsctp/rtpsctpsend -d "4,basesink:8,gstutils:5,buffer:9,sctpsink:8,videotestsrc:4,textoverlay:8"

./run build/rtpsctp/rtpsctprecv -d "4,gstutils:5,basesrc:8,pushsrc:8,sctpsrc:8,rtpbasedepayload:5,rtpbuffer:9,buffer:9" 
```

The GST_DEBUG string can be specified with `-d`
the run script offers the options to build the project `-d`, build usersctp `-u`, fetch from
upstream `-f` and so on..
`--dot [dir]` to output the .dot files when the pipeline enters playing state


Run directly:

```bash
export LD_LIBRARY_PATH=/home/slendl/Projects/gst/gst-sctp/usrsctp/build:${LD_LIBRARY_PATH}
export GST_PLUGIN_PATH=/home/slendl/Projects/gst/gst-sctp/build

sudo LD_LIBRARY_PATH=/path/to/usrsctp/build ./echo_server
sudo LD_LIBRARY_PATH=/path/to/usrsctp/build ./client 127.0.0.1 7

sudo -E LD_LIBRARY_PATH=/home/slendl/Projects/gst/gst-sctp/usrsctp/build \
   GST_DEBUG="3,basesink:4,sctpsink:6" \
   ./build/rtpsctp/rtpsctpsend 2>&1
```

## convert .dot files to .png

``` bash
dot dot/sender.dot -Tpng -o dot/sender.png
dot dot/receiver.dot -Tpng -o dot/reveiver.png
```

# Setup for Experiments

briefly:

two VMs with network interfaces like this

```sh
11.1.1.1  <--->  11.1.1.2
12.1.1.1  <--->  12.1.1.2
```


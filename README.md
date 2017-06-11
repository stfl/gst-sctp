This is the experimentation test pad for my master thesis.

The thesis focuses on the improvement of real-time multimedia streaming to minimize delay while
improving the failure resilience through using multi homed endpoints.
The improvements through a newly developed transmission scheme on transport layer are evaluated. For
this an extension to the transport layer protocol SCTP is developed.
 

This repo contains a GStreamer SCTP Plugin containing the following two pipeline elements.

**sctpsrc**
**sctpsink**

The implementation uses: [libusrsctp](https://github.com/sctplab/usrsctp)

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
-V variant to set the pipeline variant

**Available Variants**
- (udp)    UDP
- (single) SCTP Single homed
- (cmt)    SCTP CMT with Buffer Splitting
- (dupl)   SCTP where frames are duplicated to both paths to increase path failure resilience
- (dpr)    SCTP Deadline Based Preventive Retransmission (DPR)

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
11.1.1.1:1111  <--->  11.1.1.2:2221
12.1.1.1:1112  <--->  12.1.1.2:2222
```


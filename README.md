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

## build **libusrsctp** with cmake/ninja

```bash
mkdir usrsctp/build
cd usrsctp/build/
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ../usrsctplib -G Ninja
ninja

# cmake --build .
# sudo ninja install
```

## build the plugin with *meson+ninja*
### plain version:

```bash
mkdir build
meson build --prefix $(pwd)/build --buildtype=release --strip -D SCTP_DEBUG=false
cd build
ninja

sudo ninja install
```

### debug version:

```bash
mkdir build_debug
meson build_debug --prefix $(pwd)/build_debug --buildtype=debugoptimized -D SCTP_DEBUG=true
cd build_debug
ninja

sudo ninja install
```

# Run

```bash
./run build/rtpsctp/rtpsctpsend -d "4,basesink:8,gstutils:5,buffer:9,sctpsink:8,videotestsrc:4,textoverlay:8" -t cmt

./run build/rtpsctp/rtpsctprecv -d "4,gstutils:5,basesrc:8,pushsrc:8,sctpsrc:8,rtpbasedepayload:5,rtpbuffer:9,buffer:9"  -t cmt
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

## Overview 

two VMs with network interfaces like in the following. It has to be one global and one private IP in
order to make the source IP selection work in usrsctp. This is a limitation in usrsctp in this
specific setup. UDP encapsulation might help with that.

```sh
192.168.0.1  <--->  192.168.0.2
12.0.0.1     <--->  12.0.0.2
```

## NFS Setup

*/etc/fstab* on Receiver
```
/home/slendl/Projects/gst/gst-sctp /srv/nfs/gst-sctp            none bind 0 0
/usr/lib/gstreamer-1.0/            /srv/nfs/gststream-1.0-build none bind 0 0
```

*/etc/exports*
```
/srv/nfs                     192.168.153.128/24(rw,fsid=root,no_subtree_check)
/srv/nfs/gst-sctp            192.168.153.128/24(rw,no_subtree_check,nohide)
/srv/nfs/gststream-1.0-build 192.168.153.128/24(rw,no_subtree_check,nohide)
```

*/etc/fstab* on Sender
```
192.168.153.130:/gst-sctp            /home/slendl/Projects/gst/gst-sctp nfs noauto,x-systemd.automount,x-systemd.device-timeout=10,timeo=14,x-systemd.idle-timeout=1min 0 0
192.168.153.130:/gstreamer-1.0-build /usr/lib/gstreamer-1.0/            nfs noauto,x-systemd.automount,x-systemd.device-timeout=10,timeo=14,x-systemd.idle-timeout=1min 0 0
```

```bash
bp=/home/slendl/Projects/gst/gst-plugins-base/build; \
gp=/home/slendl/Projects/gst/gst-plugins-good/build; \
(cd $bp ninja && sudo ninja install >/dev/null) && \
scp $bp/gst-libs/gst/rtp/GstRtp-1.0.gir root@clone:/usr/share/gir-1.0/ &
scp $bp/gst-libs/gst/rtp/GstRtp-1.0.typelib root@clone:/usr/lib/girrepository-1.0 &
(cd $gp; ninja && sudo ninja install >/dev/null) && \
scp $gp/gst/rtp/libgstrtp.so $bp/gst-libs/gst/rtp/libgstrtp-1.0.so*
root@clone:/usr/lib/gstreamer-1.0
scp  root@clone:/usr/share/gir-1.0/
```

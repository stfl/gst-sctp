project('gst-sctp', 'c',
    version : '1.10.0',
    meson_version : '>= 0.35.0',
    default_options : [ 'warning_level=1',
      'buildtype=debugoptimized' ])

    gst_version = meson.project_version()
version_arr = gst_version.split('.')
gst_version_major = version_arr[0]
gst_version_minor = version_arr[1]
gst_version_micro = version_arr[2]
if version_arr.length() == 4
  gst_version_nano = version_arr[3]
else
  gst_version_nano = 0
endif

plugins_install_dir = '@0@/gstreamer-1.0'.format(get_option('libdir'))

glib_req = '>= 2.40.0'
gst_req = '>= @0@.@1@.0'.format(gst_version_major, gst_version_minor)

cc = meson.get_compiler('c')

if cc.get_id() == 'msvc'
  # Ignore several spurious warnings for things gstreamer does very commonly
  # If a warning is completely useless and spammy, use '/wdXXXX' to suppress it
  # If a warning is harmless but hard to fix, use '/woXXXX' so it's shown once
  # NOTE: Only add warnings here if you are sure they're spurious
  add_global_arguments('/wd4018', '/wd4244', '/wd4996', language : 'c')
  # Disable SAFESEH with MSVC for plugins and libs that use external deps that
  # are built with MinGW
  noseh_link_args = ['/SAFESEH:NO']
else
  noseh_link_args = []
endif

cdata = configuration_data()

check_headers = [
    ['HAVE_DLFCN_H',      'dlfcn.h'],
    ['HAVE_FCNTL_H',      'fcntl.h'],
    ['HAVE_INTTYPES_H',   'inttypes.h'],
    ['HAVE_MEMORY_H',     'memory.h'],
    ['HAVE_PROCESS_H',    'process.h'],
    ['HAVE_STDINT_H',     'stdint.h'],
    ['HAVE_STDLIB_H',     'stdlib.h'],
    ['HAVE_STRINGS_H',    'strings.h'],
    ['HAVE_STRING_H',     'string.h'],
    ['HAVE_SYS_IOCTL_H',  'sys/ioctl.h'],
    ['HAVE_SYS_PARAM_H',  'sys/param.h'],
    ['HAVE_SYS_SOCKET_H', 'sys/socket.h'],
    ['HAVE_SYS_STAT_H',   'sys/stat.h'],
    ['HAVE_SYS_TIME_H',   'sys/time.h'],
    ['HAVE_SYS_TYPES_H',  'sys/types.h'],
    ['HAVE_UNISTD_H',     'unistd.h'],
    ]

foreach h : check_headers
  if cc.has_header(h.get(1))
    cdata.set(h.get(0), 1)
  endif
endforeach

check_functions = [
    ['HAVE_ASINH', 'asinh', '#include<math.h>'],
    ['HAVE_CLOCK_GETTIME', 'clock_gettime', '#include<time.h>'],
    ['HAVE_COSH', 'cosh', '#include<math.h>'],
    # check token HAVE_CPU_ALPHA
    # check token HAVE_CPU_ARM
    # check token HAVE_CPU_CRIS
    # check token HAVE_CPU_CRISV32
    # check token HAVE_CPU_HPPA
    # check token HAVE_CPU_I386
    # check token HAVE_CPU_IA64
    # check token HAVE_CPU_M68K
    # check token HAVE_CPU_MIPS
    # check token HAVE_CPU_PPC
    # check token HAVE_CPU_PPC64
    # check token HAVE_CPU_S390
    # check token HAVE_CPU_SPARC
    # check token HAVE_CPU_X86_64
    ['HAVE_DCGETTEXT', 'dcgettext', '#include<libintl.h>'],
    # check token HAVE_DIRECTSOUND
    # check token HAVE_EXPERIMENTAL
    # check token HAVE_EXTERNAL
    # check token HAVE_FPCLASS
    # check token HAVE_GCC_ASM
    ['HAVE_GETPAGESIZE', 'getpagesize', '#include<unistd.h>'],
    # check token HAVE_GETTEXT
    # check token HAVE_GST_V4L2
    # check token HAVE_IOS
    ['HAVE_ISINF', 'isinf', '#include<math.h>'],
    # check token HAVE_LIBV4L2
    ['HAVE_MMAP', 'mmap', '#include<sys/mman.h>'],
    # check token HAVE_OSS
    # check token HAVE_OSS4
    # check token HAVE_OSS_INCLUDE_IN_MACHINE
    # check token HAVE_OSS_INCLUDE_IN_ROOT
    # check token HAVE_OSS_INCLUDE_IN_SYS
    # check token HAVE_OSX_AUDIO
    # check token HAVE_OSX_VIDEO
    # check token HAVE_RDTSC
    ['HAVE_SINH', 'sinh', '#include<math.h>'],
    # check token HAVE_SUNAUDIO
    # check token HAVE_WAVEFORM
    ]

foreach f : check_functions
  if cc.has_function(f.get(1), prefix : f.get(2))
    cdata.set(f.get(0), 1)
  endif
endforeach

cdata.set('SIZEOF_CHAR',  cc.sizeof('char'))
cdata.set('SIZEOF_INT',   cc.sizeof('int'))
cdata.set('SIZEOF_LONG',  cc.sizeof('long'))
cdata.set('SIZEOF_SHORT', cc.sizeof('short'))
cdata.set('SIZEOF_VOIDP', cc.sizeof('void*'))

# Here be fixmes.
# FIXME: check if this is correct
cdata.set('HAVE_CPU_X86_64',    host_machine.cpu() == 'amd64')
cdata.set('HAVE_GCC_ASM',       cc.get_id() != 'msvc')
cdata.set('VERSION',            '"@0@"'.format(gst_version))
cdata.set('PACKAGE_VERSION',    '"@0@"'.format(gst_version))
cdata.set('GST_LICENSE',        '"LGPL"')
cdata.set('PACKAGE',            '"gst-plugins-good"')
cdata.set('GETTEXT_PACKAGE',    '"gst-plugins-good-1.0"')
cdata.set('PACKAGE_NAME',       '"GStreamer Good Plug-ins"')
cdata.set('GST_PACKAGE_NAME',   '"GStreamer Good Plug-ins"')
cdata.set('GST_PACKAGE_ORIGIN', '"Unknown package origin"') # FIXME: make configurable

# Mandatory GST deps
gst_dep = dependency('gstreamer-1.0', version : gst_req,
    fallback : ['gstreamer', 'gst_dep'])
gstbase_dep = dependency('gstreamer-base-1.0', version : gst_req,
    fallback : ['gstreamer', 'gst_base_dep'])
gstnet_dep = dependency('gstreamer-net-1.0', version : gst_req,
    fallback : ['gstreamer', 'gst_net_dep'])
if host_machine.system() != 'windows'
  gstcheck_dep = dependency('gstreamer-check-1.0', version : gst_req,
      fallback : ['gstreamer', 'gst_check_dep'])
endif
gstcontroller_dep = dependency('gstreamer-controller-1.0', version : gst_req,
    fallback : ['gstreamer', 'gst_controller_dep'])

gstpbutils_dep = dependency('gstreamer-pbutils-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'pbutils_dep'])
gstallocators_dep = dependency('gstreamer-allocators-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'allocators_dep'])
gstapp_dep = dependency('gstreamer-app-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'app_dep'])
gstaudio_dep = dependency('gstreamer-audio-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'audio_dep'])
gstfft_dep = dependency('gstreamer-fft-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'fft_dep'])
gstriff_dep = dependency('gstreamer-riff-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'riff_dep'])
gstrtp_dep = dependency('gstreamer-rtp-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'rtp_dep'])
gstrtsp_dep = dependency('gstreamer-rtsp-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'rtsp_dep'])
gstsdp_dep = dependency('gstreamer-sdp-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'sdp_dep'])
gsttag_dep = dependency('gstreamer-tag-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'tag_dep'])
gstvideo_dep = dependency('gstreamer-video-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'video_dep'])

gio_dep = dependency('gio-2.0', version : glib_req)
glib_deps = [dependency('glib-2.0', version : glib_req),
    dependency('gobject-2.0', version : glib_req)]


gst_args = ['-DHAVE_CONFIG_H']
configinc = include_directories('.')
# libsinc = include_directories('gst-libs')


##### include usrsctp ########

usrsctp_check_headers = [
    ['HAVE_USRSCTP_H', 'usrsctp.h'],
    ]

foreach h : check_headers
  if cc.has_header(h.get(1))
    cdata.set(h.get(0), 1)
  endif
endforeach

#### checking struct members
####--------------------------

check_struct_members = [

    ['HAVE_SA_LEN',    'struct sockaddr',      'sa_len',    '#include<sys/types.h>\n#include<sys/socket.h>'],
    ['HAVE_SIN_LEN',   'struct sockaddr_in',   'sin_len',   '#include<sys/types.h>\n#include<netinet/in.h>'],
    ['HAVE_SIN6_LEN',  'struct sockaddr_in6',  'sin6_len',  '#include<sys/types.h>\n#include<netinet/in.h>'],
    ['HAVE_SCONN_LEN', 'struct sockaddr_conn', 'sconn_len', '#include<usrsctp.h'],

    ['HAVE_SA_LEN',    'struct sockaddr',      'sa_len',    '#include <sys/types.h>'],
    ['HAVE_SA_LEN',    'struct sockaddr',      'sa_len',    '#include <sys/socket.h>'],
    ['HAVE_SIN_LEN',   'struct sockaddr_in',   'sin_len',   '#include <sys/types.h>'],
    ['HAVE_SIN_LEN',   'struct sockaddr_in',   'sin_len',   '#include <netinet/in.h>'],
    ['HAVE_SIN6_LEN',  'struct sockaddr_in6',  'sin6_len',  '#include <sys/types.h>'],
    ['HAVE_SIN6_LEN',  'struct sockaddr_in6',  'sin6_len',  '#include <netinet/in.h>'],
    ['HAVE_SCONN_LEN', 'struct sockaddr_conn', 'sconn_len', '#include <usrsctp.h'],
    ]

foreach m : check_struct_members
  if cc.has_member(m.get(1), m.get(2), prefix : m.get(3))
    cdata.set(m.get(0), 1)
  endif
endforeach

#### checking options
####------------------

check_options = ['SCTP_DEBUG', 'INET', 'INET6']
foreach o : check_options
  if get_option(o)
    cdata.set(o, 1)
  endif
endforeach


configure_file(input : 'config.h.meson',
    output : 'config.h',
    configuration : cdata)

th_dep = dependency('threads')

usrsctp_build_path = join_paths(meson.source_root(), 'usrsctp/build')
message('expecting libusrsctp at: ' + usrsctp_build_path)
usrsctp_dep = cc.find_library('libusrsctp', dirs : usrsctp_build_path)

# embedd the library path into the executable > with setuid LD_LIBRARY_PATH doesn't work
Rargs = ['-R '+usrsctp_build_path ]

Werror_args = [ # '-Werror', # can also be enabled with meson -werror
                '-Wno-error=unused-variable',
                '-Wno-error=unused-function',
                '-Wno-error=unused-label',
                '-Wno-error=unused-value' ]

subdir('sctp')
subdir('rtpudp')
subdir('rtpsctp')
subdir('sctpinittest')

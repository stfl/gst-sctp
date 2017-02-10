project('gst-sctp', 'c')

gst_dep = dependency('gstreamer-1.0')
gio_dep = dependency('gio-2.0')

usrsctp_inc = include_directories('usrsctp/usrsctplib')

# usrsctp_dep = declare_dependency(link_with : 'buildusrsctp', include_directories : usrsctp_inc)
# TODO better would be to add it with subproject()
usrsctp_build_path = join_paths(meson.source_root(), 'usrsctp/build')
message('expecting libusrsctp at: ' + usrsctp_build_path)
compiler_obj = meson.get_compiler('c')
usrsctp_dep = compiler_obj.find_library('libusrsctp', dirs : usrsctp_build_path)

subdir('sctp')
# subdir('sctpsrc/')

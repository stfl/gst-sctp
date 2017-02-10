sctp_sources = [
  'gstsctp.c',
  'gstsctpsrc.c',
  'gstsctpsink.c'
]

gstsctp = library('gstsctp',
  sctp_sources,
  dependencies : [gst_dep, gio_dep, usrsctp_dep],
  install : true,
)

  # c_args : gst_plugins_good_args,
# include_directories : [configinc, libsinc],
# install_dir : plugins_install_dir

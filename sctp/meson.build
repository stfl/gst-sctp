sctp_sources = [
      'gstsctp.c',
      'gstsctpsrc.c',
      'gstsctpsink.c'
      ]

gstsctp = library('gstsctp',
      sctp_sources,
      dependencies : [gst_dep, gstbase_dep, gstnet_dep, gio_dep, usrsctp_dep],
      c_args : gst_args,
      include_directories : configinc,
      install_dir : plugins_install_dir,
      install : true,
      )

# c_args : gst_plugins_good_args,
# include_directories : [configinc, libsinc],
# install_dir : plugins_install_dir

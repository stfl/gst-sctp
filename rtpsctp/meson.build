executable('rtpsctpsend', 'gstRtpSctpSend.c',
      dependencies : [gst_dep, gio_dep],
      c_args : [gst_args, Werror_args],
      include_directories : [configinc, usrsctpinc],
      # link_args : Rargs
      )

executable('rtpsctprecv', 'gstRtpSctpReceive.c',
      dependencies : [gst_dep, gio_dep],
      c_args : [gst_args, Werror_args],
      include_directories : [configinc, usrsctpinc],
      # link_args : Rargs,
      )

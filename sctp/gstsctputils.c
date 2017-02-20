#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <usrsctp.h>

void
usrsctp_debug_printf(const char *format, ...)
{
   va_list ap;
   gchar *out;

   va_start(ap, format);
   out = gst_info_strdup_vprintf(format, ap);
   /* vprintf(format, ap); */
   va_end(ap);

   out[strcspn(out, "\n")] = '\0';
   GST_DEBUG("%s", out);
   g_free(out);
}

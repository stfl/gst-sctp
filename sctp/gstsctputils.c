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

#include "gstsctputils.h"

#define GST_CAT_LEVEL_LOG(cat,level,object,...) G_STMT_START{		\
  if (G_UNLIKELY ((level) <= GST_LEVEL_MAX && (level) <= _gst_debug_min)) {						\
    gst_debug_log ((cat), (level), __FILE__, GST_FUNCTION, __LINE__,	\
        (GObject *) (object), __VA_ARGS__);				\
  }									\
}G_STMT_END

GST_DEBUG_CATEGORY_STATIC (GST_CAT_USRSCTP);
GST_DEBUG_CATEGORY_STATIC (GST_CAT_SCTPUTILS);

void gst_usrsctp_debug_init(){
   GST_DEBUG_CATEGORY_INIT (GST_CAT_USRSCTP, "usrsctp",
         GST_DEBUG_FG_GREEN,
         "usrsctp");

   GST_DEBUG_CATEGORY_INIT (GST_CAT_SCTPUTILS, "sctputils",
         GST_DEBUG_FG_MAGENTA,
         "sctputils");
}

void usrsctp_debug_printf(const char *format, ...) {
   va_list ap;
   gchar *out;

   va_start(ap, format);
   out = gst_info_strdup_vprintf(format, ap);
   /* if (G_UNLIKELY ((GST_LEVEL_DEBUG) <= GST_LEVEL_MAX && (GST_LEVEL_DEBUG) <= _gst_debug_min)) {
    *    gst_debug_log_valist (GST_CAT_USRSCTP, GST_LEVEL_DEBUG, "", "", 0, NULL,
    *          format, ap);
    * } */
   va_end(ap);

   out[strcspn(out, "\n")] = '\0';
   GST_CAT_TRACE(GST_CAT_USRSCTP,"%s", out);
   g_free(out);
}

GST_EXPORT void hexDump (char *desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    // Output description if given.
    if (desc != NULL)
        printf ("%s:\n", desc);

    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        printf("  NEGATIVE LENGTH: %i\n",len);
        return;
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.
            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf (" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf ("  %s\n", buff);
}

GST_EXPORT void print_rtp_header (GstElement *obj, unsigned char *buffer) {
   RTPHeader *rtph = (RTPHeader *)buffer;
   GST_CAT_DEBUG_OBJECT(GST_CAT_SCTPUTILS, obj,
         "RTPHeader: V:%u, P:%u, X:%u, CC:%u, M:%u PT:%u, Seq:%5u, TS:%10u, ssrc:%9u",
         rtph->version, rtph->P, rtph->X, rtph->CC, rtph->M, rtph->PT,
         rtph->seq_num, ntohl(rtph->TS), rtph->ssrc);
}

int usrsctp_addrs_to_string(struct sockaddr *addrs, int n, GString *str) {
   struct sockaddr *addr;
   addr = addrs;
   for (int i = 0; i < n; i++) {
      if (i > 0) {
         g_string_append(str, ", ");
      }
      switch (addr->sa_family) {
         case AF_INET:
            {
               struct sockaddr_in *sin;
               char buf[INET_ADDRSTRLEN];
               const char *name;

               sin = (struct sockaddr_in *)addr;
               name = inet_ntop(AF_INET, &sin->sin_addr, buf, INET_ADDRSTRLEN);
               g_string_append(str, name);
#ifndef HAVE_SA_LEN
               addr = (struct sockaddr *)((caddr_t)addr + sizeof(struct sockaddr_in));
#endif
               break;
            }
         case AF_INET6:
            {
               struct sockaddr_in6 *sin6;
               char buf[INET6_ADDRSTRLEN];
               const char *name;

               sin6 = (struct sockaddr_in6 *)addr;
               name = inet_ntop(AF_INET6, &sin6->sin6_addr, buf, INET6_ADDRSTRLEN);
               g_string_append(str, name);
#ifndef HAVE_SA_LEN
               addr = (struct sockaddr *)((caddr_t)addr + sizeof(struct sockaddr_in6));
#endif
               break;
            }
         default:
            break;
      }
#ifdef HAVE_SA_LEN
      addr = (struct sockaddr *)((caddr_t)addr + addr->sa_len);
#endif
   }
   return str->len;
}

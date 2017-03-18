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

#include "gstsctpsrc.h"

/* GST_DEBUG_CATEGORY_STATIC(gst_sctputils_debug_category);
 *
 * #define GST_CAT_DEFAULT gst_sctputils_debug_category
 *
 * G_DEFINE_TYPE_WITH_CODE (GstSctpUtils, gst_sctputils, GST_TYPE_BASE_SRC,
 *       GST_DEBUG_CATEGORY_INIT (gst_sctputils_debug_category, "sctputils",
 *          GST_DEBUG_BG_GREEN | GST_DEBUG_FG_RED | GST_DEBUG_BOLD,
 *          "debug category for sctpsink element")); */

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

void hexDump (char *desc, void *addr, int len) {
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



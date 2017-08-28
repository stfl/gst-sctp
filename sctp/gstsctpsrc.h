/* GStreamer
 * Copyright (C) 2016 FIXME <fixme@example.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_SCTPSRC_H_
#define _GST_SCTPSRC_H_

/* #include <gst/base/gstbasesrc.h> */
#include <gst/base/gstpushsrc.h>

G_BEGIN_DECLS

#define GST_TYPE_SCTPSRC   (gst_sctpsrc_get_type())
#define GST_SCTPSRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SCTPSRC,GstSctpSrc))
#define GST_SCTPSRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SCTPSRC,GstSctpSrcClass))
#define GST_IS_SCTPSRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SCTPSRC))
#define GST_IS_SCTPSRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SCTPSRC))

typedef struct _GstSctpSrc GstSctpSrc;
typedef struct _GstSctpSrcClass GstSctpSrcClass;

struct _GstSctpSrc {
  GstBaseSrc base_sctpsrc;

  /* properties */
  gchar *dest_ip;
  gchar *src_ip;
  gchar *dest_ip_secondary;
  gchar *src_ip_secondary;
  gint dest_port;
  gint src_port;

  guint32 timestamp_offset;

  /* gchar     *peer; */
  /* gint       peer_port; */

  gboolean  udp_encaps;
  gint      udp_encaps_port_local;
  gint      udp_encaps_port_remote;

  gboolean unordered;
  gboolean nr_sack;

  gboolean cmt;
  gboolean bs;

  struct socket *sock;
  gboolean  socket_open;

  gint       ttl;
  GstCaps   *caps;
  gint       buffer_size;
  guint64    timeout;

  gint       skip_first_bytes;
  gboolean   close_socket;
  /* gboolean   retrieve_sender_address; */

  guint64     pushed;

  // FIXME define as paramater
  struct sockaddr *laddrs;      // array of local sockaddr
  struct sockaddr *paddrs;      // array of peer sockaddr

};

struct _GstSctpSrcClass {
  /* GstBaseSrcClass base_sctpsrc_class; */
  GstPushSrcClass parent_class;
};

typedef struct _SctpSrcStats {
   guint32     pushed;
} SctpSrcStats;

GType gst_sctpsrc_get_type (void);

G_END_DECLS

#endif

// vim: ft=c

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

#ifndef _GST_SCTPSINK_H_
#define _GST_SCTPSINK_H_

#include <gst/base/gstbasesink.h>

G_BEGIN_DECLS

#define GST_TYPE_SCTPSINK   (gst_sctpsink_get_type())
#define GST_SCTPSINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SCTPSINK,GstSctpSink))
#define GST_SCTPSINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SCTPSINK,GstSctpSinkClass))
#define GST_IS_SCTPSINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SCTPSINK))
#define GST_IS_SCTPSINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SCTPSINK))

typedef struct _GstSctpSink GstSctpSink;
typedef struct _GstSctpSinkClass GstSctpSinkClass;

struct _GstSctpSink
{
  GstBaseSink base_sctpsink;

  /* properties */
  gchar     *address;
  gchar     *host;
  gint       port;
  gint       src_port;
  /* gint       ttl; */
  /* gint       buffer_size; */
  /* guint64    timeout; */
  /* gint       skip_first_bytes; */

  gboolean  udp_encaps;
  gint      udp_encaps_port_local;
  gint      udp_encaps_port_remote;

  struct socket *sock;
  /* our sockets */
  /* GSocket   *used_socket; */
  /* GInetSocketAddress *addr; */
  /* gboolean   external_socket; */

  /* gboolean   made_cancel_fd; */
  /* GCancellable *cancellable; */

  /* memory management */
  /* GstAllocator *allocator; */
  /* GstAllocationParams params; */

  /* GstMemory   *mem; */
  /* GstMapInfo   map; */
  /* GstMemory   *mem_max; */
  /* GstMapInfo   map_max; */
  /* GInputVector vec[2]; */

  /* gchar     *uri; */
};

struct _GstSctpSinkClass
{
  GstBaseSinkClass base_sctpsink_class;
};

GType gst_sctpsink_get_type (void);

G_END_DECLS

#endif

// vim: ft=c

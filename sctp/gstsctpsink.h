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
#include <netinet/in.h>

G_BEGIN_DECLS

#define GST_TYPE_SCTPSINK   (gst_sctpsink_get_type())
#define GST_SCTPSINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SCTPSINK,GstSctpSink))
#define GST_SCTPSINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SCTPSINK,GstSctpSinkClass))
#define GST_IS_SCTPSINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SCTPSINK))
#define GST_IS_SCTPSINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SCTPSINK))

typedef struct _GstSctpSink GstSctpSink;
typedef struct _GstSctpSinkClass GstSctpSinkClass;

enum DuplicationPolicy{
	DUPL_POLICY_OFF,
	DUPL_POLICY_DUPLICATE,
	DUPL_POLICY_DPR,
};

struct _GstSctpSink
{
  GstBaseSink base_sctpsink;

  /* properties */
  gchar *dest_ip;
  gchar *src_ip;
  gchar *dest_ip_secondary;
  gchar *src_ip_secondary;
  gint dest_port;
  gint src_port;

  struct sockaddr_in dest_addr;
  struct sockaddr_in dest_addr_secondary;
  struct sockaddr_in src_addr;
  struct sockaddr_in src_addr_secondary;

  gboolean  udp_encaps;
  gint      udp_encaps_port_local;
  gint      udp_encaps_port_remote;

  gboolean unordered;
  gboolean nr_sack;

  gboolean   cmt;
  gboolean   bs;
  gchar     *dupl_policy_string;
  enum DuplicationPolicy dupl_policy;

  gint   pr_policy;
  gint   pr_value;

  guint32   timestamp_offset;
  guint32   deadline;
  guint32   delay;
  guint32   delay_padding;

  struct socket *sock;
  gboolean  socket_open;

  GstClock *systemclock;

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

   guint stats_timer_id;
};

struct _GstSctpSinkClass
{
  GstBaseSinkClass base_sctpsink_class;
};

typedef struct _SctpSinkStats {
   guint32     received;
} SctpSinkStats;


GType gst_sctpsink_get_type (void);

G_END_DECLS

#endif

// vim: ft=c

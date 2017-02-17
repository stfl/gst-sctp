/* GStreamer
 * Copyright (C) 2017 Stefan Lendl <ste.lendl@gmail.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gstsctpsink
 *
 * The sctpsink element does FIXME stuff.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v fakesrc ! sctpsink ! FIXME ! fakesink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include "gstsctpsink.h"

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

int done = 0;

GST_DEBUG_CATEGORY_STATIC (gst_sctpsink_debug_category);
#define GST_CAT_DEFAULT gst_sctpsink_debug_category

#define SCTP_DEFAULT_HOST                "localhost"
#define SCTP_DEFAULT_PORT                9
#define SCTP_DEFAULT_SRC_PORT            4455
#define SCTP_DEFAULT_UDP_ENCAPS_PORT     9989
#define SCTP_DEFAULT_UDP_ENCAPS_SRC_PORT 9988
#define SCTP_DEFAULT_UDP_ENCAPS          FALSE

enum
{
   PROP_0,
   PROP_HOST,
   PROP_PORT,
   PROP_SRC_PORT,
   PROP_UDP_ENCAPS,
   PROP_UDP_ENCAPS_PORT,
   PROP_UDP_ENCAPS_SRC_PORT,
   /* FILL ME */
};

/* prototypes */

static void gst_sctpsink_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_sctpsink_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec);
static void gst_sctpsink_dispose (GObject * object);
static void gst_sctpsink_finalize (GstSctpSink * sctpsink);

static gboolean gst_sctpsink_activate_pull (GstBaseSink * sink, gboolean active);
/* static void gst_sctpsink_get_times (GstBaseSink * sink, GstBuffer * buffer, GstClockTime * start, GstClockTime * end); */
/* static gboolean gst_sctpsink_propose_allocation (GstBaseSink * sink, GstQuery * query); */
static gboolean gst_sctpsink_start (GstBaseSink * sink);
static gboolean gst_sctpsink_stop (GstBaseSink * sink);
/* static gboolean gst_sctpsink_unlock (GstBaseSink * sink); */
/* static gboolean gst_sctpsink_unlock_stop (GstBaseSink * sink); */
static gboolean gst_sctpsink_query (GstBaseSink * sink, GstQuery * query);
/* static gboolean gst_sctpsink_event (GstBaseSink * sink, GstEvent * event); */
/* static GstFlowReturn gst_sctpsink_wait_event (GstBaseSink * sink, GstEvent * event); */
/* static GstFlowReturn gst_sctpsink_prepare (GstBaseSink * sink, GstBuffer * buffer); */
/* static GstFlowReturn gst_sctpsink_prepare_list (GstBaseSink * sink, GstBufferList * buffer_list); */
/* static GstFlowReturn gst_sctpsink_preroll (GstBaseSink * sink, GstBuffer * buffer); */
static GstFlowReturn gst_sctpsink_render (GstBaseSink * sink, GstBuffer * buffer);
static GstFlowReturn gst_sctpsink_render_list (GstBaseSink * sink, GstBufferList * buffer_list);
static gboolean sctpsink_iter_render_list (GstBuffer **buffer, guint idx, gpointer user_data);

/* usrsctp functions */

static int usrsctp_receive_cb(struct socket *sock, union sctp_sockstore addr, void *data, size_t
      datalen, struct sctp_rcvinfo rcv, int flags, void *ulp_info);
void usrsctp_debug_printf(const char *format, ...);

/* pad templates */
static GstStaticPadTemplate gst_sctpsink_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
      GST_PAD_SINK,
      GST_PAD_ALWAYS,
      GST_STATIC_CAPS_ANY
      );

/* class initializa[con */
G_DEFINE_TYPE_WITH_CODE (GstSctpSink, gst_sctpsink, GST_TYPE_BASE_SINK,
      GST_DEBUG_CATEGORY_INIT (gst_sctpsink_debug_category, "sctpsink",
         GST_DEBUG_BG_GREEN | GST_DEBUG_FG_RED | GST_DEBUG_BOLD,
         "debug category for sctpsink element"));

static void
gst_sctpsink_class_init (GstSctpSinkClass * klass)
{
   GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
   GstBaseSinkClass *base_sink_class = GST_BASE_SINK_CLASS (klass);

   /* Setting up pads and setting metadata should be moved to
      base_class_init if you intend to subclass this class. */
   gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
         gst_static_pad_template_get (&gst_sctpsink_sink_template));


   gobject_class->set_property = gst_sctpsink_set_property;
   gobject_class->get_property = gst_sctpsink_get_property;
   gobject_class->dispose = gst_sctpsink_dispose;
   gobject_class->finalize = (GObjectFinalizeFunc) gst_sctpsink_finalize;
   base_sink_class->activate_pull = gst_sctpsink_activate_pull;
   /* base_sink_class->get_times = gst_sctpsink_get_times; */
   /* base_sink_class->propose_allocation = gst_sctpsink_propose_allocation; */
   base_sink_class->start = gst_sctpsink_start;
   base_sink_class->stop = gst_sctpsink_stop;
   /* base_sink_class->unlock = gst_sctpsink_unlock; */
   /* base_sink_class->unlock_stop = gst_sctpsink_unlock_stop; */
   base_sink_class->query = gst_sctpsink_query;
   /* base_sink_class->event = gst_sctpsink_event; */
   /* base_sink_class->wait_event = gst_sctpsink_wait_event; */
   /* base_sink_class->prepare = gst_sctpsink_prepare; */
   /* base_sink_class->prepare_list = gst_sctpsink_prepare_list; */
   /* base_sink_class->preroll = gst_sctpsink_preroll; */
   base_sink_class->render = gst_sctpsink_render;
   base_sink_class->render_list = gst_sctpsink_render_list;

   g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_HOST,
         g_param_spec_string ("host", "Host",
            "The host/IP of the endpoint send the packets to. The other side must be running",
            SCTP_DEFAULT_HOST, G_PARAM_READWRITE));
   g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_PORT,
         g_param_spec_int ("port", "Port",
            "The port to send the packets to",
            0, 65535, SCTP_DEFAULT_PORT,
            G_PARAM_READWRITE));

   g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SRC_PORT,
         g_param_spec_int ("src-port", "Source Port",
            "The port to bind the socket to",
            0, 65535, SCTP_DEFAULT_SRC_PORT,
            G_PARAM_READWRITE));

   g_object_class_install_property (gobject_class, PROP_UDP_ENCAPS,
         g_param_spec_boolean ("udp-encaps", "UDP encapsulation",
            "Enable UDP encapsulation",
            SCTP_DEFAULT_UDP_ENCAPS, G_PARAM_READWRITE));
   g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_UDP_ENCAPS_PORT,
         g_param_spec_int ("udp-encaps-port", "UDP encapuslation destination port",
            "The dest port used with UDP encapsulate",
            0, 65535, SCTP_DEFAULT_UDP_ENCAPS_PORT,
            G_PARAM_READWRITE));
   g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_UDP_ENCAPS_SRC_PORT,
         g_param_spec_int ("udp-encaps-src-port",  "UDP encapuslation source port",
            "The src port used with UDP encapsulate",
            0, 65535, SCTP_DEFAULT_UDP_ENCAPS_SRC_PORT,
            G_PARAM_READWRITE));

   gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
         "SCTP Sink", "Generic", "SCTP Sink",
         "Stefan Lendl <ste.lendl@gmail.com>");
}

static void
gst_sctpsink_init (GstSctpSink * sctpsink)
{
   GstPad *sinkpad = GST_BASE_SINK_PAD(sctpsink);
   GST_INFO("sctpsink: name: %s\n",GST_PAD_NAME(sinkpad));

   sctpsink->host = g_strdup (SCTP_DEFAULT_HOST);
   sctpsink->port = SCTP_DEFAULT_PORT;
}

void
gst_sctpsink_set_property (GObject * object, guint property_id,
      const GValue * value, GParamSpec * pspec)
{
   GstSctpSink *sctpsink = GST_SCTPSINK (object);

   switch (property_id) {
      case PROP_HOST: {
            const gchar *host;
            host = g_value_get_string (value);
            g_free (sctpsink->host);
            sctpsink->host = g_strdup (host);
            GST_INFO("set host:%s", sctpsink->host);
            break; }
      case PROP_PORT:
         sctpsink->port = g_value_get_int (value);
         GST_INFO("set port:%d", sctpsink->port);
         break;
      case PROP_UDP_ENCAPS_PORT:
         sctpsink->udp_encaps_port = g_value_get_int (value);
         GST_INFO("set UDP encapsulation port:%d", sctpsink->udp_encaps_port);
         break;
      case PROP_UDP_ENCAPS_SRC_PORT:
         sctpsink->udp_encaps_src_port = g_value_get_int (value);
         GST_INFO("set UDP encapsulation src port:%d", sctpsink->udp_encaps_src_port);
         break;
      default:
         G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
         break;
   }
}

void
gst_sctpsink_get_property (GObject * object, guint property_id,
      GValue * value, GParamSpec * pspec)
{
   GstSctpSink *sctpsink = GST_SCTPSINK (object);

   GST_DEBUG_OBJECT (sctpsink, "get_property");

   switch (property_id) {
      case PROP_HOST:
         g_value_set_string (value, sctpsink->host);
         break;
      case PROP_PORT:
         g_value_set_int (value, sctpsink->port);
         break;
      case PROP_SRC_PORT:
         g_value_set_int (value, sctpsink->src_port);
         break;
      case PROP_UDP_ENCAPS:
         g_value_set_boolean (value, sctpsink->udp_encaps);
         break;
      case PROP_UDP_ENCAPS_PORT:
         g_value_set_int (value, sctpsink->udp_encaps_port);
         break;
      case PROP_UDP_ENCAPS_SRC_PORT:
         g_value_set_int (value, sctpsink->udp_encaps_src_port);
         break;

      default:
         G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
         break;
   }
}

void
gst_sctpsink_dispose (GObject * object)
{
   GstSctpSink *sctpsink = GST_SCTPSINK (object);

   GST_DEBUG_OBJECT (sctpsink, "dispose");

   /* clean up as possible.  may be called multiple times */

   G_OBJECT_CLASS (gst_sctpsink_parent_class)->dispose (object);
}

void
gst_sctpsink_finalize (GstSctpSink * sctpsink)
{
   GST_DEBUG_OBJECT (sctpsink, "finalize");

   /* clean up object here */

   g_free (sctpsink->host);
   sctpsink->host = NULL;

   G_OBJECT_CLASS (gst_sctpsink_parent_class)->finalize ((GObject *) sctpsink);
}

/* start or stop a pulling thread */
static gboolean
gst_sctpsink_activate_pull (GstBaseSink * sink, gboolean active)
{
   GstSctpSink *sctpsink = GST_SCTPSINK (sink);

   GST_DEBUG_OBJECT (sctpsink, "activate_pull");

   return TRUE;
}

/* get the start and end times for syncing on this buffer */
/* static void
 * gst_sctpsink_get_times (GstBaseSink * sink, GstBuffer * buffer,
 *     GstClockTime * start, GstClockTime * end)
 * {
 *   GstSctpSink *sctpsink = GST_SCTPSINK (sink);
 *
 *   GST_DEBUG_OBJECT (sctpsink, "get_times");
 *
 * } */

/* propose allocation parameters for upstream */
/* static gboolean
 * gst_sctpsink_propose_allocation (GstBaseSink * sink, GstQuery * query)
 * {
 *   GstSctpSink *sctpsink = GST_SCTPSINK (sink);
 *
 *   GST_DEBUG_OBJECT (sctpsink, "propose_allocation");
 *
 *   return TRUE;
 * } */

static int
usrsctp_receive_cb(struct socket *sock, union sctp_sockstore addr, void *data, size_t datalen,
      struct sctp_rcvinfo rcv, int flags, void *ulp_info)
{
   char namebuf[INET6_ADDRSTRLEN];
   const char *name;
   uint16_t port;

   GST_INFO("usrsctp_receive_cb");

   if (data) {
      if (flags & MSG_NOTIFICATION) {
         GST_DEBUG("Notification of length %d received.", (int)datalen);
      } else {
         switch (addr.sa.sa_family) {
#ifdef INET
            case AF_INET:
               name = inet_ntop(AF_INET, &addr.sin.sin_addr, namebuf, INET_ADDRSTRLEN);
               port = ntohs(addr.sin.sin_port);
               break;
#endif
#ifdef INET6
            case AF_INET6:
               name = inet_ntop(AF_INET6, &addr.sin6.sin6_addr, namebuf, INET6_ADDRSTRLEN),
                    port = ntohs(addr.sin6.sin6_port);
               break;
#endif
            case AF_CONN:
               snprintf(namebuf, INET6_ADDRSTRLEN, "%p", addr.sconn.sconn_addr);
               name = namebuf;
               port = ntohs(addr.sconn.sconn_port);
               break;
            default:
               name = NULL;
               port = 0;
               break;
         }
         GST_DEBUG("Msg of length %d received from %s:%u on stream %d with SSN %u and TSN %u, PPID %u, context %u",
               (int)datalen, name, port, rcv.rcv_sid, rcv.rcv_ssn, rcv.rcv_tsn, ntohl(rcv.rcv_ppid),
               rcv.rcv_context);
         if (flags & MSG_EOR) {
            struct sctp_sndinfo snd_info;

            snd_info.snd_sid = rcv.rcv_sid;
            snd_info.snd_flags = 0;
            if (rcv.rcv_flags & SCTP_UNORDERED) {
               snd_info.snd_flags |= SCTP_UNORDERED;
            }
            snd_info.snd_ppid = rcv.rcv_ppid;
            snd_info.snd_context = 0;
            snd_info.snd_assoc_id = rcv.rcv_assoc_id;

            if (usrsctp_sendv(sock, data, datalen, NULL, 0, &snd_info, sizeof(struct sctp_sndinfo),
                     SCTP_SENDV_SNDINFO, 0) < 0) {
               GST_ERROR("sctp_sendv");
            }
         }
      }
      free(data);
      GST_DEBUG("post free");
   } else {
      // FIXME handle this properly
      // send the info upstream somehow....
      GST_INFO("received data NULL");
      usrsctp_close(sock);
   }
   return (1);
}

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

/* start and stop processing, ideal for opening/closing the resource */
static gboolean
gst_sctpsink_start (GstBaseSink * sink)
{
   GstSctpSink *sctpsink = GST_SCTPSINK (sink);

   GST_INFO_OBJECT (sctpsink, "start sctpsink");

   /* struct socket *sock; */
   struct sockaddr *addr, *addrs;
   struct sockaddr_in addr4;
   struct sockaddr_in6 addr6;
   /* struct sctp_udpencaps encaps; */
   /* char buffer[80]; */
   int i, n;

   GString *addr_string, *addr2_string;

   /* if (argc > 4) { */
   /*    usrsctp_init(atoi(argv[4]), NULL, usrsctp_debug_printf); */
   /* } else { */
   // FIXME udp encapsulation?
   /* usrsctp_init(9899, NULL, usrsctp_debug_printf); */
   usrsctp_init(0, NULL, usrsctp_debug_printf);
   /* } */
#ifdef SCTP_DEBUG
   usrsctp_sysctl_set_sctp_debug_on(SCTP_DEBUG_ALL);
#endif
   usrsctp_sysctl_set_sctp_blackhole(2);
   if ((sctpsink->sock = usrsctp_socket(AF_INET6, SOCK_STREAM, IPPROTO_SCTP,
                                       usrsctp_receive_cb, NULL, 0, NULL)) == NULL) {
      GST_ERROR("usrsctp_socket");
   }
   if (sctpsink->src_port) {  // with given source port
      memset((void *)&addr6, 0, sizeof(struct sockaddr_in6));
#ifdef HAVE_SIN6_LEN
      addr6.sin6_len = sizeof(struct sockaddr_in6);
#endif
      addr6.sin6_family = AF_INET6;
      addr6.sin6_port = htons( sctpsink->src_port );
      addr6.sin6_addr = in6addr_any;
      if (usrsctp_bind(sctpsink->sock, (struct sockaddr *)&addr6, sizeof(struct sockaddr_in6)) < 0) {
         GST_ERROR("usrsctp_bind");
      }
   }
   /* if (argc > 5) { // with udp encapsulation
    *    memset(&encaps, 0, sizeof(struct sctp_udpencaps));
    *    encaps.sue_address.ss_family = AF_INET6;
    *    encaps.sue_port = htons(atoi(argv[5]));
    *    if (usrsctp_setsockopt(sctpsink->sock, IPPROTO_SCTP, SCTP_REMOTE_UDP_ENCAPS_PORT, (const void*)&encaps,
    *             (socklen_t)sizeof(struct sctp_udpencaps)) < 0) {
    *       GST_ERROR("setsockopt");
    *    }
    * } */

   // clear the addresses again
   memset((void *)&addr4, 0, sizeof(struct sockaddr_in));
   memset((void *)&addr6, 0, sizeof(struct sockaddr_in6));
#ifdef HAVE_SIN_LEN
   addr4.sin_len = sizeof(struct sockaddr_in);
#endif
#ifdef HAVE_SIN6_LEN
   addr6.sin6_len = sizeof(struct sockaddr_in6);
#endif
   addr4.sin_family = AF_INET;
   addr6.sin6_family = AF_INET6;
   addr4.sin_port = htons( sctpsink->port );
   addr6.sin6_port = htons( sctpsink->port );
   if (inet_pton(AF_INET6, sctpsink->host, &addr6.sin6_addr) == 1) {
      if (usrsctp_connect(sctpsink->sock, (struct sockaddr *)&addr6, sizeof(struct sockaddr_in6)) < 0) {
         GST_ERROR("usrsctp_connect");
      }
   } else if (inet_pton(AF_INET, sctpsink->host, &addr4.sin_addr) == 1) {
      if (usrsctp_connect(sctpsink->sock, (struct sockaddr *)&addr4, sizeof(struct sockaddr_in)) < 0) {
         GST_ERROR("usrsctp_connect");
      }
   } else {
      GST_ERROR("Illegal destination address");
   }
   if ((n = usrsctp_getladdrs(sctpsink->sock, 0, &addrs)) < 0) {
      GST_ERROR("usrsctp_getladdrs");
   } else {
      addr_string = g_string_new("Local addresses: ");
      /* GString *addr_string = g_string_new(NULL);  */
      addr = addrs;
      for (i = 0; i < n; i++) {
         if (i > 0) {
            g_string_append(addr_string, ", ");
         }
         switch (addr->sa_family) {
            case AF_INET:
               {
                  struct sockaddr_in *sin;
                  char buf[INET_ADDRSTRLEN];
                  const char *name;

                  sin = (struct sockaddr_in *)addr;
                  name = inet_ntop(AF_INET, &sin->sin_addr, buf, INET_ADDRSTRLEN);
                  g_string_append(addr_string, name);
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
                  g_string_append(addr_string, name);
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
      /* GST_INFO("%s", addr_string->str); */
      g_string_free(addr_string, TRUE);
      usrsctp_freeladdrs(addrs);
   }

   // get the peer addresses..
   if ((n = usrsctp_getpaddrs(sctpsink->sock, 0, &addrs)) < 0) {
      GST_ERROR("usrsctp_getpaddrs");
   } else {
      addr2_string = g_string_new("Peer addresses: ");
      addr = addrs;
      for (i = 0; i < n; i++) {
         if (i > 0) {
            g_string_append(addr2_string, ", ");
         }
         switch (addr->sa_family) {
            case AF_INET:
               {
                  struct sockaddr_in *sin;
                  char buf[INET_ADDRSTRLEN];
                  const char *name;

                  sin = (struct sockaddr_in *)addr;
                  name = inet_ntop(AF_INET, &sin->sin_addr, buf, INET_ADDRSTRLEN);
                  g_string_append(addr2_string, name);
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
                  g_string_append(addr2_string, name);
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
      /* GST_INFO("%s", addr2_string->str); */
      g_string_free(addr2_string, TRUE);
      usrsctp_freepaddrs(addrs);
   }

   return TRUE;
}

static gboolean
gst_sctpsink_stop (GstBaseSink * sink)
{
   GstSctpSink *sctpsink = GST_SCTPSINK (sink);
   struct sctpstat stat;

   GST_DEBUG_OBJECT (sctpsink, "stop");

   if (!done) {
      if (usrsctp_shutdown(sctpsink->sock, SHUT_WR) < 0) {
         GST_ERROR("usrsctp_shutdown");
      }
   }

   usrsctp_get_stat(&stat);
   GST_DEBUG_OBJECT(sctpsink, "Number of packets (sent/received): (%u/%u)",
         stat.sctps_outpackets, stat.sctps_inpackets);

   while (usrsctp_finish() != 0) {
      sleep(1);
   }

   return TRUE;
}

/* unlock any pending access to the resource. subclasses should unlock
 * any function ASAP. */
/* static gboolean
 * gst_sctpsink_unlock (GstBaseSink * sink)
 * {
 *   GstSctpSink *sctpsink = GST_SCTPSINK (sink);
 *
 *   GST_DEBUG_OBJECT (sctpsink, "unlock");
 *
 *   return TRUE;
 * } */

/* Clear a previously indicated unlock request not that unlocking is
 * complete. Sub-classes should clear any command queue or indicator they
 * set during unlock */
/* static gboolean
 * gst_sctpsink_unlock_stop (GstBaseSink * sink)
 * {
 *   GstSctpSink *sctpsink = GST_SCTPSINK (sink);
 *
 *   GST_DEBUG_OBJECT (sctpsink, "unlock_stop");
 *
 *   return TRUE;
 * } */

/* notify subclass of query */
static gboolean
gst_sctpsink_query (GstBaseSink * sink, GstQuery * query)
{
   gboolean ret;
   /* GstSctpSink *sctpsink = GST_SCTPSINK (sink); */
   /* GST_DEBUG_OBJECT (sctpsink, "query: %s", GST_QUERY_TYPE_NAME(query)); */

   switch (GST_QUERY_TYPE (query)) {
      /* case GST_QUERY_CAPS:
       *    [> we should report the supported caps here <]
       *    ret = GST_STATIC_CAPS_ANY;
       *    break; */
      default:
         /* just call the default handler */
         ret = gst_pad_query_default (sink->sinkpad, GST_OBJECT(sink), query);
         break;
   }
   return ret;
}

/* notify subclass of event */
/*    static gboolean
 * gst_sctpsink_event (GstBaseSink * sink, GstEvent * event)
 * {
 *    gboolean ret;
 *    GstSctpSink *sctpsink = GST_SCTPSINK (sink);
 *
 *    GST_DEBUG_OBJECT (sctpsink, "event: %s", GST_EVENT_TYPE_NAME(event));
 *
 *    switch (GST_EVENT_TYPE (event)) {
 *       case GST_EVENT_EOS:
 *          [> end-of-stream, we should close down all stream leftovers here <]
 *          gst_sctpsink_stop (sink);
 *
 *          ret = gst_pad_event_default (sink->sinkpad, GST_OBJECT(sink), event);
 *          break;
 *       default:
 *          ret = gst_pad_event_default (sink->sinkpad, GST_OBJECT(sink), event);
 *          break;
 *    }
 *    return ret;
 * } */

/* wait for eos or gap, subclasses should chain up to parent first */
/*    static GstFlowReturn
 * gst_sctpsink_wait_event (GstBaseSink * sink, GstEvent * event)
 * {
 *    GstSctpSink *sctpsink = GST_SCTPSINK (sink);
 *
 *    GST_DEBUG_OBJECT (sctpsink, "wait_event");
 *
 *    return GST_FLOW_OK;
 * } */

/* notify subclass of buffer or list before doing sync */
/* static GstFlowReturn
 * gst_sctpsink_prepare (GstBaseSink * sink, GstBuffer * buffer)
 * {
 *   GstSctpSink *sctpsink = GST_SCTPSINK (sink);
 *
 *   GST_DEBUG_OBJECT (sctpsink, "prepare");
 *
 *   return GST_FLOW_OK;
 * } */

/** Called to prepare the buffer list for render_list . This function is called before
 * synchronisation is performed. */
/* static GstFlowReturn
 * gst_sctpsink_prepare_list (GstBaseSink * sink, GstBufferList * buffer_list)
 * {
 *   GstSctpSink *sctpsink = GST_SCTPSINK (sink);
 *
 *   GST_DEBUG_OBJECT (sctpsink, "prepare_list");
 *
 *   return GST_FLOW_OK;
 * } */

/* notify subclass of preroll buffer or real buffer */
/*    static GstFlowReturn
 * gst_sctpsink_preroll (GstBaseSink * sink, GstBuffer * buffer)
 * {
 *    GstSctpSink *sctpsink = GST_SCTPSINK (sink);
 *
 *    GST_DEBUG_OBJECT (sctpsink, "preroll");
 *
 *    return GST_FLOW_OK;
 * } */

/** Called when a buffer should be presented or output, at the correct moment if the GstBaseSink has
 * been set to sync to the clock. */
static GstFlowReturn
gst_sctpsink_render (GstBaseSink * sink, GstBuffer * buffer)
{
   GstSctpSink *sctpsink = GST_SCTPSINK (sink);
   /* GST_DEBUG_OBJECT (sctpsink, "render"); */

   usrsctp_sendv(sctpsink->sock, buffer, gst_buffer_get_size(buffer), NULL, 0, NULL, 0,
         SCTP_SENDV_NOINFO, 0);

   GST_DEBUG("got a buffer of size:%lu", gst_buffer_get_size(buffer));

   return GST_FLOW_OK;
}

/** Render a BufferList */
static GstFlowReturn
gst_sctpsink_render_list (GstBaseSink * sink, GstBufferList * buffer_list)
{
   /* GstSctpSink *sctpsink = GST_SCTPSINK (sink); */

   gst_buffer_list_foreach(buffer_list, sctpsink_iter_render_list, sink);

   return GST_FLOW_OK;
}

static gboolean
sctpsink_iter_render_list (GstBuffer **buffer, guint idx, gpointer user_data) {
   GstSctpSink *sctpsink = GST_SCTPSINK(user_data);

   GST_DEBUG_OBJECT(sctpsink, "iter through the list [%u]:%lu", idx, gst_buffer_get_size(*buffer));
   usrsctp_sendv(sctpsink->sock, buffer, gst_buffer_get_size(*buffer), NULL, 0, NULL, 0,
         SCTP_SENDV_NOINFO, 0);

   return TRUE;
}

// vim: ft=c

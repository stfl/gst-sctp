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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gstsctpsrc
 *
 * The sctpsrc element does FIXME stuff.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v fakesrc ! sctpsrc ! FIXME ! fakesink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstsctpsrc.h"
#include <gst/base/gstpushsrc.h>
#include <gst/gst.h>
#include "gstsctputils.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <usrsctp.h>
#include <netinet/sctp_constants.h>

#define BUFFER_SIZE 10240

GST_DEBUG_CATEGORY_STATIC(gst_sctpsrc_debug_category);
#define GST_CAT_DEFAULT gst_sctpsrc_debug_category

#define SCTP_DEFAULT_UDP_ENCAPS FALSE
#define SCTP_DEFAULT_UDP_ENCAPS_PORT_REMOTE 9988
#define SCTP_DEFAULT_UDP_ENCAPS_PORT_LOCAL 9989

/*  PORTS defined for the RECEIVER */

#define  SCTP_DEFAULT_DEST_IP_PRIMARY      "128.131.89.244"
//#define  SCTP_DEFAULT_DEST_IP_PRIMARY      "192.168.0.2"
#define  SCTP_DEFAULT_SRC_IP_PRIMARY       "128.131.89.238"
//#define  SCTP_DEFAULT_SRC_IP_PRIMARY       "192.168.0.1"
#define  SCTP_DEFAULT_DEST_IP_SECONDARY    "12.0.0.2"
#define  SCTP_DEFAULT_SRC_IP_SECONDARY     "12.0.0.1"
#define  SCTP_DEFAULT_DEST_PORT            22222
#define  SCTP_DEFAULT_SRC_PORT             11111

#define SCTP_DEFAULT_ASSOC_VALUE           47

#define  SCTP_DEFAULT_BS                   FALSE
#define  SCTP_DEFAULT_CMT                  FALSE

#define  SCTP_DEFAULT_UNORDED              TRUE
#define  SCTP_DEFAULT_NR_SACK              TRUE

/* #define SCTP_USRSCTP_DEBUG                   (SCTP_DEBUG_INDATA1|SCTP_DEBUG_TIMER1|SCTP_DEBUG_OUTPUT1|SCTP_DEBUG_OUTPUT1|SCTP_DEBUG_OUTPUT4|SCTP_DEBUG_INPUT1|SCTP_DEBUG_INPUT2) */
/* #define SCTP_USRSCTP_DEBUG                   SCTP_DEBUG_ALL */
#define SCTP_USRSCTP_DEBUG                   SCTP_DEBUG_NONE

/* prototypes */
static void gst_sctpsrc_set_property(GObject *object, guint property_id, const GValue *value,
                                     GParamSpec *pspec);
static void gst_sctpsrc_get_property(GObject *object, guint property_id, GValue *value,
                                     GParamSpec *pspec);
static void gst_sctpsrc_dispose(GObject *object);
static void gst_sctpsrc_finalize(GObject *object);

/* static GstCaps *gst_sctpsrc_get_caps (GstBaseSrc * src, GstCaps * filter); */
/* static gboolean gst_sctpsrc_negotiate (GstBaseSrc * src); */
/* static GstCaps *gst_sctpsrc_fixate (GstBaseSrc * src, GstCaps * caps); */
static gboolean gst_sctpsrc_set_caps (GstBaseSrc * src, GstCaps * caps);
/* static gboolean gst_sctpsrc_decide_allocation (GstBaseSrc * src, GstQuery * query); */
static gboolean gst_sctpsrc_start(GstBaseSrc *src);
static gboolean gst_sctpsrc_stop(GstBaseSrc *src);
/* static void gst_sctpsrc_get_times (GstBaseSrc * src, GstBuffer * buffer,
 * GstClockTime * start,
 * GstClockTime * end); */
/* static gboolean gst_sctpsrc_get_size (GstBaseSrc * src, guint64 * size); */
/* static gboolean gst_sctpsrc_is_seekable (GstBaseSrc * src); */
/* static gboolean gst_sctpsrc_prepare_seek_segment (GstBaseSrc * src, GstEvent
 * * seek, GstSegment *
 * segment); */
/* static gboolean gst_sctpsrc_do_seek (GstBaseSrc * src, GstSegment * segment);
*/
/* static gboolean gst_sctpsrc_unlock (GstBaseSrc * src); */
/* static gboolean gst_sctpsrc_unlock_stop (GstBaseSrc * src); */
/* static gboolean gst_sctpsrc_query (GstBaseSrc * src, GstQuery * query); */
/* static gboolean gst_sctpsrc_event (GstBaseSrc * src, GstEvent * event); */

static GstFlowReturn gst_sctpsrc_create(GstPushSrc *src, GstBuffer **buf);
/* static GstFlowReturn gst_sctpsrc_alloc (GstPushSrc * src, GstBuffer ** buf);
*/
/* static GstFlowReturn gst_sctpsrc_fill (GstPushSrc * src, GstBuffer * buf); */

/* static int sctpsrc_receive_cb(struct socket *sock, union sctp_sockstore addr, void *data,
 *                               size_t datalen, struct sctp_rcvinfo rcv, int flags, void *ulp_info); */

enum {
   PROP_0,
   PROP_HOST,
   PROP_PORT,
   /* PROP_PORT_REMOTE, */
   PROP_UDP_ENCAPS,
   PROP_UDP_ENCAPS_PORT_LOCAL,
   PROP_UDP_ENCAPS_PORT_REMOTE,
   PROP_USRSCTP_STATS,
   /* PROP_STATS, */
   PROP_PUSHED,

   PROP_PR,
   PROP_PR_VALUE,
   PROP_UNORDERED,
   PROP_CMT,
   PROP_BS,
   PROP_TS_OFFSET_VALUE,
   /* FILL ME */
};

/* pad templates */
static GstStaticPadTemplate gst_sctpsrc_src_template =
    GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

/* class initialization */
G_DEFINE_TYPE_WITH_CODE(GstSctpSrc, gst_sctpsrc, GST_TYPE_PUSH_SRC,
               GST_DEBUG_CATEGORY_INIT(gst_sctpsrc_debug_category, "sctpsrc",
                              GST_DEBUG_BG_YELLOW | GST_DEBUG_FG_RED | GST_DEBUG_BOLD,
                              "debug category for sctpsrc element"));

static void gst_sctpsrc_class_init(GstSctpSrcClass *klass) {
   GObjectClass *gobject_class     = G_OBJECT_CLASS(klass);
   GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS(klass);
   GstPushSrcClass *push_src_class = (GstPushSrcClass *)klass;

   /* Setting up pads and setting metadata should be moved to
      base_class_init if you intend to subclass this class. */
   gst_element_class_add_pad_template(GST_ELEMENT_CLASS(klass),
                                      gst_static_pad_template_get(&gst_sctpsrc_src_template));

   gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass), "SCTP packet receiver",
                                         "Source/Network", "Receive data over the network via SCTP",
                                         "Stefan Lendl <ste.lendl@gmail.com>");

   gobject_class->set_property = gst_sctpsrc_set_property;
   gobject_class->get_property = gst_sctpsrc_get_property;
   gobject_class->dispose      = gst_sctpsrc_dispose;
   gobject_class->finalize     = gst_sctpsrc_finalize;
   /* base_src_class->get_caps = GST_DEBUG_FUNCPTR (gst_sctpsrc_get_caps); */
   /* base_src_class->negotiate = GST_DEBUG_FUNCPTR (gst_sctpsrc_negotiate); */
   /* base_src_class->fixate = GST_DEBUG_FUNCPTR (gst_sctpsrc_fixate); */
   base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_sctpsrc_set_caps);
   /* base_src_class->decide_allocation = GST_DEBUG_FUNCPTR (gst_sctpsrc_decide_allocation); */
   base_src_class->start = GST_DEBUG_FUNCPTR(gst_sctpsrc_start);
   base_src_class->stop  = GST_DEBUG_FUNCPTR(gst_sctpsrc_stop);
   /* base_src_class->get_times = GST_DEBUG_FUNCPTR (gst_sctpsrc_get_times); */
   /* base_src_class->get_size = GST_DEBUG_FUNCPTR (gst_sctpsrc_get_size); */
   /* base_src_class->is_seekable = GST_DEBUG_FUNCPTR (gst_sctpsrc_is_seekable);
   */
   /* base_src_class->prepare_seek_segment = GST_DEBUG_FUNCPTR(gst_sctpsrc_prepare_seek_segment);
    */
   /* base_src_class->do_seek = GST_DEBUG_FUNCPTR (gst_sctpsrc_do_seek); */
   /* base_src_class->unlock = GST_DEBUG_FUNCPTR (gst_sctpsrc_unlock); */
   /* base_src_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_sctpsrc_unlock_stop);
   */
   /* base_src_class->query = GST_DEBUG_FUNCPTR (gst_sctpsrc_query); */
   /* base_src_class->event = GST_DEBUG_FUNCPTR (gst_sctpsrc_event); */

   push_src_class->create = gst_sctpsrc_create;
   /* push_src_class->alloc = GST_DEBUG_FUNCPTR (gst_sctpsrc_alloc); */
   /* push_src_class->fill = GST_DEBUG_FUNCPTR (gst_sctpsrc_fill); */

   /* g_object_class_install_property( gobject_class, PROP_HOST, */
   /*     g_param_spec_string("host", "Host", "The host IP address to receive packets from", */
   /*                         SCTP_DEFAULT_HOST, G_PARAM_READWRITE)); */
   /* g_object_class_install_property(gobject_class, PROP_PORT, */
   /*                                 g_param_spec_int("port", "Port", "The port packets are received", */
   /*                                                  0, 65535, SCTP_DEFAULT_PORT, */
   /*                                                  G_PARAM_READWRITE)); */

   g_object_class_install_property(gobject_class, PROP_UDP_ENCAPS,
       g_param_spec_boolean("udp-encaps", "UDP encapsulation", "Enable UDP encapsulation",
                            SCTP_DEFAULT_UDP_ENCAPS, G_PARAM_READWRITE));
   g_object_class_install_property(gobject_class, PROP_UDP_ENCAPS_PORT_LOCAL,
       g_param_spec_int("udp-encaps-port-local", "local UDP encapuslation port",
                        "The local port used with UDP encapsulate", 0, 65535,
                        SCTP_DEFAULT_UDP_ENCAPS_PORT_LOCAL, G_PARAM_READWRITE));
   g_object_class_install_property(gobject_class, PROP_UDP_ENCAPS_PORT_REMOTE,
       g_param_spec_int("udp-encaps-port-remote", "remote UDP encapuslation port",
                        "The remote port used with UDP encapsulate", 0, 65535,
                        SCTP_DEFAULT_UDP_ENCAPS_PORT_REMOTE, G_PARAM_READWRITE));

   g_object_class_install_property (gobject_class, PROP_USRSCTP_STATS,
         g_param_spec_pointer ("usrsctp-stats",  "usrsctp stats",
            "Stats (struct sctpstat *) provided by libusrsctp",
            G_PARAM_READABLE));
   /* g_object_class_install_property (gobject_class, PROP_STATS,
    *       g_param_spec_pointer ("stats",  "stats",
    *          "Stats (struct sctpsrc *)",
    *          G_PARAM_READABLE)); */

   g_object_class_install_property (gobject_class, PROP_TS_OFFSET_VALUE,
         g_param_spec_uint ("timestamp-offset",  "timestamp offset for RTP timestamp measurements",
            "timestamp_offset",
            0, G_MAXUINT, 0,
            G_PARAM_READWRITE));

   g_object_class_install_property(gobject_class, PROP_PUSHED,
       g_param_spec_uint64("pushed", "packets pushed", "Packets pushed to next element",
          0, G_MAXUINT64, 0, G_PARAM_READABLE));

   g_object_class_install_property (gobject_class, PROP_CMT,
         g_param_spec_boolean ("cmt", "CMT",
            "enable Concurrent Multipath Transmission",
            SCTP_DEFAULT_CMT, G_PARAM_READWRITE));
   g_object_class_install_property (gobject_class, PROP_BS,
         g_param_spec_boolean ("buffer-split", "BS",
            "enable buffer splitting for CMT",
            SCTP_DEFAULT_BS, G_PARAM_READWRITE));

   gst_usrsctp_debug_init();
}

static void gst_sctpsrc_init(GstSctpSrc *sctpsrc) {
   /* sctpsrc->host = g_strdup(SCTP_DEFAULT_HOST); */
   /* sctpsrc->port = SCTP_DEFAULT_PORT; */

   sctpsrc->udp_encaps             = SCTP_DEFAULT_UDP_ENCAPS;
   sctpsrc->udp_encaps_port_local  = SCTP_DEFAULT_UDP_ENCAPS_PORT_LOCAL;
   sctpsrc->udp_encaps_port_remote = SCTP_DEFAULT_UDP_ENCAPS_PORT_REMOTE;
   sctpsrc->pushed                 = 0;

   sctpsrc->nr_sack = SCTP_DEFAULT_NR_SACK;
   sctpsrc->unordered = SCTP_DEFAULT_UNORDED;

   sctpsrc->cmt = SCTP_DEFAULT_CMT;
   sctpsrc->bs = SCTP_DEFAULT_BS;

   sctpsrc->dest_ip = g_strdup(SCTP_DEFAULT_DEST_IP_PRIMARY);
   sctpsrc->dest_port = SCTP_DEFAULT_DEST_PORT;
   sctpsrc->src_ip = g_strdup(SCTP_DEFAULT_SRC_IP_PRIMARY);
   sctpsrc->src_port = SCTP_DEFAULT_SRC_PORT;

   sctpsrc->dest_ip_secondary = g_strdup(SCTP_DEFAULT_DEST_IP_SECONDARY);
   sctpsrc->src_ip_secondary = g_strdup(SCTP_DEFAULT_SRC_IP_SECONDARY);
   sctpsrc->timestamp_offset = 0;
}

void gst_sctpsrc_set_property(GObject *object, guint property_id, const GValue *value,
                              GParamSpec *pspec) {
   GstSctpSrc *sctpsrc = GST_SCTPSRC(object);
   /* GST_DEBUG_OBJECT (sctpsrc, "set_property"); */

   switch (property_id) {
   case PROP_UDP_ENCAPS:
      sctpsrc->udp_encaps = g_value_get_boolean(value);
      GST_DEBUG_OBJECT(sctpsrc, "set UDP encapsulation:%s", sctpsrc->udp_encaps ? "TRUE" : "FALSE");
      break;
   case PROP_UDP_ENCAPS_PORT_REMOTE:
      sctpsrc->udp_encaps_port_remote = g_value_get_int(value);
      GST_DEBUG_OBJECT(sctpsrc, "set UDP encapsulation port:%d", sctpsrc->udp_encaps_port_remote);
      break;
   case PROP_UDP_ENCAPS_PORT_LOCAL:
      sctpsrc->udp_encaps_port_local = g_value_get_int(value);
      GST_DEBUG_OBJECT(sctpsrc, "set UDP encapsulation src port:%d",
                       sctpsrc->udp_encaps_port_local);
      break;

      case PROP_CMT:
         sctpsrc->cmt = g_value_get_boolean (value);
         GST_DEBUG_OBJECT(sctpsrc, "set CMT:%s", sctpsrc->cmt ? "TRUE" : "FALSE");
         break;
      case PROP_BS:
         sctpsrc->bs = g_value_get_boolean (value);
         GST_DEBUG_OBJECT(sctpsrc, "set Buffer Split:%s", sctpsrc->bs ? "TRUE" : "FALSE");
         break;
      case PROP_TS_OFFSET_VALUE:
         sctpsrc->timestamp_offset = g_value_get_uint (value);
         GST_DEBUG_OBJECT(sctpsrc, "set timestamp offset value: %d", sctpsrc->timestamp_offset);
         break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
   }
}

void gst_sctpsrc_get_property(GObject *object, guint property_id, GValue *value,
                              GParamSpec *pspec) {
   GstSctpSrc *sctpsrc = GST_SCTPSRC(object);
   /* GST_DEBUG_OBJECT (sctpsrc, "get_property"); */

   switch (property_id) {
   case PROP_UDP_ENCAPS:
      g_value_set_boolean(value, sctpsrc->udp_encaps);
      break;
   case PROP_UDP_ENCAPS_PORT_REMOTE:
      g_value_set_int(value, sctpsrc->udp_encaps_port_remote);
      break;
   case PROP_UDP_ENCAPS_PORT_LOCAL:
      g_value_set_int(value, sctpsrc->udp_encaps_port_local);
      break;
   case PROP_PUSHED:
      g_value_set_uint64(value, sctpsrc->pushed);
      break;
   case PROP_USRSCTP_STATS: {
      struct sctpstat stats;
      usrsctp_get_stat(&stats);
      g_value_set_pointer (value, (gpointer *)&stats);
      break; }

      case PROP_CMT:
         g_value_set_boolean (value, sctpsrc->cmt);
      break;
      case PROP_BS:
         g_value_set_boolean (value, sctpsrc->bs);
      break;
      case PROP_TS_OFFSET_VALUE:
         g_value_set_uint (value, sctpsrc->timestamp_offset);
         break;

   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
   }
}

void gst_sctpsrc_dispose(GObject *object)
{
   GstSctpSrc *sctpsrc = GST_SCTPSRC(object);

   GST_DEBUG_OBJECT(sctpsrc, "dispose");

   /* clean up as possible.  may be called multiple times */

   G_OBJECT_CLASS(gst_sctpsrc_parent_class)->dispose(object);
}

void gst_sctpsrc_finalize(GObject *object)
{
   GstSctpSrc *sctpsrc = GST_SCTPSRC(object);
   GST_DEBUG_OBJECT(sctpsrc, "finalize");

   // FIXME: null-out all attributes
   g_free (sctpsrc->src_ip);
   sctpsrc->src_ip = NULL;
   g_free (sctpsrc->dest_ip);
   sctpsrc->dest_ip = NULL;

   g_free (sctpsrc->src_ip_secondary);
   sctpsrc->src_ip_secondary = NULL;
   g_free (sctpsrc->dest_ip_secondary);
   sctpsrc->dest_ip_secondary = NULL;

   // free all memory
   while (usrsctp_finish() != 0) {
      sleep(1);
   }

   G_OBJECT_CLASS(gst_sctpsrc_parent_class)->finalize(object);
}

/* get caps from subclass */
/* static GstCaps * gst_sctpsrc_get_caps (GstBaseSrc * src, GstCaps * filter)
 * {
 *   GstSctpSrc *sctpsrc = GST_SCTPSRC (src);
 *   GST_DEBUG_OBJECT (sctpsrc, "get_caps: %s", gst_caps_to_string(filter));
 *
 *   return NULL;
 * } */

/* decide on caps */
/* static gboolean gst_sctpsrc_negotiate (GstBaseSrc * src)
 * {
 *   GstSctpSrc *sctpsrc = GST_SCTPSRC (src);
 *
 *   GST_DEBUG_OBJECT (sctpsrc, "negotiate");
 *
 *   return TRUE;
 * } */

/* called if, in negotiation, caps need fixating */
/* static GstCaps *gst_sctpsrc_fixate(GstBaseSrc *src, GstCaps *caps) {
 *    GstSctpSrc *sctpsrc = GST_SCTPSRC(src);
 *    GST_DEBUG_OBJECT(sctpsrc, "fixate");
 *
 *    return NULL;
 * } */

/* notify the subclass of new caps */
static gboolean gst_sctpsrc_set_caps (GstBaseSrc * src, GstCaps * caps) {
  GstSctpSrc *sctpsrc = GST_SCTPSRC (src);
  GST_DEBUG_OBJECT (sctpsrc, "set_caps");

  return gst_pad_set_caps(src->srcpad, caps);
}

/* setup allocation query */
/* static gboolean gst_sctpsrc_decide_allocation (GstBaseSrc * src, GstQuery * query)
 * {
 *   GstSctpSrc *sctpsrc = GST_SCTPSRC (src);
 *   GST_DEBUG_OBJECT (sctpsrc, "decide_allocation");
 *
 *   return GST_BASE_SRC_CLASS(gst_sctpsrc_parent_class)->decide_allocation(src, query);
 * } */

/* start and stop processing, ideal for opening/closing the resource */
static gboolean gst_sctpsrc_start(GstBaseSrc *src)
{
   GstSctpSrc *sctpsrc = GST_SCTPSRC(src);

   struct sctp_udpencaps encaps;
   struct sctp_assoc_value av;
   const int on = 1;

   usrsctp_init(sctpsrc->udp_encaps ? sctpsrc->udp_encaps_port_local : 0, NULL,
                usrsctp_debug_printf_receiver);

#ifdef SCTP_DEBUG
   usrsctp_sysctl_set_sctp_debug_on(SCTP_USRSCTP_DEBUG);
   usrsctp_sysctl_set_sctp_logging_level(SCTP_LTRACE_ERROR_ENABLE|SCTP_LTRACE_CHUNK_ENABLE);
#endif

   usrsctp_sysctl_set_sctp_blackhole(2);
   usrsctp_sysctl_set_sctp_heartbeat_interval_default(5000); // (30000ms)
   usrsctp_sysctl_set_sctp_delayed_sack_time_default(30);   // 200 mimize sack delay */
   if (sctpsrc->nr_sack == TRUE)
      usrsctp_sysctl_set_sctp_nrsack_enable(1);                /* non-renegable SACKs */
   usrsctp_sysctl_set_sctp_ecn_enable(1);                   /* sctp_ecn_enable > default enabled */
   /* usrsctp_sysctl_set_sctp_enable_sack_immediately(1);      [> Enable I-Flag <] */

   /* if ((sctpsrc->sock = usrsctp_socket(AF_INET6, SOCK_SEQPACKET, IPPROTO_SCTP, */
   /* sctpsrc_receive_cb, NULL, 0, NULL)) == NULL) { */
   if ((sctpsrc->sock =
            usrsctp_socket(AF_INET6, SOCK_SEQPACKET, IPPROTO_SCTP, NULL, NULL, 0, NULL)) == NULL) {
      GST_ERROR_OBJECT(sctpsrc, "usrsctp_socket");
   }

   /* https://github.com/sctplab/usrsctp/blob/0.9.3.0/Manual.md#socket-options */
   /* https://tools.ietf.org/html/rfc6458#section-8 */
   if (usrsctp_setsockopt(sctpsrc->sock, IPPROTO_SCTP, SCTP_I_WANT_MAPPED_V4_ADDR,
                          (const void *)&on, (socklen_t)sizeof(int)) < 0) {
      GST_ERROR_OBJECT(sctpsrc, "usrsctp_setsockopt SCTP_I_WANT_MAPPED_V4_ADDR");
   }

   /* something about referencing with input data when using one-to-many sockets */
   memset(&av, 0, sizeof(struct sctp_assoc_value));
   av.assoc_id    = SCTP_ALL_ASSOC;
   av.assoc_value = SCTP_DEFAULT_ASSOC_VALUE;

   if (usrsctp_setsockopt(sctpsrc->sock, IPPROTO_SCTP, SCTP_CONTEXT, (const void *)&av,
                          (socklen_t)sizeof(struct sctp_assoc_value)) < 0) {
      GST_ERROR_OBJECT(sctpsrc, "usrsctp_setsockopt SCTP_CONTEXT");
   }

   /* describes SCTP receive information about a received message through recvmsg() */
   if (usrsctp_setsockopt(sctpsrc->sock, IPPROTO_SCTP, SCTP_RECVRCVINFO, &on, sizeof(int)) < 0) {
      GST_ERROR_OBJECT(sctpsrc, "usrsctp_setsockopt SCTP_RECVRCVINFO");
   }

   if (sctpsrc->udp_encaps) {
      memset(&encaps, 0, sizeof(struct sctp_udpencaps));
      encaps.sue_address.ss_family = AF_INET6;
      encaps.sue_port              = htons(sctpsrc->udp_encaps_port_remote);
      if (usrsctp_setsockopt(sctpsrc->sock, IPPROTO_SCTP, SCTP_REMOTE_UDP_ENCAPS_PORT,
                             (const void *)&encaps, (socklen_t)sizeof(struct sctp_udpencaps)) < 0) {
         GST_ERROR_OBJECT(sctpsrc, "usrsctp_setsockopt SCTP_REMOTE_UDP_ENCAPS_PORT");
      }
   }

   /* subscribe to the given events in event_types[] */
   struct sctp_event event;
   uint16_t event_types[] = {SCTP_SHUTDOWN_EVENT
                           /* , SCTP_ASSOC_CHANGE */
                           /* , SCTP_PEER_ADDR_CHANGE */
                           , SCTP_REMOTE_ERROR
                           /* , SCTP_ADAPTATION_INDICATION */
                           /* , SCTP_PARTIAL_DELIVERY_EVENT */
                           };
   memset(&event, 0, sizeof(event));
   event.se_assoc_id = SCTP_FUTURE_ASSOC;
   event.se_on       = 1;
   for (int i = 0; i < (unsigned int)(sizeof(event_types) / sizeof(uint16_t)); i++) {
      event.se_type = event_types[i];
      if (usrsctp_setsockopt(sctpsrc->sock, IPPROTO_SCTP, SCTP_EVENT, &event,
                             sizeof(struct sctp_event)) < 0) {
         GST_ERROR_OBJECT(sctpsrc, "usrsctp_setsockopt SCTP_EVENT");
      }
   }

   // disable any Nagle-like algorithms
   if (usrsctp_setsockopt(sctpsrc->sock, IPPROTO_SCTP, SCTP_NODELAY,
            (const void *)&(int){1}, (socklen_t)sizeof(int)) < 0) {
      GST_ERROR_OBJECT(sctpsrc, "usrsctp_setsockopt SCTP_NODELAY");
   }

   struct sockaddr_in addr4[2];
   memset(&addr4, 0, sizeof(struct sockaddr_in) * 2);
   addr4[0].sin_family      = addr4[1].sin_family = AF_INET;
   addr4[0].sin_port        = addr4[1].sin_port = htons(sctpsrc->src_port);

   if (inet_pton(AF_INET, sctpsrc->src_ip, &addr4[0].sin_addr) <= 0) {
      GST_ERROR_OBJECT(sctpsrc, "Illegal source address: %s %s", sctpsrc->src_ip, strerror(errno));
      return FALSE;
   }
   if (inet_pton(AF_INET, sctpsrc->src_ip_secondary, &addr4[1].sin_addr) <= 0) {
      GST_ERROR_OBJECT(sctpsrc, "Illegal source address: %s %s", sctpsrc->src_ip_secondary, strerror(errno));
      return FALSE;
   }
   /* addr4[0].sin_addr.s_addr        = INADDR_ANY; */
   GST_DEBUG_OBJECT(sctpsrc, "binding server to: %s, %s port: %d",
         sctpsrc->src_ip, sctpsrc->src_ip_secondary, sctpsrc->src_port);

   /* if (usrsctp_bind(sctpsrc->sock, (struct sockaddr *)&addr4[0], sizeof(struct sockaddr_in)) < 0) {
    *    GST_ERROR_OBJECT(sctpsrc, "usrsctp_bind failed: %s", strerror(errno));
    *    return FALSE;
    * } */
   if (usrsctp_bindx(sctpsrc->sock, (struct sockaddr *)&addr4, 2, SCTP_BINDX_ADD_ADDR) < 0) {
      GST_ERROR_OBJECT(sctpsrc, "usrsctp_bindx failed: %s", strerror(errno));
      return FALSE;
   }

   if (usrsctp_listen(sctpsrc->sock, 2) < 0) {
      GST_ERROR_OBJECT(sctpsrc, "usrsctp_listen failed: %s", strerror(errno));
   }

   /* struct sockaddr *addr_accept;
    * socklen_t sock_l;
    * if (usrsctp_accept(sctpsrc->sock, addr_accept, &sock_l) == NULL) {
    *    GST_ERROR_OBJECT(sctpsrc, "usrsctp_accept failed: %s", strerror(errno));
    * }
    * GST_INFO_OBJECT(sctpsrc, "Association established _accept"); */

   /********** WAIT until session established */
/*    socklen_t from_len = (socklen_t)sizeof(struct sockaddr_in6);
 *    int flags    = 0;
 *    socklen_t infolen = (socklen_t)sizeof(struct sctp_rcvinfo);
 *    GstMapInfo map;
 *    GstBuffer *outbuf = NULL;
 *    outbuf = gst_buffer_new_and_alloc(BUFFER_SIZE);
 *    gst_buffer_map(outbuf, &map, GST_MAP_READWRITE);
 *
 *    struct sctp_rcvinfo rcv_info;
 *    unsigned int infotype;
 *    struct sockaddr_in6 from;
 *
 *    while(1) {
 *       int n = usrsctp_recvv(sctpsrc->sock, (void *)map.data, BUFFER_SIZE, (struct sockaddr *)&from,
 *             &from_len, (void *)&rcv_info, &infolen, &infotype, &flags);
 *       if (flags & MSG_NOTIFICATION) {
 *          union sctp_notification *sn = (union sctp_notification *)map.data;
 *          if (sn->sn_header.sn_type == SCTP_ASSOC_CHANGE &&
 *                sn->sn_assoc_change.sac_state == SCTP_COMM_UP) {
 *             GST_INFO_OBJECT(sctpsrc, "Association established");
 *             break;
 *          } else {
 *             GST_TRACE_OBJECT(sctpsrc, "Notificatjion of type %u length %llu received.",
 *                   sn->sn_header.sn_type, (unsigned long long)n);
 *             continue;
 *          }
 *       } else {
 *          GST_TRACE_OBJECT(sctpsrc, "Msg of length %llu received, still waiting for session setup",
 *                (unsigned long long)n);
 *       }
 *    } */

   /* GST_INFO_OBJECT(sctpsrc, "binding");
    * if (usrsctp_bindx(sctpsrc->sock, (struct sockaddr *)&addr4[1], 1, SCTP_BINDX_ADD_ADDR) < 0) {
    *    GST_ERROR_OBJECT(sctpsrc, "usrsctp_bindx failed: %s", strerror(errno));
    *    return FALSE;
    * }
    * GST_INFO_OBJECT(sctpsrc, "bind returned"); */
   /* gst_sctpsrc_stop((GstBaseSrc *)sctpsrc); */
   /* exit(0); */

   int n;
   struct sockaddr *addrs;
   GString *addr_string;
   if ((n = usrsctp_getladdrs(sctpsrc->sock, 0, &addrs)) < 0) {
      GST_ERROR_OBJECT(sctpsrc, "usrsctp_getladdrs: %s", strerror(errno));
   } else {
      addr_string = g_string_new("");
      usrsctp_addrs_to_string(addrs, n, addr_string);
      GST_INFO_OBJECT(sctpsrc, "SCTP Local addresses: %s", addr_string->str);
      g_string_free(addr_string, TRUE);
      usrsctp_freeladdrs(addrs);
   }

   sctpsrc->socket_open = TRUE;
   return TRUE;
}

static gboolean gst_sctpsrc_stop(GstBaseSrc *src)
{
   GstSctpSrc *sctpsrc = GST_SCTPSRC(src);
   GST_DEBUG_OBJECT(sctpsrc, "stop");

   struct sctpstat stat;
   usrsctp_get_stat(&stat);
   GST_INFO_OBJECT(sctpsrc, "Number of packets sent:\t\t\t%u",             stat.sctps_outpackets);
   GST_INFO_OBJECT(sctpsrc, "Number of packets received:\t\t%u",           stat.sctps_inpackets);
   GST_INFO_OBJECT(sctpsrc, "total input DATA chunks\t\t\t%u",             stat.sctps_recvdata);
   GST_INFO_OBJECT(sctpsrc, "Received duplicate Data:\t\t\t%u\t(%4.1f%%)",      stat.sctps_recvdupdata,
         (double)stat.sctps_recvdupdata/(double)stat.sctps_recvdata * 100);

   GST_INFO_OBJECT(sctpsrc, "output ordered chunks\t\t\t%u",               stat.sctps_outorderchunks);
   GST_INFO_OBJECT(sctpsrc, "output unordered chunks\t\t\t%u",             stat.sctps_outunorderchunks);
	GST_INFO_OBJECT(sctpsrc, "output control chunks\t\t\t%u",               stat.sctps_outcontrolchunks);
   GST_INFO_OBJECT(sctpsrc, "out of the blue\t\t\t\t%u",                   stat.sctps_outoftheblue);

   GST_INFO_OBJECT(sctpsrc, "input control chunks\t\t\t%u",                stat.sctps_incontrolchunks);
   GST_INFO_OBJECT(sctpsrc, "input ordered chunks\t\t\t%u",                stat.sctps_inorderchunks);
   GST_INFO_OBJECT(sctpsrc, "input unordered chunks\t\t\t%u",              stat.sctps_inunorderchunks);

	GST_INFO_OBJECT(sctpsrc, "total output SACKs\t\t\t%u",                  stat.sctps_sendsacks);
   GST_INFO_OBJECT(sctpsrc, "total input SACKs\t\t\t%u",                   stat.sctps_recvsacks);

   GST_INFO_OBJECT(sctpsrc, "ip_output error counter\t\t\t%u",             stat.sctps_senderrors);

   GST_INFO_OBJECT(sctpsrc, "Packet drop from middle box\t\t%u",           stat.sctps_pdrpfmbox);
   GST_INFO_OBJECT(sctpsrc, "P-drop from end host\t\t\t%u",                stat.sctps_pdrpfehos);
   GST_INFO_OBJECT(sctpsrc, "P-drops with data\t\t\t%u",                   stat.sctps_pdrpmbda);

   GST_INFO_OBJECT(sctpsrc, "data drops due to chunk limit reached\t%u", stat.sctps_datadropchklmt);
   GST_INFO_OBJECT(sctpsrc, "data drops due to rwnd limit reached\t%u",  stat.sctps_datadroprwnd);
   GST_INFO_OBJECT(sctpsrc, "max burst doesn't allow sending\t\t%u",       stat.sctps_maxburstqueued);
   GST_INFO_OBJECT(sctpsrc, "nagle allowed sending\t\t\t%u",               stat.sctps_naglesent);
   GST_INFO_OBJECT(sctpsrc, "nagle doesn't allow sending\t\t%u",           stat.sctps_naglequeued);

   usrsctp_close(sctpsrc->sock);


   if (! sctpsrc->socket_open)
      return TRUE;

   // free all memory
   while (usrsctp_finish() != 0) {
      sleep(1);
   }

   sctpsrc->socket_open = FALSE;

   return TRUE;
}

/* get the total size of the resource in bytes */
/* static gboolean gst_sctpsrc_get_size (GstBaseSrc * src, guint64 * size)
 * {
 *   GstSctpSrc *sctpsrc = GST_SCTPSRC (src);
 *   GST_DEBUG_OBJECT (sctpsrc, "get_size");
 *
 *   return TRUE;
 * } */

/* check if the resource is seekable */
/* static gboolean
 * gst_sctpsrc_is_seekable (GstBaseSrc * src)
 * {
 *   GstSctpSrc *sctpsrc = GST_SCTPSRC (src);
 *
 *   GST_DEBUG_OBJECT (sctpsrc, "is_seekable");
 *
 *   return TRUE;
 * } */

/* unlock any pending access to the resource. subclasses should unlock
 * any function ASAP. */
/* static gboolean
 * gst_sctpsrc_unlock (GstBaseSrc * src)
 * {
 *   GstSctpSrc *sctpsrc = GST_SCTPSRC (src);
 *
 *   GST_DEBUG_OBJECT (sctpsrc, "unlock");
 *
 *   return TRUE;
 * } */

/* Clear any pending unlock request, as we succeeded in unlocking */
/* static gboolean
 * gst_sctpsrc_unlock_stop (GstBaseSrc * src)
 * {
 *   GstSctpSrc *sctpsrc = GST_SCTPSRC (src);
 *
 *   GST_DEBUG_OBJECT (sctpsrc, "unlock_stop");
 *
 *   return TRUE;
 * } */

/* notify subclasses of a query */
/* static gboolean
 * gst_sctpsrc_query (GstBaseSrc * src, GstQuery * query)
 * {
 *   GstSctpSrc *sctpsrc = GST_SCTPSRC (src);
 *   GST_DEBUG_OBJECT (sctpsrc, "query: %s", GST_QUERY_TYPE_NAME(query));
 *
 *   return gst_pad_query_default (src->srcpad, GST_OBJECT(src), query);
 * } */

/* notify subclasses of an event */
/* static gboolean gst_sctpsrc_event(GstBaseSrc *src, GstEvent *event) {
 *    GstSctpSrc *sctpsrc = GST_SCTPSRC(src);
 *    GST_DEBUG_OBJECT(sctpsrc, "event: %s", gst_event_type_get_name (event->type));
 *
 *    return gst_pad_event_default(src->srcpad, GST_OBJECT(src), event);
 * } */

/* ask the subclass to create a buffer with offset and size, the default
 * implementation will call alloc and fill. */
static GstFlowReturn gst_sctpsrc_create(GstPushSrc *src, GstBuffer **buf) {
   GstSctpSrc *sctpsrc = GST_SCTPSRC(src);
   ssize_t n;
   int flags;

   char name[INET6_ADDRSTRLEN];
   /* char *buffer = g_malloc(BUFFER_SIZE); */
   /* buf = g_malloc(BUFFER_SIZE); */
   /* [BUFFER_SIZE]; // FIXME: change to a malloc! */

   socklen_t infolen;
   struct sctp_rcvinfo rcv_info;
   unsigned int infotype;
   struct sockaddr_in6 from;
   socklen_t from_len;
   GstBuffer *outbuf = NULL;

   // allocate the buffer
   GstMapInfo map;
   outbuf = gst_buffer_new_and_alloc(BUFFER_SIZE);
   gst_buffer_map(outbuf, &map, GST_MAP_READWRITE);

   from_len = (socklen_t)sizeof(struct sockaddr_in6);
   flags    = 0;
   infolen  = (socklen_t)sizeof(struct sctp_rcvinfo);

   n = usrsctp_recvv(sctpsrc->sock, (void *)map.data, BUFFER_SIZE, (struct sockaddr *)&from,
                     &from_len, (void *)&rcv_info, &infolen, &infotype, &flags);
   if (n > 0) {
      if (flags & MSG_NOTIFICATION) {
         union sctp_notification *sn = (union sctp_notification *)map.data;
         switch (sn->sn_header.sn_type) {
            case SCTP_SHUTDOWN_EVENT:
               GST_INFO_OBJECT(sctpsrc, "Shutdown revieved");

               /* gst_sctpsrc_stop(GST_BASE_SRC(sctpsrc)); */
               return GST_FLOW_EOS;
            case SCTP_REMOTE_ERROR:
               GST_INFO_OBJECT(sctpsrc, "Remote error received");
               /* gst_sctpsrc_stop(GST_BASE_SRC(sctpsrc)); */
               return GST_FLOW_EOS;
            default:
               GST_TRACE_OBJECT(sctpsrc, "Notificatjion of type %u length %llu received.",
                     sn->sn_header.sn_type, (unsigned long long)n);
               break;
         }
      } else {
         if (infotype == SCTP_RECVV_RCVINFO) {
            GST_TRACE_OBJECT(
                sctpsrc, "Msg len %4llu from [%s]:%u SID %d, "
                         "SSN %u, TSN %u, PPID %u, contxt %u, compl %d, U %d",
                (unsigned long long)n, inet_ntop(AF_INET6, &from.sin6_addr, name, INET6_ADDRSTRLEN),
                ntohs(from.sin6_port), rcv_info.rcv_sid, rcv_info.rcv_ssn, rcv_info.rcv_tsn,
                ntohl(rcv_info.rcv_ppid), rcv_info.rcv_context, (flags & MSG_EOR) ? 1 : 0,
                (rcv_info.rcv_flags & SCTP_UNORDERED) ? 1 : 0);
         } else {
            GST_TRACE_OBJECT(sctpsrc, "Msg of length %llu received from %s:%u, compl %d",
                             (unsigned long long)n,
                             inet_ntop(AF_INET6, &from.sin6_addr, name, INET6_ADDRSTRLEN),
                             ntohs(from.sin6_port), (flags & MSG_EOR) ? 1 : 0);
         }
         print_rtp_header((GstElement *)sctpsrc, map.data);
      }
   } else {
      GST_WARNING_OBJECT(sctpsrc, "GST_FLOW_EOS");
      return GST_FLOW_EOS;
   }


  /* use buffer metadata so receivers can also track the address */
  /* if (saddr) {
   *   gst_buffer_add_net_address_meta (outbuf, saddr);
   *   g_object_unref (saddr);
   *   saddr = NULL;
   * } */

   /* gst_buffer_unmap(*buf, &map); */

   *buf = GST_BUFFER_CAST(outbuf);
   sctpsrc->pushed++;


   /* *buf = buffer; */
   return GST_FLOW_OK;
}

/* ask the subclass to allocate an output buffer. The default implementation
 * will use the negotiated allocator. */
/* static GstFlowReturn gst_sctpsrc_alloc (GstPushSrc * src, GstBuffer ** buf)
 * {
 *   GstSctpSrc *sctpsrc = GST_SCTPSRC (src);
 *
 *   GST_DEBUG_OBJECT (sctpsrc, "alloc");
 *
 *   return GST_FLOW_OK;
 * } */

/* ask the subclass to fill the buffer with data from offset and size */
/* static GstFlowReturn gst_sctpsrc_fill (GstPushSrc * src, GstBuffer * buf)
 * {
 *   GstSctpSrc *sctpsrc = GST_SCTPSRC (src);
 *
 *   GST_DEBUG_OBJECT (sctpsrc, "fill");
 *
 *   return GST_FLOW_OK;
 * } */

static int sctpsrc_receive_cb(struct socket *sock, union sctp_sockstore addr, void *data,
                              size_t datalen, struct sctp_rcvinfo rcv, int flags, void *ulp_info) {
   char namebuf[INET6_ADDRSTRLEN];
   const char *name;
   uint16_t port;

   if (data) {
      if (flags & MSG_NOTIFICATION) {
         GST_INFO("Notification of length %d received.", (int)datalen);
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
         GST_INFO("Msg of length %d received from %s:%u on stream %d with SSN %u "
                  "and TSN %u, PPID "
                  "%u, context %u.",
                  (int)datalen, name, port, rcv.rcv_sid, rcv.rcv_ssn, rcv.rcv_tsn,
                  ntohl(rcv.rcv_ppid), rcv.rcv_context);
         if (flags & MSG_EOR) {
            struct sctp_sndinfo snd_info;

            snd_info.snd_sid   = rcv.rcv_sid;
            snd_info.snd_flags = 0;
            if (rcv.rcv_flags & SCTP_UNORDERED) {
               snd_info.snd_flags |= SCTP_UNORDERED;
            }
            snd_info.snd_ppid     = rcv.rcv_ppid;
            snd_info.snd_context  = 0;
            snd_info.snd_assoc_id = rcv.rcv_assoc_id;
            if (usrsctp_sendv(sock, data, datalen, NULL, 0, &snd_info, sizeof(struct sctp_sndinfo),
                              SCTP_SENDV_SNDINFO, 0) < 0) {
               GST_ERROR("sctp_sendv");
            }
         }
      }
      free(data);
   }
   return (1);
}

// vim: ft=c

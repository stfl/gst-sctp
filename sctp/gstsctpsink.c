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
#include <gst/rtp/gstrtpbuffer.h>

#include "gstsctpsink.h"
#include "gstsctputils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <usrsctp.h>
#include <netinet/sctp_constants.h>
/* #include <netinet/sctp.h> */

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>

#include <netdb.h>

int done = 0;

GST_DEBUG_CATEGORY_STATIC (gst_sctpsink_debug_category);
#define GST_CAT_DEFAULT gst_sctpsink_debug_category

// paths definition
#define  SCTP_DEFAULT_DEST_IP_PRIMARY         "192.168.0.1"
//#define  SCTP_DEFAULT_DEST_IP_PRIMARY         "12.0.0.1"
#define  SCTP_DEFAULT_SRC_IP_PRIMARY          "192.168.0.2"
//#define  SCTP_DEFAULT_SRC_IP_PRIMARY          "12.0.0.2"
#define  SCTP_DEFAULT_DEST_IP_SECONDARY       "128.131.89.238"
#define  SCTP_DEFAULT_SRC_IP_SECONDARY        "128.131.89.244"
#define  SCTP_DEFAULT_DEST_PORT       11111
#define  SCTP_DEFAULT_SRC_PORT        22222

#define  SCTP_DEFAULT_PR_POLICY               SCTP_PR_SCTP_NONE // 0x0000 /* Reliable transfer */
#define  SCTP_DEFAULT_PR_VALUE                0 //80  // ms

#define SCTP_PR_SCTP_ALL                      0x000f /* Used for aggregated stats */

#define  SCTP_DEFAULT_UNORDED                 TRUE
#define  SCTP_DEFAULT_NR_SACK                 TRUE

#define  SCTP_DEFAULT_UDP_ENCAPS_PORT_REMOTE  9989
#define  SCTP_DEFAULT_UDP_ENCAPS_PORT_LOCAL   9988
#define  SCTP_DEFAULT_UDP_ENCAPS              FALSE

#define  SCTP_DEFAULT_BS                      FALSE
#define  SCTP_DEFAULT_CMT                     FALSE
#define  SCTP_DEFAULT_DUPL_POLICY             "off"

#define SCTP_SNDBUF                           4294967295U // 2^30 = 134MB

#define  SCTP_DEFAULT_DEADLINE_US             450000
#define  SCTP_DEFAULT_DELAY                   0
#define  SCTP_DEFAULT_DELAY_PADDING_US        80000 // us

/* #define SCTP_USRSCTP_DEBUG                   (SCTP_DEBUG_INDATA1|SCTP_DEBUG_TIMER1|SCTP_DEBUG_OUTPUT1|SCTP_DEBUG_OUTPUT1|SCTP_DEBUG_OUTPUT4|SCTP_DEBUG_INPUT1|SCTP_DEBUG_INPUT2|SCTP_DEBUG_OUTPUT2) */
/* #define SCTP_USRSCTP_DEBUG                  (SCTP_DEBUG_TIMER3|SCTP_DEBUG_OUTPUT1|SCTP_DEBUG_OUTPUT2|SCTP_DEBUG_OUTPUT3|SCTP_DEBUG_OUTPUT4) //|SCTP_DEBUG_INDATA1|SCTP_DEBUG_INPUT1) //|SCTP_DEBUG_OUTPUT4) */
/* #define SCTP_USRSCTP_DEBUG                  (SCTP_DEBUG_OUTPUT4|SCTP_DEBUG_UTIL2) */
#define SCTP_USRSCTP_DEBUG                  SCTP_DEBUG_OUTPUT4
/* (SCTP_DEBUG_TIMER3|SCTP_DEBUG_UTIL2) */
/* #define SCTP_USRSCTP_DEBUG                   SCTP_DEBUG_ALL */

#define SCTP_PPID       99
#define SCTP_SID        1
#define SCTP_ASSOC_ID   1

enum
{
   PROP_0,
   PROP_HOST,
   PROP_PORT,
   PROP_SRC_PORT,
   PROP_UDP_ENCAPS,
   PROP_UDP_ENCAPS_PORT_REMOTE,
   PROP_UDP_ENCAPS_PORT_LOCAL,
   PROP_USRSCTP_STATS,
   PROP_PR,
   PROP_PR_VALUE,
   PROP_UNORDERED,
   PROP_CMT,
   PROP_BS,
   PROP_DUPL_POLICY,
   PROP_TS_OFFSET_VALUE,
   PROP_DEADLINE,
   PROP_DELAY,
   PROP_DELAY_PADDING,
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
static GstFlowReturn usrsctp_render_send (GstSctpSink *sctpsink, GstBuffer *buffer, gboolean last);

static gboolean sctpsink_render_list_iter (GstBuffer **buffer, guint idx, gpointer user_data);
/* static gboolean buffer_list_copy_data (GstBuffer ** buf, guint idx, gpointer data); */
/* static gboolean buffer_list_calc_size (GstBuffer ** buf, guint idx, gpointer data); */

/* static void print_rtp_header (GstSctpSink *obj, unsigned char *buffer); */
/* static gboolean stats_timer (gpointer priv); */
static void calc_retran_time(GstSctpSink * sink, guint8 *data, struct timeval *rtx_deadline);

/* usrsctp functions */

static int usrsctp_receive_cb(struct socket *sock, union sctp_sockstore addr, void *data, size_t
      datalen, struct sctp_rcvinfo rcv, int flags, void *ulp_info);

/* pad templates */
static GstStaticPadTemplate gst_sctpsink_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
      GST_PAD_SINK,
      GST_PAD_ALWAYS,
      GST_STATIC_CAPS_ANY
      );

/* class initialization */
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

   g_object_class_install_property (gobject_class, PROP_UDP_ENCAPS,
         g_param_spec_boolean ("udp-encaps", "UDP encapsulation",
            "Enable UDP encapsulation",
            SCTP_DEFAULT_UDP_ENCAPS, G_PARAM_READWRITE));
   /* g_object_class_install_property (gobject_class, PROP_UDP_ENCAPS_PORT_REMOTE, */
   /*       g_param_spec_int ("udp-encaps-port-remote", "remote UDP encapuslation port", */
   /*          "The remote (destnation) port used with UDP encapsulate", */
   /*          0, 65535, SCTP_DEFAULT_UDP_ENCAPS_PORT_REMOTE, */
   /*          G_PARAM_READWRITE)); */
   /* g_object_class_install_property (gobject_class, PROP_UDP_ENCAPS_PORT_LOCAL, */
   /*       g_param_spec_int ("udp-encaps-port-local",  "local UDP encapuslation port", */
   /*          "The local (source) port used with UDP encapsulate", */
   /*          0, 65535, SCTP_DEFAULT_UDP_ENCAPS_PORT_LOCAL, */
   /*          G_PARAM_READWRITE)); */
   g_object_class_install_property (gobject_class, PROP_USRSCTP_STATS,
         g_param_spec_pointer ("usrsctp-stats",  "usrsctp stats",
            "Stats (struct sctpstat *) provided by libusrsctp",
            G_PARAM_READABLE));

   g_object_class_install_property (gobject_class, PROP_CMT,
         g_param_spec_boolean ("cmt", "CMT",
            "enable Concurrent Multipath Transmission",
            SCTP_DEFAULT_CMT, G_PARAM_READWRITE));
   g_object_class_install_property (gobject_class, PROP_BS,
         g_param_spec_boolean ("buffer-split", "BS",
            "enable buffer splitting for CMT",
            SCTP_DEFAULT_BS, G_PARAM_READWRITE));
   g_object_class_install_property (gobject_class, PROP_DUPL_POLICY,
         g_param_spec_string  ("duplication-policy", "duplication policy",
            "enable how packets are duplicated onto all pahts",
            SCTP_DEFAULT_DUPL_POLICY, G_PARAM_READWRITE));

   g_object_class_install_property (gobject_class, PROP_PR,
         g_param_spec_string ("pr-policy",  "Policy partial reliabiltiy",
            "pr policy as defined in usrsctp.h",
 SCTP_DEFAULT_PR_POLICY, G_PARAM_READWRITE));
   g_object_class_install_property (gobject_class, PROP_PR_VALUE,
         g_param_spec_int ("pr-value",  "value for partial reliabiltiy",
            "pr value",
            0, 65535, SCTP_DEFAULT_PR_VALUE,
            G_PARAM_READWRITE));
   g_object_class_install_property (gobject_class, PROP_TS_OFFSET_VALUE,
         g_param_spec_uint ("timestamp-offset",  "timestamp offset for RTP timestamp measurements",
            "timestamp_offset",
            0, G_MAXUINT, 0,
            G_PARAM_READWRITE));
   g_object_class_install_property (gobject_class, PROP_DEADLINE,
         g_param_spec_uint ("deadline" , "deadline",
            "deadline in us",
            0, G_MAXUINT, SCTP_DEFAULT_DEADLINE_US,
            G_PARAM_READWRITE));
   g_object_class_install_property (gobject_class, PROP_DELAY,
         g_param_spec_uint ("delay" , "delay on the paths",
            "delay on the paths in us",
            0, G_MAXUINT, SCTP_DEFAULT_DELAY,
            G_PARAM_READWRITE));
   g_object_class_install_property (gobject_class, PROP_DELAY_PADDING,
         g_param_spec_uint ("delay-padding" , "addition padding in ms for the delay for DRP rtx dl calc",
            "padding for the delay",
            0, G_MAXUINT, SCTP_DEFAULT_DELAY_PADDING_US,
            G_PARAM_READWRITE));

   gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
         "SCTP Sink", "Sink/Network",
         "Send data over the network via SCTP",
         "Stefan Lendl <ste.lendl@gmail.com>");
}

static void
gst_sctpsink_init (GstSctpSink * sctpsink)
{
   /* GstPad *sinkpad = GST_BASE_SINK_PAD(sctpsink); */

   /* sctpsink->host = g_strdup (SCTP_DEFAULT_HOST); */
   /* sctpsink->port = SCTP_DEFAULT_PORT; */
   /* sctpsink->src_port = SCTP_DEFAULT_SRC_PORT; */

   /* sctpsink->udp_encaps = SCTP_DEFAULT_UDP_ENCAPS; */
   /* sctpsink->udp_encaps_port_local = SCTP_DEFAULT_UDP_ENCAPS_PORT_LOCAL; */
   /* sctpsink->udp_encaps_port_remote = SCTP_DEFAULT_UDP_ENCAPS_PORT_REMOTE; */

   sctpsink->unordered = SCTP_DEFAULT_UNORDED;
   sctpsink->nr_sack = SCTP_DEFAULT_NR_SACK;

   sctpsink->cmt = SCTP_DEFAULT_CMT;
   sctpsink->bs = SCTP_DEFAULT_BS;

   sctpsink->dupl_policy_string = g_strdup(SCTP_DEFAULT_DUPL_POLICY);
   sctpsink->dupl_policy = DUPL_POLICY_OFF;

   sctpsink->pr_policy = SCTP_DEFAULT_PR_POLICY;
   sctpsink->pr_value = SCTP_DEFAULT_PR_VALUE;

   sctpsink->dest_ip   = g_strdup(SCTP_DEFAULT_DEST_IP_PRIMARY);
   sctpsink->dest_port =          SCTP_DEFAULT_DEST_PORT;
   sctpsink->src_ip    = g_strdup(SCTP_DEFAULT_SRC_IP_PRIMARY);
   sctpsink->src_port  =          SCTP_DEFAULT_SRC_PORT;

   sctpsink->dest_ip_secondary   = g_strdup(SCTP_DEFAULT_DEST_IP_SECONDARY);
   /* sctpsink->dest_port_secondary =          SCTP_DEFAULT_DEST_PORT_SECONDARY; */
   sctpsink->src_ip_secondary    = g_strdup(SCTP_DEFAULT_SRC_IP_SECONDARY);
   /* sctpsink->src_port_secondary  =          SCTP_DEFAULT_SRC_PORT_SECONDARY; */

   sctpsink->systemclock = gst_system_clock_obtain();
   sctpsink->timestamp_offset = 0;
   sctpsink->deadline = SCTP_DEFAULT_DEADLINE_US;
   sctpsink->delay = SCTP_DEFAULT_DELAY;
   sctpsink->delay_padding = SCTP_DEFAULT_DELAY_PADDING_US;

   gst_usrsctp_debug_init();
}

void
gst_sctpsink_set_property (GObject * object, guint property_id,
      const GValue * value, GParamSpec * pspec)
{
   GstSctpSink *sctpsink = GST_SCTPSINK (object);

   switch (property_id) {
      /* case PROP_SRC_PORT: */
      /*    sctpsink->src_port = g_value_get_int (value); */
      /*    GST_DEBUG_OBJECT(sctpsink, "set src port: %d", sctpsink->src_port); */
      /*    break; */

      case PROP_UDP_ENCAPS:
         sctpsink->udp_encaps = g_value_get_boolean (value);
         GST_DEBUG_OBJECT(sctpsink, "set UDP encapsulation: %s", sctpsink->udp_encaps ? "TRUE" : "FALSE");
         break;
      case PROP_UDP_ENCAPS_PORT_REMOTE:
         sctpsink->udp_encaps_port_remote = g_value_get_int (value);
         GST_DEBUG_OBJECT(sctpsink, "set UDP encapsulation port: %d", sctpsink->udp_encaps_port_remote);
         break;
      case PROP_UDP_ENCAPS_PORT_LOCAL:
         sctpsink->udp_encaps_port_local = g_value_get_int (value);
         GST_DEBUG_OBJECT(sctpsink, "set UDP encapsulation src port: %d", sctpsink->udp_encaps_port_local);
         break;

      case PROP_CMT:
         sctpsink->cmt = g_value_get_boolean (value);
         GST_DEBUG_OBJECT(sctpsink, "set CMT: %s", sctpsink->cmt ? "TRUE" : "FALSE");
         break;
      case PROP_BS:
         sctpsink->bs = g_value_get_boolean (value);
         GST_DEBUG_OBJECT(sctpsink, "set Buffer Split: %s", sctpsink->bs ? "TRUE" : "FALSE");
         break;
      case PROP_DUPL_POLICY: {
         const gchar *dupl_policy;
         dupl_policy = g_value_get_string (value);
         if (0 == strncmp(dupl_policy, "off", 10))
            sctpsink->dupl_policy = DUPL_POLICY_OFF;
         else if (0 == strncmp(dupl_policy, "dupl", 10))
            sctpsink->dupl_policy = DUPL_POLICY_DUPLICATE;
         else if (0 == strncmp(dupl_policy, "dpr", 10))
            sctpsink->dupl_policy = DUPL_POLICY_DPR;
         else {
            GST_ERROR_OBJECT(sctpsink, "Wrong duplication policy");
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
         }
         g_free (sctpsink->dupl_policy_string);
         sctpsink->dupl_policy_string = g_strdup (dupl_policy);
         GST_DEBUG_OBJECT(sctpsink, "set duplication policy: %s", sctpsink->dupl_policy_string);
         break; }
      case PROP_PR_VALUE:
         sctpsink->pr_value = g_value_get_int (value);
         GST_DEBUG_OBJECT(sctpsink, "set pr value: %d", sctpsink->pr_value);
         break;
      case PROP_TS_OFFSET_VALUE:
         sctpsink->timestamp_offset = g_value_get_uint (value);
         GST_DEBUG_OBJECT(sctpsink, "set timestamp offset value: %dsek", sctpsink->timestamp_offset);
         break;
      case PROP_DEADLINE:
         sctpsink->deadline = g_value_get_uint (value);
         GST_DEBUG_OBJECT(sctpsink, "set dealine value: %dus", sctpsink->deadline);
         break;
      case PROP_DELAY:
         sctpsink->delay = g_value_get_uint (value);
         GST_DEBUG_OBJECT(sctpsink, "set delay value: %dus", sctpsink->delay);
         break;
      case PROP_DELAY_PADDING:
         sctpsink->delay_padding = g_value_get_uint (value);
         GST_DEBUG_OBJECT(sctpsink, "set delay padding value: %uus", sctpsink->delay_padding);
         break;

      case PROP_PR: {
         const gchar *pr_policy;
         pr_policy = g_value_get_string (value);
         if ((0 == strncmp(pr_policy, "none", 10))
               || (0 == strncmp(pr_policy, "none", 10)))
            sctpsink->pr_policy = SCTP_PR_SCTP_NONE;
         else if (0 == strncmp(pr_policy, "rtx", 10))
            sctpsink->pr_policy = SCTP_PR_SCTP_RTX;
         else if (0 == strncmp(pr_policy, "ttl", 10))
            sctpsink->pr_policy = SCTP_PR_SCTP_TTL;
         else if ((0 == strncmp(pr_policy, "prio", 10))
               || (0 == strncmp(pr_policy, "buf", 10)))
            sctpsink->pr_policy = SCTP_PR_SCTP_BUF;
         else {
            GST_ERROR_OBJECT(sctpsink, "Wrong pr policy");
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
         }
         GST_DEBUG_OBJECT(sctpsink, "set pr policy: %s", pr_policy);
         break; }

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

   switch (property_id) {
      /* case PROP_SRC_PORT: */
      /*    g_value_set_int (value, sctpsink->src_port); */
         /* break; */
      case PROP_UDP_ENCAPS:
         g_value_set_boolean (value, sctpsink->udp_encaps);
         break;
      case PROP_UDP_ENCAPS_PORT_REMOTE:
         g_value_set_int (value, sctpsink->udp_encaps_port_remote);
         break;
      case PROP_UDP_ENCAPS_PORT_LOCAL:
         g_value_set_int (value, sctpsink->udp_encaps_port_local);
         break;
      case PROP_USRSCTP_STATS: {
         struct sctpstat stats;
         usrsctp_get_stat(&stats);
         g_value_set_pointer (value, (gpointer *)&stats);
         break; }

      case PROP_CMT:
         g_value_set_boolean (value, sctpsink->cmt);
         break;
      case PROP_BS:
         g_value_set_boolean (value, sctpsink->bs);
         break;
      case PROP_DUPL_POLICY:
         g_value_set_string (value, sctpsink->dupl_policy_string);
         break;
      case PROP_PR:
         g_value_set_int (value, sctpsink->pr_policy);
         break;
      case PROP_PR_VALUE:
         g_value_set_int (value, sctpsink->pr_value);
         break;
      case PROP_TS_OFFSET_VALUE:
         g_value_set_uint (value, sctpsink->timestamp_offset);
         break;
      case PROP_DELAY:
         g_value_set_uint (value, sctpsink->delay);
         break;
      case PROP_DEADLINE:
         g_value_set_uint (value, sctpsink->deadline);
         break;
      case PROP_DELAY_PADDING:
         g_value_set_double (value, sctpsink->delay_padding);
         break;

      default:
         G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
         break;
   }
}

void gst_sctpsink_dispose (GObject * object) {
   GstSctpSink *sctpsink = GST_SCTPSINK (object);

   GST_DEBUG_OBJECT (sctpsink, "dispose");

   /* clean up as possible.  may be called multiple times */

   G_OBJECT_CLASS (gst_sctpsink_parent_class)->dispose (object);
}

void gst_sctpsink_finalize (GstSctpSink * sctpsink) {
   GST_DEBUG_OBJECT (sctpsink, "finalize");

   // FIXME: null-out all attributes


   g_free (sctpsink->src_ip);
   sctpsink->src_ip = NULL;
   g_free (sctpsink->dest_ip);
   sctpsink->dest_ip = NULL;

   g_free (sctpsink->src_ip_secondary);
   sctpsink->src_ip_secondary = NULL;
   g_free (sctpsink->dest_ip_secondary);
   sctpsink->dest_ip_secondary = NULL;

   g_free (sctpsink->dupl_policy_string);
   sctpsink->dupl_policy_string = NULL;

   // free all memory
   while (usrsctp_finish() != 0) {
      sleep(1);
   }

   G_OBJECT_CLASS (gst_sctpsink_parent_class)->finalize ((GObject *) sctpsink);
}

/* start or stop a pulling thread */
static gboolean gst_sctpsink_activate_pull (GstBaseSink * sink, gboolean active) {
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
      }
      free(data);
   } else {
      GST_INFO("received data NULL");
      usrsctp_close(sock);
   }
   return (1);
}


/* start and stop processing, ideal for opening/closing the resource */
static gboolean
gst_sctpsink_start (GstBaseSink * sink)
{
   GstSctpSink *sctpsink = GST_SCTPSINK (sink);

   /* struct socket *sock; */
   struct sockaddr *addrs;
   /* struct sockaddr_in6 addr6; */
   /* struct sctp_udpencaps encaps; */
   /* char buffer[80]; */
   int n;
   GString *addr_string;

   // FIXME udp encapsulation?
   /* usrsctp_init(9899, NULL, usrsctp_debug_printf); */
   usrsctp_init(sctpsink->udp_encaps ? sctpsink->udp_encaps_port_local : 0, NULL,
                usrsctp_debug_printf_sender);

#ifdef SCTP_DEBUG
   usrsctp_sysctl_set_sctp_debug_on(SCTP_USRSCTP_DEBUG);
   usrsctp_sysctl_set_sctp_logging_level(SCTP_CWND_LOGGING_ENABLE|SCTP_CWND_MONITOR_ENABLE);
         /* SCTP_LTRACE_ERROR_ENABLE|SCTP_LTRACE_CHUNK_ENABLE); */
#endif

   usrsctp_sysctl_set_sctp_blackhole(2);
   usrsctp_sysctl_set_sctp_heartbeat_interval_default(5000); // (30000ms)
   usrsctp_sysctl_set_sctp_delayed_sack_time_default(10);   // 200 mimize sack delay */
   if (sctpsink->nr_sack)
      usrsctp_sysctl_set_sctp_nrsack_enable(1);                /* non-renegable SACKs */
   usrsctp_sysctl_set_sctp_ecn_enable(0);                   /* sctp_ecn_enable > default enabled */
   usrsctp_sysctl_set_sctp_enable_sack_immediately(1);      // Enable I-Flag
   /* sctp_path_pf_threshold */ /* TODO leave potentially failed state disabled for now */

   usrsctp_sysctl_set_sctp_max_burst_default(0);
   usrsctp_sysctl_set_sctp_use_cwnd_based_maxburst(0);
   usrsctp_sysctl_set_sctp_fr_max_burst_default(0);

   usrsctp_sysctl_set_sctp_sendspace(SCTP_SNDBUF);
   usrsctp_sysctl_set_sctp_max_chunks_on_queue(8192);
   usrsctp_sysctl_set_sctp_path_pf_threshold(2);

   /* CMT Options */
   if (sctpsink->cmt)
      usrsctp_sysctl_set_sctp_cmt_on_off(1);
   if (sctpsink->bs)
      usrsctp_sysctl_set_sctp_buffer_splitting(1);

   usrsctp_sysctl_set_sctp_shutdown_guard_time_default(10);

   if ((sctpsink->sock = usrsctp_socket(AF_INET6, SOCK_STREAM, IPPROTO_SCTP,
               usrsctp_receive_cb, NULL, 0, NULL)) == NULL) {
      GST_ERROR_OBJECT(sctpsink, "usrsctp_socket");
   }

   /* if (usrsctp_set_non_blocking(sctpsink->sock, 0)) */
   /*    GST_ERROR_OBJECT(sctpsink, "usrsctp_set_non_blocking: %s", strerror(errno)); */

   /* if (usrsctp_setsockopt(sctpsink->sock, SOL_SOCKET, SO_SNDBUF,
    *          (const void *)&(int){SCTP_SNDBUF}, (socklen_t)sizeof(int)) < 0) {
    *    GST_ERROR_OBJECT(sctpsink, "usrsctp_setsockopt SO_SNDBUF");
    * } */


   GST_DEBUG_OBJECT(sctpsink, "binding client to: %s, %s port: %d",
         sctpsink->src_ip, sctpsink->src_ip_secondary, sctpsink->src_port);


   struct sockaddr_in addr4_src[2];
   memset(&addr4_src, 0, sizeof(struct sockaddr_in) * 2);
   addr4_src[0].sin_family      = addr4_src[1].sin_family = AF_INET;
   addr4_src[0].sin_port        = htons(sctpsink->src_port);

   if (inet_pton(AF_INET, sctpsink->src_ip, &addr4_src[0].sin_addr) <= 0) {
      GST_ERROR_OBJECT(sctpsink, "Illegal source address: %s", sctpsink->src_ip);
      return FALSE;
   }
   if (inet_pton(AF_INET, sctpsink->src_ip_secondary, &addr4_src[1].sin_addr) <= 0) {
      GST_ERROR_OBJECT(sctpsink, "Illegal source address: %s", sctpsink->src_ip_secondary);
      return FALSE;
   }

   /* addr4_src[0].sin_addr.s_addr        = INADDR_ANY; */
   /* if (usrsctp_bind(sctpsink->sock, (struct sockaddr *)&addr4_src[0], sizeof(struct sockaddr_in)) < 0) {
    *    GST_ERROR_OBJECT(sctpsink, "usrsctp_bind failed: %s", strerror(errno));
    *    return FALSE;
    * } */
   if (usrsctp_bindx(sctpsink->sock, (struct sockaddr *)&addr4_src, 2, SCTP_BINDX_ADD_ADDR) < 0) {
      GST_ERROR_OBJECT(sctpsink, "usrsctp_bindx failed: %s", strerror(errno));
      return FALSE;
   }

   if ((n = usrsctp_getladdrs(sctpsink->sock, 0, &addrs)) < 0) {
      GST_ERROR_OBJECT(sctpsink, "usrsctp_getladdrs %s", strerror(errno));
   } else {
      addr_string = g_string_new("");
      usrsctp_addrs_to_string(addrs, n, addr_string);
      GST_INFO_OBJECT(sctpsink, "SCTP Local addresses: %s", addr_string->str);
      g_string_free(addr_string, TRUE);
      usrsctp_freeladdrs(addrs);
   }

      /************ SCTP Options *****************/
   /* if (argc > 5) { // udp encapsulation
    *    memset(&encaps, 0, sizeof(struct sctp_udpencaps));
    *    encaps.sue_address.ss_family = AF_INET6;
    *    encaps.sue_port = htons(atoi(argv[5]));
    *    if (usrsctp_setsockopt(sctpsink->sock, IPPROTO_SCTP, SCTP_REMOTE_UDP_ENCAPS_PORT, (const void*)&encaps,
    *             (socklen_t)sizeof(struct sctp_udpencaps)) < 0) {
    *       GST_ERROR_OBJECT(sctpsink, "setsockopt");
    *    }
    * } */

   struct sctp_event event;
   uint16_t event_types[] = {/* SCTP_ASSOC_CHANGE, */
                             /* SCTP_PEER_ADDR_CHANGE, */
                             SCTP_REMOTE_ERROR,
                             SCTP_SHUTDOWN_EVENT,
                             SCTP_ADAPTATION_INDICATION,
                             SCTP_PARTIAL_DELIVERY_EVENT};
   memset(&event, 0, sizeof(event));
   event.se_assoc_id = SCTP_FUTURE_ASSOC;
   event.se_on       = 1;
   for (int i = 0; i < (unsigned int)(sizeof(event_types) / sizeof(uint16_t)); i++) {
      event.se_type = event_types[i];
      if (usrsctp_setsockopt(sctpsink->sock, IPPROTO_SCTP, SCTP_EVENT,
               &event, sizeof(struct sctp_event)) < 0) {
         GST_ERROR_OBJECT(sctpsink, "usrsctp_setsockopt SCTP_EVENT");
      }
   }

   // disable any Nagle-like algorithms
   if (usrsctp_setsockopt(sctpsink->sock, IPPROTO_SCTP, SCTP_NODELAY,
            (const void *)&(int){1}, (socklen_t)sizeof(int)) < 0) {
      GST_ERROR_OBJECT(sctpsink, "usrsctp_setsockopt SCTP_NODELAY");
   }

   /* also possible with socket option including time and frequency
      * 8.1.19.  Get or Set Delayed SACK Timer (SCTP_DELAYED_SACK)
      * https://tools.ietf.org/html/rfc6458#section-8.1.19 */

   // SCTP_MAXSEG > 0    == fragmentation is not limited.. and definde by pmtu

   struct sockaddr_in addr4_dest[2];
   memset(&addr4_dest, 0, sizeof(struct sockaddr_in) * 2);
   addr4_dest[0].sin_family      = addr4_dest[1].sin_family = AF_INET;
   addr4_dest[0].sin_port        = addr4_dest[1].sin_port = htons(sctpsink->dest_port);

   if (inet_pton(AF_INET, sctpsink->dest_ip, &addr4_dest[0].sin_addr) <= 0) {
      GST_ERROR_OBJECT(sctpsink, "Illegal destination address: %s", sctpsink->dest_ip);
      return FALSE;
   }
   if (inet_pton(AF_INET, sctpsink->dest_ip_secondary, &addr4_dest[1].sin_addr) <= 0) {
      GST_ERROR_OBJECT(sctpsink, "Illegal destination address: %s", sctpsink->dest_ip_secondary);
      return FALSE;
   }

   memset(&sctpsink->dest_addr, 0, sizeof(struct sockaddr_in));
   memset(&sctpsink->dest_addr_secondary, 0, sizeof(struct sockaddr_in));
   sctpsink->dest_addr.sin_family = sctpsink->dest_addr_secondary.sin_family = AF_INET;
   sctpsink->dest_addr.sin_port   = sctpsink->dest_addr_secondary.sin_port = htons(sctpsink->dest_port);

   if (inet_pton(AF_INET, sctpsink->dest_ip, &sctpsink->dest_addr.sin_addr) <= 0) {
      GST_ERROR_OBJECT(sctpsink, "Illegal destination address: %s", sctpsink->dest_ip);
      return FALSE;
   }
   if (inet_pton(AF_INET, sctpsink->dest_ip_secondary, &sctpsink->dest_addr_secondary.sin_addr) <= 0) {
      GST_ERROR_OBJECT(sctpsink, "Illegal destination address: %s", sctpsink->dest_ip_secondary);
      return FALSE;
   }

   GST_DEBUG_OBJECT(sctpsink, "connecting to: %s, %s port: %d",
         sctpsink->dest_ip, sctpsink->dest_ip_secondary, sctpsink->dest_port);

   if (usrsctp_connectx(sctpsink->sock, (struct sockaddr *)&addr4_dest, 2,
            (sctp_assoc_t *)&(int){ SCTP_ASSOC_ID }) < 0) {
      GST_ERROR_OBJECT(sctpsink, "usrsctp_connect failed: %s", strerror(errno));
      return FALSE;
   }


   if ((n = usrsctp_getpaddrs(sctpsink->sock, 0, &addrs)) < 0) {
      GST_ERROR_OBJECT(sctpsink, "usrsctp_getpaddrs");
   } else {
      addr_string = g_string_new("");
      usrsctp_addrs_to_string(addrs, n, addr_string);
      GST_INFO_OBJECT(sctpsink, "SCTP Peer addresses: %s", addr_string->str);
      g_string_free(addr_string, TRUE);
      usrsctp_freepaddrs(addrs);
   }

   /* socklen_t status_len = (socklen_t)sizeof(struct sctp_status);
    * struct sctp_status status;
    * int c=0;
    * while(1) {
    *    if (usrsctp_getsockopt(sctpsink->sock, IPPROTO_SCTP, SCTP_STATUS, &status, &status_len) < 0) {
    *       GST_ERROR_OBJECT(sctpsink, "getsockopt SCTP_STATUS: %s", strerror(errno));
    *    }
    *    if (status.sstat_state == SCTP_ESTABLISHED) {
    *       break;
    *    } else if (++c > 300) { // 9 sek
    *       // FIXME post a Msg on the bus!
    *       return FALSE;
    *    } else {
    *       g_usleep(30000); // 30ms
    *    }
    * } */

   struct sctp_sendv_spa spa;
   memset(&spa, 0, sizeof(struct sctp_sendv_spa));
   spa.sendv_sndinfo.snd_sid = SCTP_SID;
   spa.sendv_sndinfo.snd_flags = SCTP_UNORDERED;
   spa.sendv_sndinfo.snd_ppid = htonl(SCTP_PPID);
   spa.sendv_sndinfo.snd_assoc_id = SCTP_ASSOC_ID;
   spa.sendv_flags = SCTP_SEND_SNDINFO_VALID;

   /* this assures that both paths are connected and bound */
   usrsctp_sendv(sctpsink->sock, "a", 1, NULL, 0,
            &spa, (socklen_t)sizeof(struct sctp_sendv_spa),
            SCTP_SENDV_SPA, 0);

   if (usrsctp_set_non_blocking(sctpsink->sock, 0))
      GST_ERROR_OBJECT(sctpsink, "usrsctp_set_non_blocking: %s", strerror(errno));

   sctpsink->socket_open = TRUE;
   /* sctpsink->stats_timer_id = g_timeout_add (1000, stats_timer, sctpsink); */
   return TRUE;
}

static gboolean
gst_sctpsink_stop (GstBaseSink * sink)
{
   GstSctpSink *sctpsink = GST_SCTPSINK (sink);
   GST_DEBUG_OBJECT (sctpsink, "stop");

   struct sctp_prstatus prstat;
   socklen_t optlen = sizeof(struct sctp_prstatus);
   memset(&prstat, 0, sizeof(struct sctp_prstatus));
   prstat.sprstat_assoc_id = SCTP_CURRENT_ASSOC; // ignored for one-to-one
   prstat.sprstat_policy = SCTP_PR_SCTP_ALL;

   if (usrsctp_getsockopt(sctpsink->sock, IPPROTO_SCTP, SCTP_PR_ASSOC_STATUS,
            (void *)&prstat, (socklen_t *)&optlen) < 0) {
      GST_ERROR_OBJECT(sctpsink, "usrsctp_getsockopt SCTP_PR_ASSOC_STATUS: %s", strerror(errno));
   } else {
      GST_INFO_OBJECT(sctpsink, "Abandoned already sent Chunks\t\t%lu", prstat.sprstat_abandoned_sent);
      GST_INFO_OBJECT(sctpsink, "Abandoned not yet sent Chunks\t\t%lu", prstat.sprstat_abandoned_unsent);
   }

   if (usrsctp_shutdown(sctpsink->sock, SHUT_RDWR) < 0)
      GST_ERROR_OBJECT(sctpsink, "usrsctp_shutdown: %s", strerror(errno));
   usrsctp_close(sctpsink->sock);

   sctpsink->socket_open = FALSE;

   struct sctpstat stat;
   usrsctp_get_stat(&stat);
   GST_INFO_OBJECT(sctpsink, "Number of packets sent:\t\t\t%u",                       stat.sctps_outpackets);
   GST_INFO_OBJECT(sctpsink, "Number of packets received:\t\t%u",                     stat.sctps_inpackets);
   GST_INFO_OBJECT(sctpsink, "total output DATA chunks\t\t\t%u",                      stat.sctps_senddata);
   GST_INFO_OBJECT(sctpsink, "total output retransmitted DATA chunks\t%u\t(%4.1f%%)", stat.sctps_sendretransdata,
         (double)stat.sctps_sendretransdata/stat.sctps_senddata * 100);

   GST_INFO_OBJECT(sctpsink, "total output fast retran DATA chunks\t%u\t(%4.1f%%)",   stat.sctps_sendfastretrans,
         (double)stat.sctps_sendfastretrans/stat.sctps_senddata * 100);
   GST_INFO_OBJECT(sctpsink, "total FR's that happened more than once\t%u",           stat.sctps_sendmultfastretrans);
   GST_INFO_OBJECT(sctpsink, "Chunks marked for retransmission,\t%u",                 stat.sctps_fastretransinrtt);
   GST_INFO_OBJECT(sctpsink, "number of multiple FR in a RTT window\t%u",             stat.sctps_markedretrans);
   GST_INFO_OBJECT(sctpsink, "Number of early FR timers that fired\t%u",              stat.sctps_timoearlyfr);

   GST_INFO_OBJECT(sctpsink, "output ordered chunks\t\t\t%u",                         stat.sctps_outorderchunks);
   GST_INFO_OBJECT(sctpsink, "output unordered chunks\t\t\t%u",                       stat.sctps_outunorderchunks);
   GST_INFO_OBJECT(sctpsink, "output control chunks\t\t\t%u",                         stat.sctps_outcontrolchunks);
   GST_INFO_OBJECT(sctpsink, "out of the blue\t\t\t\t%u",                             stat.sctps_outoftheblue);

   GST_INFO_OBJECT(sctpsink, "input control chunks\t\t\t%u",                          stat.sctps_incontrolchunks);
   GST_INFO_OBJECT(sctpsink, "input ordered chunks\t\t\t%u",                          stat.sctps_inorderchunks);
   GST_INFO_OBJECT(sctpsink, "input unordered chunks\t\t\t%u",                        stat.sctps_inunorderchunks);

   GST_INFO_OBJECT(sctpsink, "total output SACKs\t\t\t%u",                            stat.sctps_sendsacks);
   GST_INFO_OBJECT(sctpsink, "total input SACKs\t\t\t%u",                             stat.sctps_recvsacks);

   GST_INFO_OBJECT(sctpsink, "ip_output error counter\t\t\t%u",                       stat.sctps_senderrors);

   GST_INFO_OBJECT(sctpsink, "Packet drop from middle box\t\t%u",                     stat.sctps_pdrpfmbox);
   GST_INFO_OBJECT(sctpsink, "P-drop from end host\t\t\t%u",                          stat.sctps_pdrpfehos);
   GST_INFO_OBJECT(sctpsink, "P-drops with data\t\t\t%u",                             stat.sctps_pdrpmbda);
   GST_INFO_OBJECT(sctpsink, "data drops due to chunk limit reached\t%u",             stat.sctps_datadropchklmt);
   GST_INFO_OBJECT(sctpsink, "data drops due to rwnd limit reached\t%u",              stat.sctps_datadroprwnd);

   GST_INFO_OBJECT(sctpsink, "sender ran dry of user data on primary\t%u",            stat.sctps_primary_randry);

   GST_INFO_OBJECT(sctpsink, "max burst doesn't allow sending\t\t%u",                 stat.sctps_maxburstqueued);
   GST_INFO_OBJECT(sctpsink, "nagle allowed sending\t\t\t%u",                         stat.sctps_naglesent);
   GST_INFO_OBJECT(sctpsink, "nagle doesn't allow sending\t\t%u\n",                   stat.sctps_naglequeued);

   GST_INFO_OBJECT(sctpsink, "DPR Timer fired\t\t\t\t%u",                             stat.sctps_timodpr);
   GST_INFO_OBJECT(sctpsink, "DPR Timer avg handle after deadline\t%.1fms",           (gdouble)stat.sctps_dpr_avg_delay_timer / 1000);
   GST_INFO_OBJECT(sctpsink, "DPR Chunks flagged for retran\t\t%u",                   stat.sctps_dpr_flagged);
   /* GST_INFO_OBJECT(sctpsink, "DPR Chunks retran before DL\t%u",             stat.sctps_dpr_pre_deadline); */

   FILE *usrsctp_stats_sender = g_fopen ("/tmp/gst-sctp-results/usrsctp_stats_sender", "w+");
   if (usrsctp_stats_sender == NULL) {
      GST_ERROR_OBJECT (sctpsink, "could not open file for writing: %s", strerror (errno));
   }

   g_fprintf (usrsctp_stats_sender, "sent_packets=%u\n",        stat.sctps_outpackets);
   g_fprintf (usrsctp_stats_sender, "recv_packets=%u\n",        stat.sctps_inpackets);
   g_fprintf (usrsctp_stats_sender, "send_data=%u\n",           stat.sctps_senddata);
   g_fprintf (usrsctp_stats_sender, "send_retrans_data=%u\n",    stat.sctps_sendretransdata);
   g_fprintf (usrsctp_stats_sender, "send_fast_retrans=%u\n",   stat.sctps_sendfastretrans);
   g_fprintf (usrsctp_stats_sender, "send_sacks=%u\n",          stat.sctps_sendsacks);
   g_fprintf (usrsctp_stats_sender, "recv_sacks=%u\n",          stat.sctps_recvsacks);
   g_fprintf (usrsctp_stats_sender, "timer_dpr_fired=%u\n",     stat.sctps_timodpr);
   g_fprintf (usrsctp_stats_sender, "dpr_avg_delay_timer=%u\n", stat.sctps_dpr_avg_delay_timer);
   g_fprintf (usrsctp_stats_sender, "dpr_flagged=%u\n",         stat.sctps_dpr_flagged);
   g_fprintf (usrsctp_stats_sender, "abandoned_sent=%lu\n",     prstat.sprstat_abandoned_sent);
   g_fprintf (usrsctp_stats_sender, "abandoned_unsent=%lu\n",   prstat.sprstat_abandoned_unsent);

   fclose(usrsctp_stats_sender);

   // free all memory
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

static GstFlowReturn gst_sctpsink_render (GstBaseSink * sink, GstBuffer * buffer) {
   return usrsctp_render_send (GST_SCTPSINK(sink), buffer, FALSE);
}

static GstFlowReturn usrsctp_render_send (GstSctpSink *sctpsink, GstBuffer *buffer, gboolean last) {
   GstMapInfo map;
   gst_buffer_map (buffer, &map, GST_MAP_READ);
   GST_TRACE_OBJECT(sctpsink, "got a buffer of size:%4lu", gst_buffer_get_size(buffer));

   /* gst_buffer_foreach_meta (buffer, buffer_meta_for_each, buffer); */

   struct sctp_sendv_spa spa;
   memset(&spa, 0, sizeof(struct sctp_sendv_spa));
   /* struct sctp_sndinfo snd_info; */

   spa.sendv_sndinfo.snd_sid = SCTP_SID;
   if (sctpsink->unordered)
      spa.sendv_sndinfo.snd_flags = SCTP_UNORDERED;
   spa.sendv_sndinfo.snd_ppid = htonl(SCTP_PPID);
   spa.sendv_sndinfo.snd_assoc_id = SCTP_ASSOC_ID;
   spa.sendv_flags = SCTP_SEND_SNDINFO_VALID;

   if ((sctpsink->pr_policy == SCTP_PR_SCTP_TTL) ||
       (sctpsink->pr_policy == SCTP_PR_SCTP_RTX)) {
      spa.sendv_prinfo.pr_policy = sctpsink->pr_policy;
      spa.sendv_prinfo.pr_value = sctpsink->pr_value;
      spa.sendv_flags |= SCTP_SEND_PRINFO_VALID;
   }

   if (sctpsink->dupl_policy == DUPL_POLICY_DPR) {
      calc_retran_time(sctpsink, map.data, &spa.sendv_dprinfo.deadline);
      spa.sendv_dprinfo.dpr_enabled = 1;
   }

   if (last) {
      spa.sendv_sndinfo.snd_flags |= SCTP_SACK_IMMEDIATELY;
   }

   if (usrsctp_sendv(sctpsink->sock,
            map.data, gst_buffer_get_size(buffer),
            NULL, 0,
            /* (struct sockaddr *)&sctpsink->dest_addr, 1, */
            &spa, (socklen_t)sizeof(struct sctp_sendv_spa),
            SCTP_SENDV_SPA, 0) < 0) {
      GST_ERROR_OBJECT(sctpsink, "usrsctp_sendv failed: %s", strerror(errno));
      /* gst_sctpsink_stop((GstBaseSink *)sctpsink); */
      // TODO sometimes the resource is unavailable
      // > buffer full GST_FLOW_WAIT?
   }

   if (sctpsink->dupl_policy == DUPL_POLICY_DUPLICATE) {
      spa.sendv_sndinfo.snd_flags |= SCTP_ADDR_OVER;
      if (usrsctp_sendv(sctpsink->sock,
               map.data, gst_buffer_get_size(buffer),
               (struct sockaddr *)&sctpsink->dest_addr_secondary, 1,
               &spa, (socklen_t)sizeof(struct sctp_sendv_spa),
               SCTP_SENDV_SPA, 0) < 0) {
         GST_ERROR_OBJECT(sctpsink, "usrsctp_sendv on secondary failed: %s", strerror(errno));
      }
   }

   print_rtp_header((GstElement *)sctpsink, map.data);
   gst_buffer_unmap (buffer, &map);

   return GST_FLOW_OK;
}

static void calc_retran_time(GstSctpSink * sctpsink, guint8 *data, struct timeval *rtx_deadline) {
      RTPHeader *rtph = (RTPHeader *)data;
      guint32 rtp_timestamp = ntohl(rtph->TS);

      GstClockTime rtp_timestamp_unscaled = gst_util_uint64_scale_int ((guint64)rtp_timestamp,
            GST_SECOND, (gint)90000); // ns
      rtp_timestamp_unscaled += (gint64)sctpsink->timestamp_offset * GST_SECOND + // ns
         (sctpsink->deadline - sctpsink->delay - sctpsink->delay_padding) /*us*/ * 1000; // ns

      rtx_deadline->tv_sec =  (long int) rtp_timestamp_unscaled / 1000000000;
      rtx_deadline->tv_usec = (long int)(rtp_timestamp_unscaled % 1000000000) / 1000;

      struct timeval now;
      gettimeofday(&now, NULL);
      GST_TRACE_OBJECT(sctpsink, "seqnr: %" PRIu16 " now: [ %ld.%06ld ] deadline: %lu diff: %ldms, [ %ld.%06ld ]",
            ntohs(rtph->seq_num), now.tv_sec, now.tv_usec, rtp_timestamp_unscaled,
            (rtp_timestamp_unscaled - (now.tv_sec * GST_SECOND + now.tv_usec * 1000)) / GST_MSECOND,
            rtx_deadline->tv_sec, rtx_deadline->tv_usec);
   return;

}

struct buffer_list_pass {
   GstSctpSink *sink;
   GstBufferList *list;
};

/** Render a BufferList */
static GstFlowReturn gst_sctpsink_render_list (GstBaseSink *sink, GstBufferList *buffer_list) {
   /* GstSctpSink *sctpsink = ; */

   struct buffer_list_pass pass_to_list = {
      .sink = GST_SCTPSINK (sink),
      .list = buffer_list
   };

   gst_buffer_list_foreach (buffer_list, sctpsink_render_list_iter, (gpointer)&pass_to_list);

   return GST_FLOW_OK;
}


static gboolean sctpsink_render_list_iter (GstBuffer **buffer, guint idx, gpointer user_data) {
   struct buffer_list_pass *passed;
   passed = (struct buffer_list_pass *)user_data;
   GstSctpSink *sctpsink = passed->sink;

   // The last one gets a true > sending the I-Flag
   usrsctp_render_send (GST_SCTPSINK(sctpsink), *buffer,
         idx == gst_buffer_list_length(passed->list) - 1 ? TRUE : FALSE);

   return TRUE;
}

/* static gboolean stats_timer (gpointer priv) {
 *    GstSctpSink *sctpsink = (GstSctpSink *)priv;
 *
 *    if ( ! sctpsink->socket_open) {
 *       return FALSE;
 *    }
 *
 *    socklen_t len;
 *    struct sctp_status status;
 *    len = (socklen_t)sizeof(struct sctp_status);
 *    if (usrsctp_getsockopt(sctpsink->sock,
 *             IPPROTO_SCTP,
 *             SCTP_STATUS,
 *             &status,
 *             &len) < 0) {
 *       GST_ERROR_OBJECT(sctpsink, "getsockopt SCTP_STATUS: %s", strerror(errno));
 *       return FALSE;
 *    }
 *
 *    GST_INFO_OBJECT(sctpsink, "state %d, rwnd: %u, unack: %u, pend: %u, fragm: %u",
 *          status.sstat_state,
 *          status.sstat_rwnd,
 *          status.sstat_unackdata,
 *          status.sstat_penddata,
 *          status.sstat_fragmentation_point );
 *
 *    char buffer[INET6_ADDRSTRLEN];
 *    len = (socklen_t)sizeof(status.sstat_primary.spinfo_address);
 *    if (getnameinfo((struct sockaddr*)&status.sstat_primary.spinfo_address, len, buffer,
 *             sizeof(buffer), 0, 0, NI_NUMERICHOST) < 0) {
 *       GST_ERROR_OBJECT(sctpsink, "failed to fetch remote address (errno=%d)",errno);
 *       return FALSE;
 *    }
 *    GST_INFO_OBJECT(sctpsink, "Primary: %s state %d, cwnd: %u, srtt: %ums, rto: %ums, mtu: %u",
 *          buffer,
 *          status.sstat_primary.spinfo_state,
 *          status.sstat_primary.spinfo_cwnd,
 *          status.sstat_primary.spinfo_srtt,
 *          status.sstat_primary.spinfo_rto,
 *          status.sstat_primary.spinfo_mtu );
 *
 *
 * [> 8.1.2.  Association Parameters (SCTP_ASSOCINFO) <]
 *    // SO_SNDBUF
 *
 *    return TRUE;
 * } */

// vim: ft=c

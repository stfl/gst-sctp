/* _GstRtpSctpSender
 * Copyright (C) 2017 Stefan Lendl <ste.lendl@gmail.com>
 * Copyright (C) 2010 Entropy Wave Inc
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <gst/gst.h>
#include <stdlib.h>
#include <glib.h>
#include <sys/time.h>

#include <usrsctp.h>
#include <string.h>

#define GETTEXT_PACKAGE "RtpSctpSender"

GST_DEBUG_CATEGORY_STATIC (rtpsctpsend);
#define GST_CAT_DEFAULT rtpsctpsend

#define GST_FRAME_COUNT_DEFAULT "10"

typedef struct __GstRtpSctpSender _GstRtpSctpSender;
struct __GstRtpSctpSender
{
   GstElement *pipeline;
   GstBus *bus;
   GMainLoop *main_loop;

   GstElement *source_element;
   GstElement *sink_element;
   GstElement *sctpsink;
   GstElement *queue;

   gboolean paused_for_buffering;
   guint timer_id;
};

_GstRtpSctpSender *gst_RtpSctpSender_new (void);
void gst_RtpSctpSender_free (_GstRtpSctpSender *RtpSctpSender);
void gst_RtpSctpSender_create_pipeline (_GstRtpSctpSender *RtpSctpSender);
void gst_RtpSctpSender_start (_GstRtpSctpSender *RtpSctpSender);
void gst_RtpSctpSender_stop (_GstRtpSctpSender *RtpSctpSender);

static gboolean gst_RtpSctpSender_handle_message (GstBus *bus,
      GstMessage *message, gpointer data);
/* static gboolean stats_timer (gpointer priv); */


enum PipelineVariant {
   PIPELINE_UDP,           // 0
   PIPELINE_SINGLE,        // 1
   PIPELINE_CMT,           // 2
   PIPELINE_CMT_DUPL,       // 3
   PIPELINE_CMT_DPR,       // 4
   PIPELINE_UDP_DUPL,      // 5
};

gboolean verbose;
gchar *variant_string = "single"; // set a default
enum PipelineVariant variant;
gchar *num_buffers = GST_FRAME_COUNT_DEFAULT;
gchar *timestamp_offset = NULL;
gchar *deadline = NULL;
gchar *path_delay = NULL;
gchar *delay_padding = NULL;

static GOptionEntry entries[] = {
   {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL},
   {"num-buffers", 'n', 0, G_OPTION_ARG_STRING, &num_buffers, "frame count", NULL},
   {"variant", 'V', 0, G_OPTION_ARG_STRING, &variant_string,
      "define the Variant for the experiment to use", " udp|single|cmt|dupl|dpr|udpdupl " },
   {"timestamp-offset", 'T', 0, G_OPTION_ARG_STRING, &timestamp_offset,
      "timestamp offset to use for systemtime RTP timestamp", NULL},
   {"deadline", 'D', 0, G_OPTION_ARG_STRING, &deadline, "", NULL},
   {"path-delay", 'P', 0, G_OPTION_ARG_STRING, &path_delay, "", NULL},
   {"delay-padding", 'p', 0, G_OPTION_ARG_STRING, &delay_padding, "", NULL},
   {NULL}
};

int
main (int argc, char *argv[])
{
   GError *error = NULL;
   GOptionContext *context;
   _GstRtpSctpSender *RtpSctpSender;
   GMainLoop *main_loop;

   context = g_option_context_new ("- RTP SCTP Sender");
   g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
   g_option_context_add_group (context, gst_init_get_option_group ());
   if (!g_option_context_parse (context, &argc, &argv, &error)) {
      g_print ("option parsing failed: %s\n", error->message);
      g_clear_error (&error);
      exit (1);
   }

   if (timestamp_offset == NULL || deadline == NULL) {
      g_print ("timestamp-offset and deadline requiered\n");
      exit(1);
   }

   if (0 == strncmp(variant_string, "udpdupl", 10)){
      variant = PIPELINE_UDP_DUPL;
   } else if (0 == strncmp(variant_string, "udp", 10)){
      variant = PIPELINE_UDP;
   } else if (0 == strncmp(variant_string, "single", 10)){
      variant = PIPELINE_SINGLE;
   } else if (0 == strncmp(variant_string, "cmt", 10)){
      variant = PIPELINE_CMT;
   } else if (0 == strncmp(variant_string, "dupl", 10)){
      variant = PIPELINE_CMT_DUPL;
   } else if (0 == strncmp(variant_string, "dpr", 10)){
      variant = PIPELINE_CMT_DPR;
   } else {
      g_error("unknown variant: %s\n", variant_string);
   }

   g_option_context_free (context);

   GST_DEBUG_CATEGORY_INIT (rtpsctpsend, "rtpsctpsend",
         GST_DEBUG_BG_GREEN | GST_DEBUG_FG_WHITE | GST_DEBUG_BOLD,
         "The RTP over SCTP Sender Pipeline");

   RtpSctpSender = gst_RtpSctpSender_new ();
   gst_RtpSctpSender_create_pipeline (RtpSctpSender);

   gst_RtpSctpSender_start (RtpSctpSender);

   main_loop = g_main_loop_new (NULL, TRUE);
   RtpSctpSender->main_loop = main_loop;

   g_main_loop_run (main_loop);

   exit (0);
}


_GstRtpSctpSender* gst_RtpSctpSender_new (void)
{
   _GstRtpSctpSender *RtpSctpSender;

   RtpSctpSender = g_new0 (_GstRtpSctpSender, 1);

   return RtpSctpSender;
}

void
gst_RtpSctpSender_free (_GstRtpSctpSender * RtpSctpSender)
{
   if (RtpSctpSender->source_element) {
      gst_object_unref (RtpSctpSender->source_element);
      RtpSctpSender->source_element = NULL;
   }
   if (RtpSctpSender->sink_element) {
      gst_object_unref (RtpSctpSender->sink_element);
      RtpSctpSender->sink_element = NULL;
   }
   if (RtpSctpSender->sctpsink) {
      gst_object_unref (RtpSctpSender->sctpsink);
      RtpSctpSender->sctpsink = NULL;
   }
   /* if (RtpSctpSender->queue) { */
   /*    gst_object_unref (RtpSctpSender->queue); */
   /*    RtpSctpSender->queue = NULL; */
   /* } */

   if (RtpSctpSender->pipeline) {
      gst_element_set_state (RtpSctpSender->pipeline, GST_STATE_NULL);
      gst_object_unref (RtpSctpSender->pipeline);
      RtpSctpSender->pipeline = NULL;
   }
   g_free (RtpSctpSender);
}

void
gst_RtpSctpSender_create_pipeline (_GstRtpSctpSender * RtpSctpSender)
{
   /* create pipeline */
   GstElement *pipeline = gst_pipeline_new ("pipeline");

   /* create element */
   GstElement *source = gst_element_factory_make("videotestsrc", "source");
   gst_util_set_object_arg(G_OBJECT(source), "is-live", "true");
   gst_util_set_object_arg(G_OBJECT(source), "do-timestamp", "true");
   gst_util_set_object_arg(G_OBJECT(source), "pattern", "gradient");
   gst_util_set_object_arg(G_OBJECT(source), "num-buffers", num_buffers);

   GstCaps *src_caps = gst_caps_new_simple ("video/x-raw",
         "format",     G_TYPE_STRING,      "RGBA",
         "width",      G_TYPE_INT,         60,
         "height",     G_TYPE_INT,         60,
         "framerate",  GST_TYPE_FRACTION,  24,  1,
         NULL);

   GstElement *timeoverlay = gst_element_factory_make("timeoverlay", "timeoverlay");
   gst_util_set_object_arg(G_OBJECT(timeoverlay), "halignment", "right");
   gst_util_set_object_arg(G_OBJECT(timeoverlay), "valignment", "top");
   gst_util_set_object_arg(G_OBJECT(timeoverlay), "font-desc",  "Sans, 55");
   gst_util_set_object_arg(G_OBJECT(timeoverlay), "time-mode",  "stream-time");

   GstElement *rtppay = gst_element_factory_make("rtpvrawpay", "rtppay");
   gst_util_set_object_arg(G_OBJECT(rtppay), "mtu", "1400");
   gst_util_set_object_arg(G_OBJECT(rtppay), "perfect-rtptime",  "true");
   gst_util_set_object_arg(G_OBJECT(rtppay), "chunks-per-frame",  "1");
   gst_util_set_object_arg(G_OBJECT(rtppay), "seqnum-offset",  "0");
   gst_util_set_object_arg(G_OBJECT(rtppay), "timestamp-offset", timestamp_offset);

   /* GstElement *queue = gst_element_factory_make("queue2", "queue"); */

   GstElement *sink;
   if (variant == PIPELINE_UDP ) {
      sink = gst_element_factory_make("udpsink", "sink");
      g_object_set(sink,
            "host",  "192.168.0.1",
            "port",  55555,
            "bind-address",  "192.168.0.2",
            NULL);

   } else if (variant == PIPELINE_UDP_DUPL) {
      sink = gst_element_factory_make("udpsink", "sink");
      g_object_set(sink,
            "clients", "192.168.0.1:51111,128.131.89.238:52222",
            NULL);

   } else { // SCTP variants
      sink = gst_element_factory_make("sctpsink", "sink");

      gst_util_set_object_arg(G_OBJECT(sink), "udp-encaps",  "false");

      gst_util_set_object_arg(G_OBJECT(sink), "timestamp-offset", timestamp_offset);
      gst_util_set_object_arg(G_OBJECT(sink), "delay", path_delay);
      gst_util_set_object_arg(G_OBJECT(sink), "delay-padding", delay_padding);
      gst_util_set_object_arg(G_OBJECT(sink), "deadline", deadline);

      if (variant == PIPELINE_CMT || variant == PIPELINE_CMT_DPR){
         gst_util_set_object_arg(G_OBJECT(sink), "cmt",  "true");
         gst_util_set_object_arg(G_OBJECT(sink), "buffer-split",  "true");
      }

      GString *deadline_pr_value = g_string_new("");
      g_string_printf(deadline_pr_value, "%u", (uint32_t) ((atoi(deadline) - atoi(path_delay)) / 1000));

      if (variant == PIPELINE_CMT_DUPL) {
         gst_util_set_object_arg(G_OBJECT(sink), "pr_policy", "rtx");
         gst_util_set_object_arg(G_OBJECT(sink), "pr_value", "0");
         // gst_util_set_object_arg(G_OBJECT(sink), "pr_policy", "ttl");
         // gst_util_set_object_arg(G_OBJECT(sink), "pr_value", deadline_pr_value->str);
      } else if (variant == PIPELINE_CMT_DPR) {
         gst_util_set_object_arg(G_OBJECT(sink), "pr_policy", "ttl");
         gst_util_set_object_arg(G_OBJECT(sink), "pr_value", deadline_pr_value->str);
         /* gst_util_set_object_arg(G_OBJECT(sink), "pr_policy", "rtx"); */
         /* gst_util_set_object_arg(G_OBJECT(sink), "pr_value", "1"); */
      } else {
         gst_util_set_object_arg(G_OBJECT(sink), "pr_policy", "ttl");
         gst_util_set_object_arg(G_OBJECT(sink), "pr_value", deadline_pr_value->str);
         // FIXME make pr_value a variable here
         // make it an attribute of sctpsink
      }
      g_string_free(deadline_pr_value, TRUE);

      if (variant == PIPELINE_CMT_DPR) {
         gst_util_set_object_arg(G_OBJECT(sink), "duplication-policy",  "dpr");
      } else if (variant == PIPELINE_CMT_DUPL) {
         gst_util_set_object_arg(G_OBJECT(sink), "duplication-policy",  "dupl");
      } else {
         gst_util_set_object_arg(G_OBJECT(sink), "duplication-policy",  "off");
      }
   }

   /* add to pipeline */
   gst_bin_add_many(GST_BIN(pipeline), source, timeoverlay, rtppay, sink, NULL);

   /* link */
   if (!gst_element_link_filtered(source, timeoverlay, src_caps)) {
      g_warning ("Failed to link filterd source and enocder!\n");
   }
   gst_caps_unref(src_caps);

   /* gst_element_link_many(timeoverlay, rtppay, queue, sink, NULL); */

   if (!gst_element_link(timeoverlay, rtppay)) {
      g_warning ("Failed to link timeoverlay, rrtppay!\n");
   }

   if (!gst_element_link(rtppay, sink)) {
      g_critical ("Failed to link rtppay, queue'\n");
   }


   /* if (!gst_element_link(queue, sink)) { */
   /*    g_critical ("Failed to link queue, sink'\n"); */
   /* } */

   RtpSctpSender->pipeline = pipeline;

   gst_pipeline_set_auto_flush_bus (GST_PIPELINE (pipeline), FALSE);
   RtpSctpSender->bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
   gst_bus_add_watch (RtpSctpSender->bus, gst_RtpSctpSender_handle_message,
         RtpSctpSender);

   RtpSctpSender->source_element = gst_bin_get_by_name (GST_BIN (pipeline), "source");
   RtpSctpSender->sink_element = gst_bin_get_by_name (GST_BIN (pipeline), "sink");

   RtpSctpSender->sctpsink = sink;
   /* RtpSctpSender->queue = queue; */
}

void
gst_RtpSctpSender_start (_GstRtpSctpSender * RtpSctpSender)
{
   gst_element_set_state (RtpSctpSender->pipeline, GST_STATE_READY);

}

void
gst_RtpSctpSender_stop (_GstRtpSctpSender * RtpSctpSender)
{
   gst_element_set_state (RtpSctpSender->pipeline, GST_STATE_NULL);

   g_source_remove (RtpSctpSender->timer_id);
}

static void
gst_RtpSctpSender_handle_eos (_GstRtpSctpSender * RtpSctpSender)
{
   gst_RtpSctpSender_stop (RtpSctpSender);
}

static void
gst_RtpSctpSender_handle_error (_GstRtpSctpSender * RtpSctpSender,
      GError * error, const char *debug)
{
   g_print ("error: %s\n", error->message);
   gst_RtpSctpSender_stop (RtpSctpSender);
}

static void
gst_RtpSctpSender_handle_warning (_GstRtpSctpSender * RtpSctpSender,
      GError * error, const char *debug)
{
   g_print ("warning: %s\n", error->message);
}

static void
gst_RtpSctpSender_handle_info (_GstRtpSctpSender * RtpSctpSender,
      GError * error, const char *debug)
{
   g_print ("info: %s\n", error->message);
}

static void
gst_RtpSctpSender_handle_null_to_ready (_GstRtpSctpSender *
      RtpSctpSender)
{
   gst_element_set_state (RtpSctpSender->pipeline, GST_STATE_PAUSED);

}

static void
gst_RtpSctpSender_handle_ready_to_paused (_GstRtpSctpSender *
      RtpSctpSender)
{

   if (!RtpSctpSender->paused_for_buffering) {
      gst_element_set_state (RtpSctpSender->pipeline, GST_STATE_PLAYING);
   }
}

static void
gst_RtpSctpSender_handle_paused_to_playing (_GstRtpSctpSender *
      RtpSctpSender)
{
   /* RtpSctpSender->timer_id = g_timeout_add (1000, stats_timer, RtpSctpSender); */
   GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(RtpSctpSender->pipeline),
         GST_DEBUG_GRAPH_SHOW_ALL,
         "sender");
}

static void
gst_RtpSctpSender_handle_playing_to_paused (_GstRtpSctpSender *
      RtpSctpSender)
{

}

static void
gst_RtpSctpSender_handle_paused_to_ready (_GstRtpSctpSender *
      RtpSctpSender)
{

}

static void
gst_RtpSctpSender_handle_ready_to_null (_GstRtpSctpSender *
      RtpSctpSender)
{
   g_main_loop_quit (RtpSctpSender->main_loop);

}


static gboolean
gst_RtpSctpSender_handle_message (GstBus * bus, GstMessage * message,
      gpointer data)
{
   _GstRtpSctpSender *RtpSctpSender = (_GstRtpSctpSender *) data;

   switch (GST_MESSAGE_TYPE (message)) {
      case GST_MESSAGE_EOS:
         gst_RtpSctpSender_handle_eos (RtpSctpSender);
         break;
      case GST_MESSAGE_ERROR:
         {
            GError *error = NULL;
            gchar *debug;

            gst_message_parse_error (message, &error, &debug);
            gst_RtpSctpSender_handle_error (RtpSctpSender, error, debug);
         }
         break;
      case GST_MESSAGE_WARNING:
         {
            GError *error = NULL;
            gchar *debug;

            gst_message_parse_warning (message, &error, &debug);
            gst_RtpSctpSender_handle_warning (RtpSctpSender, error, debug);
         }
         break;
      case GST_MESSAGE_INFO:
         {
            GError *error = NULL;
            gchar *debug;

            gst_message_parse_info (message, &error, &debug);
            gst_RtpSctpSender_handle_info (RtpSctpSender, error, debug);
         }
         break;
      case GST_MESSAGE_TAG:
         {
            GstTagList *tag_list;

            gst_message_parse_tag (message, &tag_list);
            if (verbose)
               g_print ("tag\n");
         }
         break;
      case GST_MESSAGE_STATE_CHANGED:
         {
            GstState oldstate, newstate, pending;

            gst_message_parse_state_changed (message, &oldstate, &newstate, &pending);
            if (GST_ELEMENT (message->src) == RtpSctpSender->pipeline) {
               if (verbose)
                  g_print ("state change from %s to %s\n",
                        gst_element_state_get_name (oldstate),
                        gst_element_state_get_name (newstate));
               switch (GST_STATE_TRANSITION (oldstate, newstate)) {
                  case GST_STATE_CHANGE_NULL_TO_READY:
                     gst_RtpSctpSender_handle_null_to_ready (RtpSctpSender);
                     break;
                  case GST_STATE_CHANGE_READY_TO_PAUSED:
                     gst_RtpSctpSender_handle_ready_to_paused (RtpSctpSender);
                     break;
                  case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
                     gst_RtpSctpSender_handle_paused_to_playing (RtpSctpSender);
                     break;
                  case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
                     gst_RtpSctpSender_handle_playing_to_paused (RtpSctpSender);
                     break;
                  case GST_STATE_CHANGE_PAUSED_TO_READY:
                     gst_RtpSctpSender_handle_paused_to_ready (RtpSctpSender);
                     break;
                  case GST_STATE_CHANGE_READY_TO_NULL:
                     gst_RtpSctpSender_handle_ready_to_null (RtpSctpSender);
                     break;
                  default:
                     if (verbose)
                        g_print ("unknown state change from %s to %s\n",
                              gst_element_state_get_name (oldstate),
                              gst_element_state_get_name (newstate));
               }
            }
         }
         break;
      case GST_MESSAGE_BUFFERING:
         {
            int percent;
            gst_message_parse_buffering (message, &percent);
            //g_print("buffering %d\n", percent);
            if (!RtpSctpSender->paused_for_buffering && percent < 100) {
               g_print ("pausing for buffing\n");
               RtpSctpSender->paused_for_buffering = TRUE;
               gst_element_set_state (RtpSctpSender->pipeline, GST_STATE_PAUSED);
            } else if (RtpSctpSender->paused_for_buffering && percent == 100) {
               g_print ("unpausing for buffing\n");
               RtpSctpSender->paused_for_buffering = FALSE;
               gst_element_set_state (RtpSctpSender->pipeline, GST_STATE_PLAYING);
            }
         }
         break;
      case GST_MESSAGE_STATE_DIRTY:
      case GST_MESSAGE_CLOCK_PROVIDE:
      case GST_MESSAGE_CLOCK_LOST:
      case GST_MESSAGE_NEW_CLOCK:
      case GST_MESSAGE_STRUCTURE_CHANGE:
      case GST_MESSAGE_STREAM_STATUS:
         break;
      case GST_MESSAGE_STEP_DONE:
      case GST_MESSAGE_APPLICATION:
      case GST_MESSAGE_ELEMENT:
      case GST_MESSAGE_SEGMENT_START:
      case GST_MESSAGE_SEGMENT_DONE:
      case GST_MESSAGE_DURATION:
      case GST_MESSAGE_LATENCY:
      case GST_MESSAGE_ASYNC_START:
      case GST_MESSAGE_ASYNC_DONE:
      case GST_MESSAGE_REQUEST_STATE:
      case GST_MESSAGE_STEP_START:
      case GST_MESSAGE_QOS:
      default:
         if (verbose) {
            g_print ("message: %s\n", GST_MESSAGE_TYPE_NAME (message));
         }
         break;
   }

   return TRUE;
}



/* static gboolean stats_timer (gpointer priv)
 * {
 *    _GstRtpSctpSender *RtpSctpSender = (_GstRtpSctpSender *)priv;
 *    [> GstElement *sctpsink = gst_bin_get_by_name (GST_BIN (RtpSctpSender->pipeline), "sink"); <]
 *
 *    // queue Stats (before the sink)
 *    guint lvl_buf, lvl_byte;
 *    guint64 lvl_time;
 *    gint64 avg_in;
 *    g_object_get(G_OBJECT(RtpSctpSender->queue),
 *          "current-level-buffers", &lvl_buf,
 *          "current-level-bytes", &lvl_byte,
 *          "current-level-time", &lvl_time,
 *          "avg-in-rate", &avg_in,
 *          NULL);
 *    GST_INFO_OBJECT(RtpSctpSender->pipeline, "queue STATS: #%3u, %3ukB, %luns, avg: %ld kbit/s",
 *          lvl_buf, (lvl_byte >> 10), lvl_time, (avg_in >> 7));
 *
 *
 *    // usrsctp stats
 *    struct sctpstat *stats = NULL;
 *    g_object_get(G_OBJECT(RtpSctpSender->sctpsink), "usrsctp-stats", &stats, NULL);
 *    GST_INFO_OBJECT(RtpSctpSender->pipeline, "usrsctp STATS: rdata %4u, sdata %6u, rtxdata %3u, "
 *          "frtx %3u, hb %2u, todata %2u drpchklmt %u, drprwnd %u, ecncwnd %u, randry %u",
 *          stats->sctps_recvdata,            [> total input DATA chunks    <]
 *          stats->sctps_senddata,            [> total output DATA chunks   <]
 *          stats->sctps_sendretransdata,     [> total output retransmitted DATA chunks <]
 *          stats->sctps_sendfastretrans,     [> total output fast retransmitted DATA chunks <]
 *          stats->sctps_sendheartbeat,       [> total output HB chunks     <]
 *          stats->sctps_timodata,            [> Number of T3 data time outs <]
 *          stats->sctps_datadropchklmt,      [> Number of in data drops due to chunk limit reached <]
 *          stats->sctps_datadroprwnd,        [> Number of in data drops due to rwnd limit reached <]
 *          stats->sctps_ecnereducedcwnd,     [> Number of times a ECN reduced the cwnd <]
 *          stats->sctps_primary_randry      [> Number of times the sender ran dry of user data on primary <]
 *          );
 *
 *    return TRUE;
 * } */



/* helper functions */

#if 0
gboolean
have_element (const gchar * element_name)
{
   GstPluginFeature *feature;

   feature = gst_default_registry_find_feature (element_name,
         GST_TYPE_ELEMENT_FACTORY);
   if (feature) {
      g_object_unref (feature);
      return TRUE;
   }
   return FALSE;
}
#endif

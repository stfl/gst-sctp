/* GstRtpSctpReceiver
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

#include <usrsctp.h>
#include <string.h>

#define GETTEXT_PACKAGE "RtpSctpReceiver"

GST_DEBUG_CATEGORY_STATIC (rtpsctprecv);
#define GST_CAT_DEFAULT rtpsctprecv

typedef struct _GstRtpSctpReceiver GstRtpSctpReceiver;
struct _GstRtpSctpReceiver
{
   GstElement *pipeline;
   GstBus *bus;
   GMainLoop *main_loop;

   GstElement *source_element;
   GstElement *sink_element;

   GstElement *jitterbuffer;
   GstElement *sctpsrc;

   gboolean paused_for_buffering;
   guint timer_id;
   guint stats_timer_id;
};

GstRtpSctpReceiver *gst_RtpSctpReceiver_new (void);
void gst_RtpSctpReceiver_free (GstRtpSctpReceiver *RtpSctpReceiver);
void gst_RtpSctpReceiver_create_pipeline (GstRtpSctpReceiver *RtpSctpReceiver);
void gst_RtpSctpReceiver_create_pipeline_playbin (GstRtpSctpReceiver *RtpSctpReceiver, const char *uri);
void gst_RtpSctpReceiver_start (GstRtpSctpReceiver *RtpSctpReceiver);
void gst_RtpSctpReceiver_stop (GstRtpSctpReceiver *RtpSctpReceiver);

static gboolean gst_RtpSctpReceiver_handle_message (GstBus *bus, GstMessage *message, gpointer data);
static gboolean onesecond_timer (gpointer priv);
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
gchar *timestamp_offset = NULL;
gchar *deadline = NULL;
gchar *path_delay = NULL;
gchar *delay_padding = NULL;

static GOptionEntry entries[] = {
   {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL},
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
   GstRtpSctpReceiver *RtpSctpReceiver;
   GMainLoop *main_loop;

   context = g_option_context_new ("- RTP SCTP Receiver");
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

   GST_DEBUG_CATEGORY_INIT (rtpsctprecv, "rtpsctprecv",
         GST_DEBUG_BG_YELLOW | GST_DEBUG_FG_WHITE | GST_DEBUG_BOLD,
         "The RTP over SCTP Receiver Pipeline");

   RtpSctpReceiver = gst_RtpSctpReceiver_new ();
   gst_RtpSctpReceiver_create_pipeline (RtpSctpReceiver);
   gst_RtpSctpReceiver_start (RtpSctpReceiver);

   main_loop = g_main_loop_new (NULL, TRUE);
   RtpSctpReceiver->main_loop = main_loop;

   g_main_loop_run (main_loop);

   exit (0);
}


GstRtpSctpReceiver* gst_RtpSctpReceiver_new (void)
{
   GstRtpSctpReceiver *RtpSctpReceiver;

   RtpSctpReceiver = g_new0 (GstRtpSctpReceiver, 1);

   return RtpSctpReceiver;
}

void
gst_RtpSctpReceiver_free (GstRtpSctpReceiver * RtpSctpReceiver)
{
   if (RtpSctpReceiver->source_element) {
      gst_object_unref (RtpSctpReceiver->source_element);
      RtpSctpReceiver->source_element = NULL;
   }
   if (RtpSctpReceiver->sink_element) {
      gst_object_unref (RtpSctpReceiver->sink_element);
      RtpSctpReceiver->sink_element = NULL;
   }
   if (RtpSctpReceiver->jitterbuffer) {
      gst_object_unref (RtpSctpReceiver->jitterbuffer);
      RtpSctpReceiver->jitterbuffer = NULL;
   }

   if (RtpSctpReceiver->pipeline) {
      gst_element_set_state (RtpSctpReceiver->pipeline, GST_STATE_NULL);
      gst_object_unref (RtpSctpReceiver->pipeline);
      RtpSctpReceiver->pipeline = NULL;
   }
   g_free (RtpSctpReceiver);
}

void
gst_RtpSctpReceiver_create_pipeline (GstRtpSctpReceiver * RtpSctpReceiver)
{
   /* create pipeline */
   GstElement *pipeline = gst_pipeline_new ("pipeline");

   /* create element */
   GstElement *source;
   GstElement *source2 = NULL;
   GstElement *funnel = NULL;

   if (variant == PIPELINE_UDP) {
      source = gst_element_factory_make("udpsrc", "source");
      g_object_set(G_OBJECT(source),
            "timeout", 10000000000,  // us > 10s
            "address", "192.168.0.1",
            "port",   55555,
            NULL);

   } else if (variant == PIPELINE_UDP_DUPL) {
      /* udpsrc1 \
       *          funnel - jitterbuffer
       * udpsrc2 /                                             */

      source = gst_element_factory_make("udpsrc", "source");
      g_object_set(G_OBJECT(source),
            "timeout", 10000000000,  // us > 10s
            "address", "192.168.0.1",
            "port",   51111,
            NULL);
      /* gst_util_set_object_arg(G_OBJECT(source), "caps", */
      /*       "application/x-rtp,clock-rate=(int)90000,payload=(int)96"); */

      source2 = gst_element_factory_make("udpsrc", "source2");
      g_object_set(G_OBJECT(source2),
            "address", "128.131.89.238",
            "port",   52222,
            NULL);
      /* gst_util_set_object_arg(G_OBJECT(source2), "caps", */
      /*       "application/x-rtp,clock-rate=(int)90000,payload=(int)96"); */

      funnel = gst_element_factory_make("funnel", "funnel");

   } else {
      source = gst_element_factory_make("sctpsrc", "source");

      if (variant == PIPELINE_CMT ||
         variant == PIPELINE_CMT_DPR ||
         variant == PIPELINE_CMT_DUPL) {
         gst_util_set_object_arg(G_OBJECT(source), "cmt",  "true");
         gst_util_set_object_arg(G_OBJECT(source), "buffer-split",  "true");
      }
   }

   /* // used with rtpmux
    * GstCaps *src_caps = gst_caps_new_simple("application/x-rtp",
    *       "clock-rate",     G_TYPE_INT,      90000,
    *       "payload",        G_TYPE_INT,      96,
    *       NULL); */

   GstCaps *jb_caps = gst_caps_new_simple("application/x-rtp",
         "media",          G_TYPE_STRING,  "video",
         "clock-rate",     G_TYPE_INT,      90000,
         "encoding-name",  G_TYPE_STRING,  "RAW",
         "sampling",       G_TYPE_STRING,  "RGBA",
         "depth",          G_TYPE_STRING,  "8",
         "width",          G_TYPE_STRING,  "90",
         "height",         G_TYPE_STRING,  "60",
         "a-framerate",    G_TYPE_STRING,  "24",
         "payload",        G_TYPE_INT,      96,
         NULL);

   GstElement *rtpdepay = gst_element_factory_make("rtpvrawdepay", "rtpdepay");
   gst_util_set_object_arg(G_OBJECT(rtpdepay), "timestamp-offset", timestamp_offset);
   gst_util_set_object_arg(G_OBJECT(rtpdepay), "deadline", deadline);

   GstElement *jitterbuffer = gst_element_factory_make("rtpjitterbuffer", "jitterbuffer");
   gst_util_set_object_arg(G_OBJECT(jitterbuffer), "systime-offset", timestamp_offset);
   gst_util_set_object_arg(G_OBJECT(jitterbuffer), "deadline", deadline);
   g_object_set(G_OBJECT(jitterbuffer),
         "rfc7273-sync", FALSE,
         "drop-on-latency", TRUE,
         "do-retransmission", FALSE,
         "max-dropout-time",   50,
         "max-misorder-time",  50,  // ms
         NULL);

   gst_util_set_object_arg(G_OBJECT(jitterbuffer), "mode",  "slave");

   if (variant == PIPELINE_CMT_DPR) {
      GString *jbuf_latency = g_string_new("");
      g_string_printf(jbuf_latency, "%u",
            (uint32_t) ((atoi(deadline) - atoi(path_delay) - atoi(delay_padding)) / 1000));
      gst_util_set_object_arg(G_OBJECT(jitterbuffer), "latency",  jbuf_latency->str);
      g_string_free(jbuf_latency, TRUE);
   } else {
      gst_util_set_object_arg(G_OBJECT(jitterbuffer), "latency", "50");
   }

   GstElement *videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
   GstElement *videosink = gst_element_factory_make("ximagesink", "videsink");

   /* add to pipeline */
   gst_bin_add_many(GST_BIN(pipeline), source, rtpdepay, jitterbuffer, videoconvert, videosink, NULL);

   if (variant == PIPELINE_UDP_DUPL) {
      gst_bin_add_many(GST_BIN(pipeline), source2, funnel, NULL);

     GstPad *pad, *link_pad;

     link_pad = gst_element_get_static_pad(source, "src");
     pad = gst_element_get_request_pad(funnel, "sink_%u");
     gst_pad_link(link_pad, pad);
     gst_object_unref(pad);

     link_pad = gst_element_get_static_pad(source2, "src");
     pad = gst_element_get_request_pad(funnel, "sink_%u");
     gst_pad_link(link_pad, pad);
     gst_object_unref(pad);

     if (!gst_element_link_filtered(funnel, jitterbuffer, jb_caps)) {
        g_warning ("Failed to filtered link funnel and jitterbuffer!\n");
     }

   } else { // link all other, non-duplicated pipelines
      if (!gst_element_link_filtered(source, jitterbuffer, jb_caps)) {
         g_warning ("Failed to filtered link source and jitterbuffer!\n");
      }
   }
   gst_caps_unref(jb_caps);

   gst_element_link_many(jitterbuffer, rtpdepay, videoconvert, videosink, NULL);

   RtpSctpReceiver->pipeline = pipeline;

   gst_pipeline_set_auto_flush_bus (GST_PIPELINE (pipeline), FALSE);
   RtpSctpReceiver->bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
   gst_bus_add_watch (RtpSctpReceiver->bus, gst_RtpSctpReceiver_handle_message, RtpSctpReceiver);

   RtpSctpReceiver->source_element = gst_bin_get_by_name (GST_BIN (pipeline), "source");
   /* RtpSctpReceiver->source_element = source; */
   RtpSctpReceiver->sink_element = gst_bin_get_by_name (GST_BIN (pipeline), "videosink");
   /* RtpSctpReceiver->sink_element = videosink; */

   RtpSctpReceiver->jitterbuffer = jitterbuffer;
   RtpSctpReceiver->sctpsrc = source;
}

void
gst_RtpSctpReceiver_start (GstRtpSctpReceiver * RtpSctpReceiver)
{
   gst_element_set_state (RtpSctpReceiver->pipeline, GST_STATE_READY);

   RtpSctpReceiver->timer_id = g_timeout_add (1000, onesecond_timer, RtpSctpReceiver);
}

void
gst_RtpSctpReceiver_stop (GstRtpSctpReceiver * RtpSctpReceiver)
{
   gst_element_set_state (RtpSctpReceiver->pipeline, GST_STATE_NULL);

   g_source_remove (RtpSctpReceiver->timer_id);
}

static void
gst_RtpSctpReceiver_handle_eos (GstRtpSctpReceiver * RtpSctpReceiver)
{
   gst_RtpSctpReceiver_stop (RtpSctpReceiver);
}

static void
gst_RtpSctpReceiver_handle_error (GstRtpSctpReceiver * RtpSctpReceiver,
      GError * error, const char *debug)
{
   g_print ("error: %s\n", error->message);
   gst_RtpSctpReceiver_stop (RtpSctpReceiver);
}

static void
gst_RtpSctpReceiver_handle_warning (GstRtpSctpReceiver * RtpSctpReceiver,
      GError * error, const char *debug)
{
   g_print ("warning: %s\n", error->message);
}

static void
gst_RtpSctpReceiver_handle_info (GstRtpSctpReceiver * RtpSctpReceiver,
      GError * error, const char *debug)
{
   g_print ("info: %s\n", error->message);
}

static void
gst_RtpSctpReceiver_handle_null_to_ready (GstRtpSctpReceiver *
      RtpSctpReceiver)
{
   gst_element_set_state (RtpSctpReceiver->pipeline, GST_STATE_PAUSED);

}

static void
gst_RtpSctpReceiver_handle_ready_to_paused (GstRtpSctpReceiver *
      RtpSctpReceiver)
{
   if (!RtpSctpReceiver->paused_for_buffering) {
      gst_element_set_state (RtpSctpReceiver->pipeline, GST_STATE_PLAYING);
   }
}

static void
gst_RtpSctpReceiver_handle_paused_to_playing (GstRtpSctpReceiver *
      RtpSctpReceiver)
{
   /* RtpSctpReceiver->stats_timer_id = g_timeout_add (1000, stats_timer, RtpSctpReceiver); */
   GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(RtpSctpReceiver->pipeline),
         GST_DEBUG_GRAPH_SHOW_ALL,
         "receiver");
/* https://gstreamer.freedesktop.org/data/doc/gstreamer/head/gstreamer/html/gstreamer-GstInfo.html#GstDebugGraphDetails
 */
}

static void
gst_RtpSctpReceiver_handle_playing_to_paused (GstRtpSctpReceiver *
      RtpSctpReceiver)
{

}

static void
gst_RtpSctpReceiver_handle_paused_to_ready (GstRtpSctpReceiver *
      RtpSctpReceiver)
{

}

static void
gst_RtpSctpReceiver_handle_ready_to_null (GstRtpSctpReceiver *
      RtpSctpReceiver)
{
   GstElement *jbuf = RtpSctpReceiver->jitterbuffer;
   guint64 num_pushed,
           num_lost,
           num_late,
           num_old_dropped,
           num_duplicate,
           num_all_duplicate,
           num_deadline_missed_in,
           num_deadline_missed_out,
           num_deadline_hit_in,
           num_deadline_hit_out,
           avg_delay_hit_in,
           avg_delay_hit_out,
           avg_delay_received,
           avg_delay_pushed;

   gdouble avg_jitter;
   GstStructure *jbuf_stats;

   // Jitter Bufer Stats
   g_object_get(G_OBJECT(jbuf), "stats", &jbuf_stats, NULL);
   if ( ! gst_structure_get (jbuf_stats,
            "num-pushed",     G_TYPE_UINT64, &num_pushed,
            "num-lost",       G_TYPE_UINT64, &num_lost,
            "num-late",       G_TYPE_UINT64, &num_late,
            "num-duplicates", G_TYPE_UINT64, &num_duplicate,
            "num-all-duplicates", G_TYPE_UINT64, &num_all_duplicate,
            "num-old-dropped", G_TYPE_UINT64, &num_old_dropped,
            "num-deadline-missed-in", G_TYPE_UINT64, &num_deadline_missed_in,
            "num-deadline-missed-out", G_TYPE_UINT64, &num_deadline_missed_out,
            "num-deadline-hit-in", G_TYPE_UINT64, &num_deadline_hit_in,
            "num-deadline-hit-out", G_TYPE_UINT64, &num_deadline_hit_out,
            "avg-delay-hit-in", G_TYPE_UINT64, &avg_delay_hit_in,
            "avg-delay-hit-out", G_TYPE_UINT64, &avg_delay_hit_out,
            "avg-delay-pushed", G_TYPE_UINT64, &avg_delay_pushed,
            "avg-delay-received", G_TYPE_UINT64, &avg_delay_received,
            "avg-jitter",     G_TYPE_UINT64, &avg_jitter,
            NULL)) {
      g_error("error getting the jitterbuffer stats");
   }
   GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "Jitterbuffer stats:");
   GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "Average Jitter:\t\t%.3f", avg_jitter);
   GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "Num Pushed:\t\t%ld", num_pushed);
   GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "Num Lost:\t\t%ld", num_lost);
   GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "Num Late:\t\t%ld", num_late);
   GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "Num Duplicate:\t\t%ld", num_duplicate);
   GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "Num Old Dropped:\t%ld\n", num_old_dropped);

   GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "Num All Duplicate:\t\t%ld", num_all_duplicate);
   GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "IN: Num Deadline Missed:\t%ld", num_deadline_missed_in);
   GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "IN: Num Deadline Hit:\t%ld", num_deadline_hit_in);
   GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "IN: Average TTD (hit):\t%.2fms", (gdouble)avg_delay_hit_in / 1000000);
   GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "IN: Average TTD:\t%.2fms\n", (gdouble)avg_delay_received / 1000000);

   GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "OUT: Num Deadline Missed:\t%ld", num_deadline_missed_out);
   GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "OUT: Num Deadline Hit:\t%ld", num_deadline_hit_out);
   GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "OUT: Average TTD (hit):\t%.2fms", (gdouble)avg_delay_hit_out / 1000000);
   GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "OUT: Average TTD (pushed):\t%.2fms", (gdouble)avg_delay_pushed / 1000000);


   g_main_loop_quit (RtpSctpReceiver->main_loop);
}


static gboolean
gst_RtpSctpReceiver_handle_message (GstBus * bus, GstMessage * message,
      gpointer data)
{
   GstRtpSctpReceiver *RtpSctpReceiver = (GstRtpSctpReceiver *) data;

   switch (GST_MESSAGE_TYPE (message)) {
      case GST_MESSAGE_EOS:
         gst_RtpSctpReceiver_handle_eos (RtpSctpReceiver);
         break;
      case GST_MESSAGE_ERROR:
         {
            GError *error = NULL;
            gchar *debug;

            gst_message_parse_error (message, &error, &debug);
            gst_RtpSctpReceiver_handle_error (RtpSctpReceiver, error, debug);
         }
         break;
      case GST_MESSAGE_WARNING:
         {
            GError *error = NULL;
            gchar *debug;

            gst_message_parse_warning (message, &error, &debug);
            gst_RtpSctpReceiver_handle_warning (RtpSctpReceiver, error, debug);
         }
         break;
      case GST_MESSAGE_INFO:
         {
            GError *error = NULL;
            gchar *debug;

            gst_message_parse_info (message, &error, &debug);
            gst_RtpSctpReceiver_handle_info (RtpSctpReceiver, error, debug);
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
            if (GST_ELEMENT (message->src) == RtpSctpReceiver->pipeline) {
               if (verbose)
                  g_print ("state change from %s to %s\n",
                        gst_element_state_get_name (oldstate),
                        gst_element_state_get_name (newstate));
               switch (GST_STATE_TRANSITION (oldstate, newstate)) {
                  case GST_STATE_CHANGE_NULL_TO_READY:
                     gst_RtpSctpReceiver_handle_null_to_ready (RtpSctpReceiver);
                     break;
                  case GST_STATE_CHANGE_READY_TO_PAUSED:
                     gst_RtpSctpReceiver_handle_ready_to_paused (RtpSctpReceiver);
                     break;
                  case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
                     gst_RtpSctpReceiver_handle_paused_to_playing (RtpSctpReceiver);
                     break;
                  case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
                     gst_RtpSctpReceiver_handle_playing_to_paused (RtpSctpReceiver);
                     break;
                  case GST_STATE_CHANGE_PAUSED_TO_READY:
                     gst_RtpSctpReceiver_handle_paused_to_ready (RtpSctpReceiver);
                     break;
                  case GST_STATE_CHANGE_READY_TO_NULL:
                     gst_RtpSctpReceiver_handle_ready_to_null (RtpSctpReceiver);
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
            if (!RtpSctpReceiver->paused_for_buffering && percent < 100) {
               g_print ("pausing for buffing\n");
               RtpSctpReceiver->paused_for_buffering = TRUE;
               gst_element_set_state (RtpSctpReceiver->pipeline, GST_STATE_PAUSED);
            } else if (RtpSctpReceiver->paused_for_buffering && percent == 100) {
               g_print ("unpausing for buffing\n");
               RtpSctpReceiver->paused_for_buffering = FALSE;
               gst_element_set_state (RtpSctpReceiver->pipeline, GST_STATE_PLAYING);
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
      case GST_MESSAGE_ELEMENT:
            if (gst_message_has_name (message, "GstUDPSrcTimeout")) {
               g_print ("got GstUDPSrcTimeout\n");
               gst_RtpSctpReceiver_stop (RtpSctpReceiver);
               break;
            }
      case GST_MESSAGE_STEP_DONE:
      case GST_MESSAGE_APPLICATION:
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



static gboolean
onesecond_timer (gpointer priv)
{
   /* GstRtpSctpReceiver *RtpSctpReceiver = (GstRtpSctpReceiver *)priv; */

   /* g_print (".\n"); */

   return TRUE;
}

/* static gboolean
 * stats_timer (gpointer priv)
 * {
 *    GstRtpSctpReceiver *RtpSctpReceiver = (GstRtpSctpReceiver *)priv;
 *    GstElement *jbuf = RtpSctpReceiver->jitterbuffer;
 *
 *    // Jitter Bufer Stats
 *    guint64 num_pushed, num_lost, num_late, num_duplicate=0;
 *    gdouble avg_jitter;
 *    GstStructure *jbuf_stats;
 *    g_object_get(G_OBJECT(jbuf), "stats", &jbuf_stats, NULL);
 *    if ( ! gst_structure_get (jbuf_stats,
 *             "num-pushed",     G_TYPE_UINT64, &num_pushed,
 *             "num-lost",       G_TYPE_UINT64, &num_lost,
 *             "num-late",       G_TYPE_UINT64, &num_late,
 *             "num-duplicates", G_TYPE_UINT64, &num_duplicate,
 *             "avg-jitter",     G_TYPE_UINT64, &avg_jitter,
 *             NULL)) {
 *       g_error("error getting the jitterbuffer stats");
 *    }
 *    GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "jbuffer STATS: ~jitter: %f #pushed: %8lu, "
 *          "#lost: %3lu, #late: %4lu, #dupl: %3lu",
 *          avg_jitter, num_pushed, num_lost, num_late, num_duplicate);
 *
 *    // FIXME somehow free(stats);
 *
 *
 *    // usrsctp Stats
 *    struct sctpstat *usrsctp_stats = NULL;
 *    g_object_get(G_OBJECT(RtpSctpReceiver->sctpsrc), "usrsctp-stats", &usrsctp_stats, NULL);
 *
 *    guint64 pushed;
 *    g_object_get(G_OBJECT(RtpSctpReceiver->sctpsrc), "pushed", &pushed, NULL);
 *    GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "usrsctp rdata %u, src pushed %lu, %0.2f, "
 *          "usrsctp drp %u",
 *          usrsctp_stats->sctps_recvdata,
 *          pushed,
 *          (gdouble)pushed/(usrsctp_stats->sctps_recvdata),
 *          usrsctp_stats->sctps_pdrpmbda
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

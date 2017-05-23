/* GstRtpSctpReceiver
 * Copyright (C) 2016 FIXME <fixme@example.com>
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

#define GETTEXT_PACKAGE "RtpSctpReceiver"


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
static gboolean stats_timer (gpointer priv);


gboolean verbose;

static GOptionEntry entries[] = {
   {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL},

   {NULL}

};

int
main (int argc, char *argv[])
{
   GError *error = NULL;
   GOptionContext *context;
   GstRtpSctpReceiver *RtpSctpReceiver;
   GMainLoop *main_loop;

   context = g_option_context_new ("- FIXME");
   g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
   g_option_context_add_group (context, gst_init_get_option_group ());
   if (!g_option_context_parse (context, &argc, &argv, &error)) {
      g_print ("option parsing failed: %s\n", error->message);
      exit (1);
   }
   g_option_context_free (context);

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
   GstElement *source = gst_element_factory_make("sctpsrc", "source");
   if (!source) {
      g_critical ("Failed to create element of type 'sctpsrc'\n");
      /* return -1; */
   }
   g_object_set(G_OBJECT(source),
         "bind",   "11.1.1.1",
         "port",   1117,
         NULL);

   GstCaps *src_caps = gst_caps_new_simple("application/x-rtp",
         "media",              G_TYPE_STRING, "video",
         "clock-rate",         G_TYPE_INT,    90000,
         "encoding-name",      G_TYPE_STRING, "H264",
         "a-framerate",        G_TYPE_STRING, "25",
         "packetization-mode", G_TYPE_STRING, "1",
         "payload",            G_TYPE_INT,    96,
         NULL);

   GstElement *rtpdepay = gst_element_factory_make("rtph264depay", "rtpdepay");
   if (!rtpdepay) {
      g_critical ("failed to create element of type 'rtph264depay'\n");
      /* return -1; */
   }

   GstElement *jitterbuffer = gst_element_factory_make("rtpjitterbuffer", "jitterbuffer");
   if (!jitterbuffer) {
      g_critical ("Failed to create element of type 'rtpjitterbuffer'\n");
      /* return -1; */
   }
   g_object_set(G_OBJECT(jitterbuffer),
         "latency", 100,
         "max-dropout-time", 100,
         "max-misorder-time", 100, // ms
         NULL );

   GstElement *decoder = gst_element_factory_make("avdec_h264", "decoder");
   if (!decoder) {
      g_critical ("failed to create element of type 'avdec_h264'\n");
      /* return -1; */
   }

   GstCaps *decode_caps = gst_caps_new_simple("video/x-h264",
         "stream-format",      G_TYPE_STRING,     "avc",
         "alignment",          G_TYPE_STRING,     "au",
         "level",              G_TYPE_STRING,     "3.1",
         "width",              G_TYPE_INT,        1280,
         "height",             G_TYPE_INT,        720,
         "framerate",          GST_TYPE_FRACTION, 25,    1,
         "pixel-aspect-ratio", GST_TYPE_FRACTION, 1,     1,
         NULL);

   GstElement *videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
   if (!videoconvert) {
      g_critical ("failed to create element of type 'videoconvert'\n");
      /* return -1; */
   }

   GstElement *videosink = gst_element_factory_make("ximagesink", "videsink");
   if (!videosink) {
      g_critical ("Failed to create element of type 'ximagesink'\n");
      /* return -1; */
   }

   /* add to pipeline */
   gst_bin_add_many(GST_BIN(pipeline), source, rtpdepay, jitterbuffer, decoder, videoconvert,
         videosink, NULL);

   /* link */
 /* rtpdepay, decoder, */

   if (!gst_element_link_filtered(source, jitterbuffer, src_caps)) {
      g_warning ("Failed to link filterd source and jitterbuffer!\n");
   }
   gst_caps_unref(src_caps);

   if (!gst_element_link(jitterbuffer, rtpdepay)) {
      g_critical ("Failed to link jitterbuffer and rtpdepay'\n");
   }

   if (!gst_element_link_filtered(rtpdepay, decoder, decode_caps)) {
      g_critical ("Failed to link rtpdepay and decoder'\n");
   }
   gst_caps_unref(decode_caps);

   if (!gst_element_link(decoder, videoconvert)) {
      g_critical ("Failed to link decoder and videoconvert'\n");
   }

   if (!gst_element_link(videoconvert, videosink)) {
      g_critical ("Failed to link videoconvert and videosink'\n");
   }

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
   RtpSctpReceiver->stats_timer_id = g_timeout_add (3000, stats_timer, RtpSctpReceiver);
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



static gboolean
onesecond_timer (gpointer priv)
{
   /* GstRtpSctpReceiver *RtpSctpReceiver = (GstRtpSctpReceiver *)priv; */

   /* g_print (".\n"); */

   return TRUE;
}

static gboolean
stats_timer (gpointer priv)
{
   GstRtpSctpReceiver *RtpSctpReceiver = (GstRtpSctpReceiver *)priv;
   GstElement *jbuf = RtpSctpReceiver->jitterbuffer;

   guint64 num_pushed, num_lost, num_late, num_duplicate=0;
   gdouble avg_jitter;
   GstStructure *jbuf_stats;
   g_object_get(G_OBJECT(jbuf), "stats", &jbuf_stats, NULL);
   if ( ! gst_structure_get (jbuf_stats,
            "num-pushed",     G_TYPE_UINT64, &num_pushed,
            "num-lost",       G_TYPE_UINT64, &num_lost,
            "num-late",       G_TYPE_UINT64, &num_late,
            "num-duplicates", G_TYPE_UINT64, &num_duplicate,
            "avg-jitter",     G_TYPE_UINT64, &avg_jitter,
            NULL)) {
      g_error("error getting the jitterbuffer stats");
   }
   GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "Jbuffer STATS: ~jitter: %f #pushed: %8lu, #lost: %3lu, #late: %4lu, #dupl: %3lu",
         avg_jitter, num_pushed, num_lost, num_late, num_duplicate);

   // FIXME somehow free(stats);

   struct sctpstat *usrsctp_stats = NULL;
   g_object_get(G_OBJECT(RtpSctpReceiver->sctpsrc), "usrsctp-stats", &usrsctp_stats, NULL);

   /* GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "usrsctp STATS: rdata %f, sdata %6u, " */
   /*       "hb %2u, todata %2u drpchklmt %u, randry %u", */
   /*       (double)usrsctp_stats->sctps_recvdata,            [> total input DATA chunks    <] */
   /*       usrsctp_stats->sctps_senddata,            [> total output DATA chunks   <] */
   /*       usrsctp_stats->sctps_sendheartbeat,       [> total output HB chunks     <] */
   /*       usrsctp_stats->sctps_timodata,            [> Number of T3 data time outs <] */
   /*       usrsctp_stats->sctps_datadropchklmt,      [> Number of in data drops due to chunk limit reached <] */
   /*       usrsctp_stats->sctps_primary_randry      [> Number of times the sender ran dry of user data on primary <] */
   /*       ); */

   guint64 pushed;
   g_object_get(G_OBJECT(RtpSctpReceiver->sctpsrc), "pushed", &pushed, NULL);
   GST_INFO_OBJECT(RtpSctpReceiver->pipeline, "pushed %lu rdata %u %0.2f, drp %u",
         pushed, usrsctp_stats->sctps_recvdata,
         (gdouble)pushed/(usrsctp_stats->sctps_recvdata),
         usrsctp_stats->sctps_pdrpmbda
         );

   return TRUE;
}

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

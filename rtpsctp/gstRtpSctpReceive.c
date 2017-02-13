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

#define GETTEXT_PACKAGE "RtpSctpReceiver"


typedef struct _GstRtpSctpReceiver GstRtpSctpReceiver;
struct _GstRtpSctpReceiver
{
   GstElement *pipeline;
   GstBus *bus;
   GMainLoop *main_loop;

   GstElement *source_element;
   GstElement *sink_element;

   gboolean paused_for_buffering;
   guint timer_id;
};

GstRtpSctpReceiver *gst_RtpSctpReceiver_new (void);
void gst_RtpSctpReceiver_free (GstRtpSctpReceiver *RtpSctpReceiver);
void gst_RtpSctpReceiver_create_pipeline (GstRtpSctpReceiver *RtpSctpReceiver);
void gst_RtpSctpReceiver_create_pipeline_playbin (GstRtpSctpReceiver *RtpSctpReceiver, const char *uri);
void gst_RtpSctpReceiver_start (GstRtpSctpReceiver *RtpSctpReceiver);
void gst_RtpSctpReceiver_stop (GstRtpSctpReceiver *RtpSctpReceiver);

static gboolean gst_RtpSctpReceiver_handle_message (GstBus *bus,
      GstMessage *message, gpointer data);
static gboolean onesecond_timer (gpointer priv);


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
   GstElement *source = gst_element_factory_make("udpsrc", "source");
   if (!source) {
      g_critical ("Failed to create element of type 'udpsrc'\n");
      /* return -1; */
   }
   g_object_set(G_OBJECT(source),
         "port",   5000,
         NULL);
   GstCaps *src_caps = gst_caps_new_simple("application/x-rtp",
         "media", G_TYPE_STRING, "video",
         "clock-rate", G_TYPE_INT, 90000,
         "encoding-name", G_TYPE_STRING, "H264",
         "a-framerate", G_TYPE_STRING, "25",
         "packetization-mode", G_TYPE_STRING, "1",
         "payload", G_TYPE_INT, 96,
         NULL);

   GstElement *rtpdepay = gst_element_factory_make("rtph264depay", "rtpdepay");
   if (!rtpdepay) {
      g_critical ("failed to create element of type 'rtph264depay'\n");
      /* return -1; */
   }

   GstElement *decoder = gst_element_factory_make("avdec_h264", "decoder");
   if (!decoder) {
      g_critical ("failed to create element of type 'avdec_h264'\n");
      /* return -1; */
   }

   GstElement *videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
   if (!videoconvert) {
      g_critical ("failed to create element of type 'avdec_h264'\n");
      /* return -1; */
   }

   GstElement *videosink = gst_element_factory_make("ximagesink", "videsink");
   if (!videosink) {
      g_critical ("Failed to create element of type 'ximagesink'\n");
      /* return -1; */
   }

   /* add to pipeline */
   gst_bin_add_many(GST_BIN(pipeline), source, rtpdepay, decoder, videoconvert, videosink, NULL);

   /* link */
 /* rtpdepay, decoder, */

   if (!gst_element_link_filtered(source, rtpdepay, src_caps)) {
      g_warning ("Failed to link filterd source and rtpdepay!\n");
   }
   gst_caps_unref(src_caps);

   if (!gst_element_link(rtpdepay, decoder)) {
      g_critical ("Failed to link source and rtpdepay'\n");
   }

   if (!gst_element_link(decoder, videoconvert)) {
      g_critical ("Failed to link source and rtpdepay'\n");
   }

   if (!gst_element_link(videoconvert, videosink)) {
      g_critical ("Failed to link source and rtpdepay'\n");
   }

   /* if (!gst_element_link_many (source, rtpdepay, decoder, videosink, NULL)) { */
   /*    g_warning ("Failed to link elements!"); */
   /* } */

   /* GstCaps *c_tmp;
    * g_object_get (G_OBJECT(source), "caps", &c_tmp, NULL);
    * g_print("%s\n", gst_caps_to_string(c_tmp));
    * g_object_unref(c_tmp); */

   RtpSctpReceiver->pipeline = pipeline;

   gst_pipeline_set_auto_flush_bus (GST_PIPELINE (pipeline), FALSE);
   RtpSctpReceiver->bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
   gst_bus_add_watch (RtpSctpReceiver->bus, gst_RtpSctpReceiver_handle_message,
         RtpSctpReceiver);

   RtpSctpReceiver->source_element =
      gst_bin_get_by_name (GST_BIN (pipeline), "source");
   RtpSctpReceiver->sink_element =
      gst_bin_get_by_name (GST_BIN (pipeline), "videosink");
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
   //GstRtpSctpReceiver *RtpSctpReceiver = (GstRtpSctpReceiver *)priv;

   g_print (".\n");

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

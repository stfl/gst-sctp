/* GstRtpUdpReceiver
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <stdlib.h>
#include <glib.h>

#define GETTEXT_PACKAGE "RtpUdpReceiver"


typedef struct _GstRtpUdpReceiver GstRtpUdpReceiver;
struct _GstRtpUdpReceiver
{
   GstElement *pipeline;
   GstBus *bus;
   GMainLoop *main_loop;

   GstElement *source_element;
   GstElement *sink_element;

   gboolean paused_for_buffering;
   guint timer_id;
};

GstRtpUdpReceiver *gst_RtpUdpReceiver_new (void);
void gst_RtpUdpReceiver_free (GstRtpUdpReceiver *RtpUdpReceiver);
void gst_RtpUdpReceiver_create_pipeline (GstRtpUdpReceiver *RtpUdpReceiver);
void gst_RtpUdpReceiver_create_pipeline_playbin (GstRtpUdpReceiver *RtpUdpReceiver, const char *uri);
void gst_RtpUdpReceiver_start (GstRtpUdpReceiver *RtpUdpReceiver);
void gst_RtpUdpReceiver_stop (GstRtpUdpReceiver *RtpUdpReceiver);

static gboolean gst_RtpUdpReceiver_handle_message (GstBus *bus,
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
   GstRtpUdpReceiver *RtpUdpReceiver;
   GMainLoop *main_loop;

   context = g_option_context_new ("- FIXME");
   g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
   g_option_context_add_group (context, gst_init_get_option_group ());
   if (!g_option_context_parse (context, &argc, &argv, &error)) {
      g_print ("option parsing failed: %s\n", error->message);
      exit (1);
   }
   g_option_context_free (context);

   RtpUdpReceiver = gst_RtpUdpReceiver_new ();
   gst_RtpUdpReceiver_create_pipeline (RtpUdpReceiver);
   gst_RtpUdpReceiver_start (RtpUdpReceiver);

   main_loop = g_main_loop_new (NULL, TRUE);
   RtpUdpReceiver->main_loop = main_loop;

   g_main_loop_run (main_loop);

   exit (0);
}


GstRtpUdpReceiver* gst_RtpUdpReceiver_new (void)
{
   GstRtpUdpReceiver *RtpUdpReceiver;

   RtpUdpReceiver = g_new0 (GstRtpUdpReceiver, 1);

   return RtpUdpReceiver;
}

void
gst_RtpUdpReceiver_free (GstRtpUdpReceiver * RtpUdpReceiver)
{
   if (RtpUdpReceiver->source_element) {
      gst_object_unref (RtpUdpReceiver->source_element);
      RtpUdpReceiver->source_element = NULL;
   }
   if (RtpUdpReceiver->sink_element) {
      gst_object_unref (RtpUdpReceiver->sink_element);
      RtpUdpReceiver->sink_element = NULL;
   }

   if (RtpUdpReceiver->pipeline) {
      gst_element_set_state (RtpUdpReceiver->pipeline, GST_STATE_NULL);
      gst_object_unref (RtpUdpReceiver->pipeline);
      RtpUdpReceiver->pipeline = NULL;
   }
   g_free (RtpUdpReceiver);
}

void
gst_RtpUdpReceiver_create_pipeline (GstRtpUdpReceiver * RtpUdpReceiver)
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

   RtpUdpReceiver->pipeline = pipeline;

   gst_pipeline_set_auto_flush_bus (GST_PIPELINE (pipeline), FALSE);
   RtpUdpReceiver->bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
   gst_bus_add_watch (RtpUdpReceiver->bus, gst_RtpUdpReceiver_handle_message,
         RtpUdpReceiver);

   RtpUdpReceiver->source_element =
      gst_bin_get_by_name (GST_BIN (pipeline), "source");
   RtpUdpReceiver->sink_element =
      gst_bin_get_by_name (GST_BIN (pipeline), "videosink");
}

void
gst_RtpUdpReceiver_start (GstRtpUdpReceiver * RtpUdpReceiver)
{
   gst_element_set_state (RtpUdpReceiver->pipeline, GST_STATE_READY);

   RtpUdpReceiver->timer_id = g_timeout_add (1000, onesecond_timer, RtpUdpReceiver);
}

void
gst_RtpUdpReceiver_stop (GstRtpUdpReceiver * RtpUdpReceiver)
{
   gst_element_set_state (RtpUdpReceiver->pipeline, GST_STATE_NULL);

   g_source_remove (RtpUdpReceiver->timer_id);
}

static void
gst_RtpUdpReceiver_handle_eos (GstRtpUdpReceiver * RtpUdpReceiver)
{
   gst_RtpUdpReceiver_stop (RtpUdpReceiver);
}

static void
gst_RtpUdpReceiver_handle_error (GstRtpUdpReceiver * RtpUdpReceiver,
      GError * error, const char *debug)
{
   g_print ("error: %s\n", error->message);
   gst_RtpUdpReceiver_stop (RtpUdpReceiver);
}

static void
gst_RtpUdpReceiver_handle_warning (GstRtpUdpReceiver * RtpUdpReceiver,
      GError * error, const char *debug)
{
   g_print ("warning: %s\n", error->message);
}

static void
gst_RtpUdpReceiver_handle_info (GstRtpUdpReceiver * RtpUdpReceiver,
      GError * error, const char *debug)
{
   g_print ("info: %s\n", error->message);
}

static void
gst_RtpUdpReceiver_handle_null_to_ready (GstRtpUdpReceiver *
      RtpUdpReceiver)
{
   gst_element_set_state (RtpUdpReceiver->pipeline, GST_STATE_PAUSED);

}

static void
gst_RtpUdpReceiver_handle_ready_to_paused (GstRtpUdpReceiver *
      RtpUdpReceiver)
{
   if (!RtpUdpReceiver->paused_for_buffering) {
      gst_element_set_state (RtpUdpReceiver->pipeline, GST_STATE_PLAYING);
   }
}

static void
gst_RtpUdpReceiver_handle_paused_to_playing (GstRtpUdpReceiver *
      RtpUdpReceiver)
{

}

static void
gst_RtpUdpReceiver_handle_playing_to_paused (GstRtpUdpReceiver *
      RtpUdpReceiver)
{

}

static void
gst_RtpUdpReceiver_handle_paused_to_ready (GstRtpUdpReceiver *
      RtpUdpReceiver)
{

}

static void
gst_RtpUdpReceiver_handle_ready_to_null (GstRtpUdpReceiver *
      RtpUdpReceiver)
{
   g_main_loop_quit (RtpUdpReceiver->main_loop);

}


static gboolean
gst_RtpUdpReceiver_handle_message (GstBus * bus, GstMessage * message,
      gpointer data)
{
   GstRtpUdpReceiver *RtpUdpReceiver = (GstRtpUdpReceiver *) data;

   switch (GST_MESSAGE_TYPE (message)) {
      case GST_MESSAGE_EOS:
         gst_RtpUdpReceiver_handle_eos (RtpUdpReceiver);
         break;
      case GST_MESSAGE_ERROR:
         {
            GError *error = NULL;
            gchar *debug;

            gst_message_parse_error (message, &error, &debug);
            gst_RtpUdpReceiver_handle_error (RtpUdpReceiver, error, debug);
         }
         break;
      case GST_MESSAGE_WARNING:
         {
            GError *error = NULL;
            gchar *debug;

            gst_message_parse_warning (message, &error, &debug);
            gst_RtpUdpReceiver_handle_warning (RtpUdpReceiver, error, debug);
         }
         break;
      case GST_MESSAGE_INFO:
         {
            GError *error = NULL;
            gchar *debug;

            gst_message_parse_info (message, &error, &debug);
            gst_RtpUdpReceiver_handle_info (RtpUdpReceiver, error, debug);
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
            if (GST_ELEMENT (message->src) == RtpUdpReceiver->pipeline) {
               if (verbose)
                  g_print ("state change from %s to %s\n",
                        gst_element_state_get_name (oldstate),
                        gst_element_state_get_name (newstate));
               switch (GST_STATE_TRANSITION (oldstate, newstate)) {
                  case GST_STATE_CHANGE_NULL_TO_READY:
                     gst_RtpUdpReceiver_handle_null_to_ready (RtpUdpReceiver);
                     break;
                  case GST_STATE_CHANGE_READY_TO_PAUSED:
                     gst_RtpUdpReceiver_handle_ready_to_paused (RtpUdpReceiver);
                     break;
                  case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
                     gst_RtpUdpReceiver_handle_paused_to_playing (RtpUdpReceiver);
                     break;
                  case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
                     gst_RtpUdpReceiver_handle_playing_to_paused (RtpUdpReceiver);
                     break;
                  case GST_STATE_CHANGE_PAUSED_TO_READY:
                     gst_RtpUdpReceiver_handle_paused_to_ready (RtpUdpReceiver);
                     break;
                  case GST_STATE_CHANGE_READY_TO_NULL:
                     gst_RtpUdpReceiver_handle_ready_to_null (RtpUdpReceiver);
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
            if (!RtpUdpReceiver->paused_for_buffering && percent < 100) {
               g_print ("pausing for buffing\n");
               RtpUdpReceiver->paused_for_buffering = TRUE;
               gst_element_set_state (RtpUdpReceiver->pipeline, GST_STATE_PAUSED);
            } else if (RtpUdpReceiver->paused_for_buffering && percent == 100) {
               g_print ("unpausing for buffing\n");
               RtpUdpReceiver->paused_for_buffering = FALSE;
               gst_element_set_state (RtpUdpReceiver->pipeline, GST_STATE_PLAYING);
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
   //GstRtpUdpReceiver *RtpUdpReceiver = (GstRtpUdpReceiver *)priv;

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

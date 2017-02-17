/* _GstRtpSctpSender
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

#define GETTEXT_PACKAGE "RtpSctpSender"


typedef struct __GstRtpSctpSender _GstRtpSctpSender;
struct __GstRtpSctpSender
{
   GstElement *pipeline;
   GstBus *bus;
   GMainLoop *main_loop;

   GstElement *source_element;
   GstElement *sink_element;

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
   _GstRtpSctpSender *RtpSctpSender;
   GMainLoop *main_loop;

   context = g_option_context_new ("- FIXME");
   g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
   g_option_context_add_group (context, gst_init_get_option_group ());
   if (!g_option_context_parse (context, &argc, &argv, &error)) {
      g_print ("option parsing failed: %s\n", error->message);
      exit (1);
   }
   g_option_context_free (context);

   RtpSctpSender = gst_RtpSctpSender_new ();

   /* if (argc > 1) { */
   /*    gchar *uri; */
   /*    if (gst_uri_is_valid (argv[1])) { */
   /*       uri = g_strdup (argv[1]); */
   /*    } else { */
   /*       uri = g_filename_to_uri (argv[1], NULL, NULL); */
   /*    } */
   /*    gst_RtpSctpSender_create_pipeline_playbin (RtpSctpSender, uri); */
   /*    g_free (uri); */
   /* } else { */
      gst_RtpSctpSender_create_pipeline (RtpSctpSender);
   /* } */

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
   GstElement *pipeline, *source, *sink, *encoder, *rtppay;
   /* GError *error = NULL; */

   /* create pipeline */
   pipeline = gst_pipeline_new ("pipeline");

   /* create element */
   source = gst_element_factory_make("videotestsrc", "source");
   if (!source) {
      /* g_print ("Failed to create element of type 'videotestsrc'\n"); */
      g_print ("Failed to create element of type 'videotestsrc'\n");
      /* return -1; */
   }
   g_object_set(G_OBJECT(source),
         /* "pattern",   GstVideoTestSrcPattern.GST_VIDEO_TEST_SRC_SNOW, */
         "is-live",   TRUE,
         NULL);
   gst_util_set_object_arg(G_OBJECT(source), "pattern", "snow");
   GstCaps *src_caps = gst_caps_new_simple ("video/x-raw",
         "width", G_TYPE_INT, 1280,
         "height", G_TYPE_INT, 720,
         "framerate", GST_TYPE_FRACTION, 25, 1,
         NULL);

   encoder = gst_element_factory_make("x264enc", "encoder");
   if (!encoder) {
      g_print ("failed to create element of type 'x264enc'\n");
      /* return -1; */
   }
   gst_util_set_object_arg(G_OBJECT(encoder), "tune", "zerolatency");
   /* g_object_set(encoder, "tune", (GstX264EncTune)"zerolatency", NULL); */


   rtppay = gst_element_factory_make("rtph264pay", "rtppay");
   if (!rtppay) {
      g_print ("failed to create element of type 'rtph264pay'\n");
      /* return -1; */
   }

   sink = gst_element_factory_make("sctpsink", "sink");
   if (!sink) {
      g_print ("Failed to create element of type 'sctpsink'\n");
      /* return -1; */
   }
   g_object_set(sink,
         /* "host",   "127.0.0.1", */
         "host",   "::1",
         "port",    1117,
         NULL);

   /* add to pipeline */
   gst_bin_add_many(GST_BIN(pipeline), source, encoder, rtppay, sink, NULL);

   /* link */
   if (!gst_element_link_filtered(source, encoder, src_caps)) {
      g_warning ("Failed to link filterd source and enocder!\n");
   }
   gst_caps_unref(src_caps);

   if (!gst_element_link_many (encoder, rtppay, sink, NULL)) {
      g_warning ("Failed to link elements!");
   }


   RtpSctpSender->pipeline = pipeline;

   gst_pipeline_set_auto_flush_bus (GST_PIPELINE (pipeline), FALSE);
   RtpSctpSender->bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
   gst_bus_add_watch (RtpSctpSender->bus, gst_RtpSctpSender_handle_message,
         RtpSctpSender);

   /* TODO: add stome other props here */
   RtpSctpSender->source_element =
      gst_bin_get_by_name (GST_BIN (pipeline), "source");
   RtpSctpSender->sink_element =
      gst_bin_get_by_name (GST_BIN (pipeline), "sink");
}

void
gst_RtpSctpSender_start (_GstRtpSctpSender * RtpSctpSender)
{
   gst_element_set_state (RtpSctpSender->pipeline, GST_STATE_READY);

   RtpSctpSender->timer_id = g_timeout_add (1000, onesecond_timer, RtpSctpSender);
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



static gboolean
onesecond_timer (gpointer priv)
{
   //_GstRtpSctpSender *RtpSctpSender = (_GstRtpSctpSender *)priv;

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

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

GST_DEBUG_CATEGORY_STATIC (gst_sctpsink_debug_category);
#define GST_CAT_DEFAULT gst_sctpsink_debug_category

#define SCTP_DEFAULT_HOST        "localhost"
#define SCTP_DEFAULT_PORT        9

enum
{
   PROP_0,
   PROP_HOST,
   PROP_PORT,
   PROP_URI,
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
gboolean gst_sctpsink_buffer_list (GstBuffer **buffer, guint idx, gpointer user_data);

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

   /* Install the properties so they can be found and set */
   g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_HOST,
         g_param_spec_string ("host", "host",
            "The host/IP of the endpoint send the packets to. The other side must be running",
            SCTP_DEFAULT_HOST,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
   g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_PORT,
         g_param_spec_int ("port", "port",
            "The port to send the packets to",
            0, 65535,
            SCTP_DEFAULT_PORT,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

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

   GST_DEBUG_OBJECT (sctpsink, "set_property");

   switch (property_id) {
      case PROP_HOST:
         {
            const gchar *host;

            host = g_value_get_string (value);
            g_free (sctpsink->host);
            sctpsink->host = g_strdup (host);
            /* g_free (sctpsink->uri);
             * sctpsink->uri =
             *     g_strdup_printf ("sctp://%s:%d", sctpsink->host, sctpsink->port); */
            GST_INFO("set host:%s", sctpsink->host);
            break;
         }
      case PROP_PORT:
         sctpsink->port = g_value_get_int (value);
         /* g_free (sctpsink->uri);
          * sctpsink->uri =
          *     g_strdup_printf ("sctp://%s:%d", sctpsink->host, sctpsink->port); */
         GST_INFO("set port:%d", sctpsink->port);
         break;
      default:
         G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
         break;
   }

   /* switch (property_id) {
    *   default:
    *     G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    *     break;
    * } */
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

/* start and stop processing, ideal for opening/closing the resource */
static gboolean
gst_sctpsink_start (GstBaseSink * sink)
{
   GstSctpSink *sctpsink = GST_SCTPSINK (sink);

   GST_DEBUG_OBJECT (sctpsink, "start sctpsink");
   GST_INFO("starting SCTP socket");

   //FIXME: setup socket here!

   return TRUE;
}

static gboolean
gst_sctpsink_stop (GstBaseSink * sink)
{
   GstSctpSink *sctpsink = GST_SCTPSINK (sink);

   GST_DEBUG_OBJECT (sctpsink, "stop");
   // FIXME: close the socket

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
   GstSctpSink *sctpsink = GST_SCTPSINK (sink);

   GST_DEBUG_OBJECT (sctpsink, "query: %s", GST_QUERY_TYPE_NAME(query));

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

static GstFlowReturn
gst_sctpsink_render (GstBaseSink * sink, GstBuffer * buffer)
{
   GstSctpSink *sctpsink = GST_SCTPSINK (sink);

   GST_DEBUG_OBJECT (sctpsink, "render");
   /* GST_WARNING("there's some rendering to do"); */
   //FIXME: implement the rendering

   GST_INFO("got a buffer of size:%lu", gst_buffer_get_size(buffer));

   return GST_FLOW_OK;
}

/* Render a BufferList */
static GstFlowReturn
gst_sctpsink_render_list (GstBaseSink * sink, GstBufferList * buffer_list)
{
   GstSctpSink *sctpsink = GST_SCTPSINK (sink);
   /* GST_DEBUG_OBJECT (sctpsink, "render_list of length: %d", gst_buffer_list_length(buffer_list)); */

   gint fullsize = 0;
   gst_buffer_list_foreach(buffer_list, gst_sctpsink_buffer_list, &fullsize);

   GST_DEBUG_OBJECT (sctpsink, "fullsize: %db in %d chunks", fullsize,
         gst_buffer_list_length(buffer_list));

   return GST_FLOW_OK;
}


gboolean
gst_sctpsink_buffer_list (GstBuffer **buffer, guint idx, gpointer user_data) {
   gint *fullsize = (gint *)user_data;

   /* GST_DEBUG("iter through list [%d] size: %lu", idx,  gst_buffer_get_size(*buffer)); */

   *fullsize += gst_buffer_get_size(*buffer);
   /* GST_DEBUG("size: %d", *fullsize); */

   return TRUE;
}
// vim: ft=c

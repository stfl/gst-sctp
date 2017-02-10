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

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include "gstsctpsrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_sctpsrc_debug_category);
#define GST_CAT_DEFAULT gst_sctpsrc_debug_category

/* prototypes */


static void gst_sctpsrc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_sctpsrc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_sctpsrc_dispose (GObject * object);
static void gst_sctpsrc_finalize (GObject * object);

static GstCaps *gst_sctpsrc_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_sctpsrc_negotiate (GstBaseSrc * src);
static GstCaps *gst_sctpsrc_fixate (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_sctpsrc_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_sctpsrc_decide_allocation (GstBaseSrc * src,
    GstQuery * query);
static gboolean gst_sctpsrc_start (GstBaseSrc * src);
static gboolean gst_sctpsrc_stop (GstBaseSrc * src);
static void gst_sctpsrc_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end);
static gboolean gst_sctpsrc_get_size (GstBaseSrc * src, guint64 * size);
static gboolean gst_sctpsrc_is_seekable (GstBaseSrc * src);
static gboolean gst_sctpsrc_prepare_seek_segment (GstBaseSrc * src,
    GstEvent * seek, GstSegment * segment);
static gboolean gst_sctpsrc_do_seek (GstBaseSrc * src, GstSegment * segment);
static gboolean gst_sctpsrc_unlock (GstBaseSrc * src);
static gboolean gst_sctpsrc_unlock_stop (GstBaseSrc * src);
static gboolean gst_sctpsrc_query (GstBaseSrc * src, GstQuery * query);
static gboolean gst_sctpsrc_event (GstBaseSrc * src, GstEvent * event);
static GstFlowReturn gst_sctpsrc_create (GstBaseSrc * src, guint64 offset,
    guint size, GstBuffer ** buf);
static GstFlowReturn gst_sctpsrc_alloc (GstBaseSrc * src, guint64 offset,
    guint size, GstBuffer ** buf);
static GstFlowReturn gst_sctpsrc_fill (GstBaseSrc * src, guint64 offset,
    guint size, GstBuffer * buf);

enum
{
  PROP_0
};

/* pad templates */

static GstStaticPadTemplate gst_sctpsrc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/unknown")
    );


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstSctpSrc, gst_sctpsrc, GST_TYPE_BASE_SRC,
    GST_DEBUG_CATEGORY_INIT (gst_sctpsrc_debug_category, "sctpsrc", 0,
        "debug category for sctpsrc element"));

static void
gst_sctpsrc_class_init (GstSctpSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_static_pad_template_get (&gst_sctpsrc_src_template));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "FIXME Long name", "Generic", "FIXME Description",
      "FIXME <fixme@example.com>");

  gobject_class->set_property = gst_sctpsrc_set_property;
  gobject_class->get_property = gst_sctpsrc_get_property;
  gobject_class->dispose = gst_sctpsrc_dispose;
  gobject_class->finalize = gst_sctpsrc_finalize;
  base_src_class->get_caps = GST_DEBUG_FUNCPTR (gst_sctpsrc_get_caps);
  base_src_class->negotiate = GST_DEBUG_FUNCPTR (gst_sctpsrc_negotiate);
  base_src_class->fixate = GST_DEBUG_FUNCPTR (gst_sctpsrc_fixate);
  base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_sctpsrc_set_caps);
  base_src_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_sctpsrc_decide_allocation);
  base_src_class->start = GST_DEBUG_FUNCPTR (gst_sctpsrc_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_sctpsrc_stop);
  base_src_class->get_times = GST_DEBUG_FUNCPTR (gst_sctpsrc_get_times);
  base_src_class->get_size = GST_DEBUG_FUNCPTR (gst_sctpsrc_get_size);
  base_src_class->is_seekable = GST_DEBUG_FUNCPTR (gst_sctpsrc_is_seekable);
  base_src_class->prepare_seek_segment =
      GST_DEBUG_FUNCPTR (gst_sctpsrc_prepare_seek_segment);
  base_src_class->do_seek = GST_DEBUG_FUNCPTR (gst_sctpsrc_do_seek);
  base_src_class->unlock = GST_DEBUG_FUNCPTR (gst_sctpsrc_unlock);
  base_src_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_sctpsrc_unlock_stop);
  base_src_class->query = GST_DEBUG_FUNCPTR (gst_sctpsrc_query);
  base_src_class->event = GST_DEBUG_FUNCPTR (gst_sctpsrc_event);
  base_src_class->create = GST_DEBUG_FUNCPTR (gst_sctpsrc_create);
  base_src_class->alloc = GST_DEBUG_FUNCPTR (gst_sctpsrc_alloc);
  base_src_class->fill = GST_DEBUG_FUNCPTR (gst_sctpsrc_fill);

}

static void
gst_sctpsrc_init (GstSctpSrc * sctpsrc)
{
}

void
gst_sctpsrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (object);

  GST_DEBUG_OBJECT (sctpsrc, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_sctpsrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (object);

  GST_DEBUG_OBJECT (sctpsrc, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_sctpsrc_dispose (GObject * object)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (object);

  GST_DEBUG_OBJECT (sctpsrc, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_sctpsrc_parent_class)->dispose (object);
}

void
gst_sctpsrc_finalize (GObject * object)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (object);

  GST_DEBUG_OBJECT (sctpsrc, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_sctpsrc_parent_class)->finalize (object);
}

/* get caps from subclass */
static GstCaps *
gst_sctpsrc_get_caps (GstBaseSrc * src, GstCaps * filter)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (src);

  GST_DEBUG_OBJECT (sctpsrc, "get_caps");

  return NULL;
}

/* decide on caps */
static gboolean
gst_sctpsrc_negotiate (GstBaseSrc * src)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (src);

  GST_DEBUG_OBJECT (sctpsrc, "negotiate");

  return TRUE;
}

/* called if, in negotiation, caps need fixating */
static GstCaps *
gst_sctpsrc_fixate (GstBaseSrc * src, GstCaps * caps)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (src);

  GST_DEBUG_OBJECT (sctpsrc, "fixate");

  return NULL;
}

/* notify the subclass of new caps */
static gboolean
gst_sctpsrc_set_caps (GstBaseSrc * src, GstCaps * caps)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (src);

  GST_DEBUG_OBJECT (sctpsrc, "set_caps");

  return TRUE;
}

/* setup allocation query */
static gboolean
gst_sctpsrc_decide_allocation (GstBaseSrc * src, GstQuery * query)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (src);

  GST_DEBUG_OBJECT (sctpsrc, "decide_allocation");

  return TRUE;
}

/* start and stop processing, ideal for opening/closing the resource */
static gboolean
gst_sctpsrc_start (GstBaseSrc * src)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (src);

  GST_DEBUG_OBJECT (sctpsrc, "start");

  return TRUE;
}

static gboolean
gst_sctpsrc_stop (GstBaseSrc * src)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (src);

  GST_DEBUG_OBJECT (sctpsrc, "stop");

  return TRUE;
}

/* given a buffer, return start and stop time when it should be pushed
 * out. The base class will sync on the clock using these times. */
static void
gst_sctpsrc_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (src);

  GST_DEBUG_OBJECT (sctpsrc, "get_times");

}

/* get the total size of the resource in bytes */
static gboolean
gst_sctpsrc_get_size (GstBaseSrc * src, guint64 * size)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (src);

  GST_DEBUG_OBJECT (sctpsrc, "get_size");

  return TRUE;
}

/* check if the resource is seekable */
static gboolean
gst_sctpsrc_is_seekable (GstBaseSrc * src)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (src);

  GST_DEBUG_OBJECT (sctpsrc, "is_seekable");

  return TRUE;
}

/* Prepare the segment on which to perform do_seek(), converting to the
 * current basesrc format. */
static gboolean
gst_sctpsrc_prepare_seek_segment (GstBaseSrc * src, GstEvent * seek,
    GstSegment * segment)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (src);

  GST_DEBUG_OBJECT (sctpsrc, "prepare_seek_segment");

  return TRUE;
}

/* notify subclasses of a seek */
static gboolean
gst_sctpsrc_do_seek (GstBaseSrc * src, GstSegment * segment)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (src);

  GST_DEBUG_OBJECT (sctpsrc, "do_seek");

  return TRUE;
}

/* unlock any pending access to the resource. subclasses should unlock
 * any function ASAP. */
static gboolean
gst_sctpsrc_unlock (GstBaseSrc * src)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (src);

  GST_DEBUG_OBJECT (sctpsrc, "unlock");

  return TRUE;
}

/* Clear any pending unlock request, as we succeeded in unlocking */
static gboolean
gst_sctpsrc_unlock_stop (GstBaseSrc * src)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (src);

  GST_DEBUG_OBJECT (sctpsrc, "unlock_stop");

  return TRUE;
}

/* notify subclasses of a query */
static gboolean
gst_sctpsrc_query (GstBaseSrc * src, GstQuery * query)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (src);

  GST_DEBUG_OBJECT (sctpsrc, "query");

  return TRUE;
}

/* notify subclasses of an event */
static gboolean
gst_sctpsrc_event (GstBaseSrc * src, GstEvent * event)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (src);

  GST_DEBUG_OBJECT (sctpsrc, "event");

  return TRUE;
}

/* ask the subclass to create a buffer with offset and size, the default
 * implementation will call alloc and fill. */
static GstFlowReturn
gst_sctpsrc_create (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (src);

  GST_DEBUG_OBJECT (sctpsrc, "create");

  return GST_FLOW_OK;
}

/* ask the subclass to allocate an output buffer. The default implementation
 * will use the negotiated allocator. */
static GstFlowReturn
gst_sctpsrc_alloc (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (src);

  GST_DEBUG_OBJECT (sctpsrc, "alloc");

  return GST_FLOW_OK;
}

/* ask the subclass to fill the buffer with data from offset and size */
static GstFlowReturn
gst_sctpsrc_fill (GstBaseSrc * src, guint64 offset, guint size, GstBuffer * buf)
{
  GstSctpSrc *sctpsrc = GST_SCTPSRC (src);

  GST_DEBUG_OBJECT (sctpsrc, "fill");

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{

  /* FIXME Remember to set the rank if it's an element that is meant
     to be autoplugged by decodebin. */
  return gst_element_register (plugin, "sctpsrc", GST_RANK_NONE,
      GST_TYPE_SCTPSRC);
}

/* FIXME: these are normally defined by the GStreamer build system.
   If you are creating an element to be included in gst-plugins-*,
   remove these, as they're always defined.  Otherwise, edit as
   appropriate for your external plugin package. */
#ifndef VERSION
#define VERSION "0.0.FIXME"
#endif
#ifndef PACKAGE
#define PACKAGE "FIXME_package"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "FIXME_package_name"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://FIXME.org/"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    sctpsrc,
    "FIXME plugin description",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)

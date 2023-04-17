/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2023 Rodrigo Valente Bernardes <<user@hostname.org>>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-gzdec
 *
 * FIXME:Describe gzdec here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! gzdec ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/controller/controller.h>

#include "gstgzdec.h"

GST_DEBUG_CATEGORY_STATIC (gst_gzdec_debug);
#define GST_CAT_DEFAULT gst_gzdec_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT,
};

/* the capabilities of the inputs and outputs.
 *
 * FIXME:describe the real formats here.
 */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

#define gst_gzdec_parent_class parent_class
G_DEFINE_TYPE (Gstgzdec, gst_gzdec, GST_TYPE_BASE_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE (gzdec, "gzdec", GST_RANK_NONE,
    GST_TYPE_GZDEC);

static void gst_gzdec_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_gzdec_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_gzdec_transform_ip (GstBaseTransform *
    base, GstBuffer * outbuf);

/* GObject vmethod implementations */

/* initialize the gzdec's class */
static void
gst_gzdec_class_init (GstgzdecClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_gzdec_set_property;
  gobject_class->get_property = gst_gzdec_get_property;

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE));

  gst_element_class_set_details_simple (gstelement_class,
      "gzdec",
      "Generic/Filter",
      "FIXME:Generic Template Filter", "Rodrigo Valente Bernardes <<user@hostname.org>>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  GST_BASE_TRANSFORM_CLASS (klass)->transform_ip =
      GST_DEBUG_FUNCPTR (gst_gzdec_transform_ip);

  /* debug category for fltering log messages
   *
   * FIXME:exchange the string 'Template gzdec' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_gzdec_debug, "gzdec", 0,
      "Template gzdec");
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_gzdec_init (Gstgzdec * filter)
{
    filter->strm = malloc(sizeof(z_stream));
    filter->strm->zalloc = Z_NULL;
    filter->strm->zfree  = Z_NULL;
    filter->strm->opaque = Z_NULL;
    int ret = inflateInit(filter->strm);
    if(ret != Z_OK ) {
        GST_DEBUG_OBJECT(filter, "inflate returned %d", ret);
    }
}

static void
gst_gzdec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstgzdec *filter = GST_GZDEC (object);

  switch (prop_id) {
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gzdec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstgzdec *filter = GST_GZDEC (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstBaseTransform vmethod implementations */

static GstFlowReturn
gst_gzdec_transform_ip (GstBaseTransform * base, GstBuffer * outbuf)
{
    Gstgzdec *filter = GST_GZDEC (base);

    if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (outbuf)))
        gst_object_sync_values (GST_OBJECT (filter), GST_BUFFER_TIMESTAMP (outbuf));

    GstMapInfo info;
    if( !gst_buffer_map(outbuf, &info, GST_MAP_WRITE)) {
        // TODO log err
        return GST_FLOW_ERROR;
    }

    filter->strm->next_in = filter->strm->next_out = info.data;
    filter->strm->avail_in = filter->strm->avail_out = info.size;

    int ret =inflate(filter->strm, Z_FINISH);
    switch ( ret )
    {
    case Z_STREAM_END:
        return GST_FLOW_OK;
    case Z_OK:
    case Z_DATA_ERROR:
        GST_DEBUG_OBJECT(filter, "inflate returned %d", ret);
    default:
        break;
    }

  return GST_FLOW_OK;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
gzdec_init (GstPlugin * gzdec)
{
  return GST_ELEMENT_REGISTER (gzdec, gzdec);
}

/* gstreamer looks for this structure to register gzdecs
 *
 * FIXME:exchange the string 'Template gzdec' with you gzdec description
 */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    gzdec,
    "gzdec",
    gzdec_init,
    PACKAGE_VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)

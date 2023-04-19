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

enum
{
  PROP_0,
};

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

static GstFlowReturn gst_gzdec_transform_ip (GstBaseTransform *
    base, GstBuffer * outbuf);

/* GObject vmethod implementations */

/* initialize the gzdec's class */
static void
gst_gzdec_class_init (GstgzdecClass * klass)
{
    gst_element_class_set_details_simple (gstelement_class,
        "gzdec",
        "Filter/Decoder",
        PLUGIN_DESCRIPTION, "Rodrigo Valente Bernardes rodrigovalente1996@gmail.com");
  
    // add our pads
    gst_element_class_add_pad_template (gstelement_class,
        gst_static_pad_template_get (&src_template));
    gst_element_class_add_pad_template (gstelement_class,
        gst_static_pad_template_get (&sink_template));

    GST_BASE_TRANSFORM_CLASS (klass)->transform_ip =
        GST_DEBUG_FUNCPTR (gst_gzdec_transform_ip);

    GST_DEBUG_CATEGORY_INIT (gst_gzdec_debug, "gzdec", 0,
        PLUGIN_DESCRIPTION);
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_gzdec_init (Gstgzdec * filter){}

static void
gst_gzdec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstgzdec *filter = GST_GZDEC (object);
}

static void
gst_gzdec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstgzdec *filter = GST_GZDEC (object);
}

static GstFlowReturn
gzdec_gzip_decompress(Gstgzdec* filter, GstBuffer* outbuf, GstMapInfo *info)
{
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree  = Z_NULL;
    strm.opaque = Z_NULL;
    strm.next_in= Z_NULL;
    strm.avail_in = 0;
    strm.avail_out = 0;
    strm.next_out = Z_NULL;

    int ret = inflateInit2(&strm, MAX_WBITS + 16);
    if(ret != Z_OK ) {
        GST_ERROR("inflateInit2 failed with %s", zError(ret));
        return GST_FLOW_ERROR;
    }

    GstMemory * mem = gst_allocator_alloc(NULL, CHUNK, NULL);

    GstMapInfo out;
    if( !gst_memory_map(mem,&out, GST_MAP_WRITE) ){
        GST_ERROR("failed to map to memory");
        return GST_FLOW_ERROR;
    }

    strm.next_in = (Bytef*) info->data;
    strm.avail_in = (uInt) info->size;
    GST_DEBUG("decompressing %ld bytes", info->size);

    // reset the buffer
    gst_buffer_memset(outbuf, 0, 0, info->size);
    int idx = 0;
    do{
        do{
            strm.avail_out = CHUNK;
            strm.next_out = (Bytef*) out.data;
            // get the data til there's none more left
            GST_DEBUG("avail_out:%d before inflate", strm.avail_out);
            ret = inflate(&strm, Z_NO_FLUSH);
            GST_DEBUG("inflate ret:%s avail_out:%d", zError(ret), strm.avail_out);
            switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                GST_ERROR("inflate failed with %s", zError(ret));
                (void)inflateEnd(&strm);
                return GST_FLOW_ERROR;
            }
            // append the memory to result buffer
            gst_buffer_insert_memory(outbuf, idx, mem);
            idx += CHUNK - strm.avail_out;
        }while(strm.avail_out == 0);
    }while(ret != Z_STREAM_END);
    (void)inflateEnd(&strm);

    // re-adjust the buffer size after the appends
    gst_buffer_set_size(outbuf, idx);
    gst_memory_unmap(mem, &out);

    return GST_FLOW_OK;
}

static GstFlowReturn
gzdec_bzip_decompress(Gstgzdec* filter, GstBuffer* outbuf, GstMapInfo *info)
{
}

static GstFlowReturn
gst_gzdec_transform_ip (GstBaseTransform * base, GstBuffer * outbuf)
{
    Gstgzdec *filter = GST_GZDEC (base);

    GST_DEBUG("transform ip");

    if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (outbuf)))
        gst_object_sync_values (GST_OBJECT (filter), GST_BUFFER_TIMESTAMP (outbuf));

    GstMapInfo in;
    if( !gst_buffer_map(outbuf, &in, GST_MAP_READ) ) {
        GST_ERROR("FAILED TO MAP BUFFER INFO");
        return GST_FLOW_ERROR;
    }

    GstFlowReturn ret = GST_FLOW_OK;
    // decide wether or not we should decompress and which type we should use if needed
    if(in.data && in.size >= 2){
        // look for the header and the magic number
        switch((in.data[0] | in.data[1])){
        case GZIP:
            GST_INFO("decoding GZIP");
            ret = gzdec_gzip_decompress(filter, outbuf, &in);
            break;
        case BZIP:
            GST_INFO("decoding BZIP");
            ret = gzdec_bzip_decompress(filter, outbuf, &in);
            break;
        default: // not covered, assume it's not compressed
        }
    }

    gst_buffer_unmap(outbuf, &in);

    return ret;
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

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    gzdec,
    "gzdec",
    gzdec_init,
    PACKAGE_VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)

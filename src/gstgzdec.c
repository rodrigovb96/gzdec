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
static gboolean
gst_gzdec_sink_event(GstBaseTransform *trans, GstEvent *event);
static void
gzdec_push_drain(Gstgzdec * filter);

/* HELPER FUNCTIONS */

static void
gzdec_send_decoded_bytes(Gstgzdec * filter, guint8 * data, gsize size);

static void
gzdec_store_decoded_bytes(Gstgzdec* filter, guint8* data, gsize decoded_bytes);

static int 
gzdec_gzip_decompress(Gstgzdec* filter, guint8* input, gsize insize);
static int 
gzdec_bzip_decompress(Gstgzdec* filter, guint8* input, gsize insize);

static GstFlowReturn
gzdec_get_flow_return(Gstgzdec * filter, int ret);

/* -------------  */

/* GObject vmethod implementations */

/* initialize the gzdec's class */
static void
gst_gzdec_class_init (GstgzdecClass * klass)
{
    GstElementClass *gstelement_class;
    gstelement_class = (GstElementClass *) klass;

    gst_element_class_set_details_simple (gstelement_class,
        "gzdec",
        "Filter/Decoder",
        PLUGIN_DESCRIPTION, "Rodrigo Valente Bernardes rodrigovalente1996@gmail.com");
  
    // add our pads
    gst_element_class_add_pad_template (gstelement_class,
        gst_static_pad_template_get (&src_template));
    gst_element_class_add_pad_template (gstelement_class,
        gst_static_pad_template_get (&sink_template));

    GST_BASE_TRANSFORM_CLASS (klass)->transform_ip = GST_DEBUG_FUNCPTR (gst_gzdec_transform_ip);
    GST_BASE_TRANSFORM_CLASS (klass)->sink_event   = GST_DEBUG_FUNCPTR (gst_gzdec_sink_event);

    GST_DEBUG_CATEGORY_INIT (gst_gzdec_debug, "gzdec", 0, PLUGIN_DESCRIPTION);
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_gzdec_init (Gstgzdec * filter){
    filter->zstrm.zalloc = Z_NULL;
    filter->zstrm.zfree  = Z_NULL;
    filter->zstrm.opaque = Z_NULL;
    filter->zstrm.next_in= Z_NULL;
    filter->zstrm.avail_in = 0;
    filter->zstrm.avail_out = 0;
    filter->zstrm.next_out = Z_NULL;

    filter->bstrm.bzalloc = NULL;
    filter->bstrm.bzfree  = NULL;
    filter->bstrm.opaque = NULL;
    filter->bstrm.next_in= NULL;
    filter->bstrm.avail_in = 0;
    filter->bstrm.avail_out = 0;
    filter->bstrm.next_out = NULL;

    filter->decoder_type = NONE;
    filter->dec_buf = NULL;
    filter->dec_buf_size = 0;
}

static gboolean
gst_gzdec_sink_event(GstBaseTransform *trans, GstEvent *event){
    Gstgzdec* filter = GST_GZDEC(trans);

    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_EOS:  // drain our remaining buffers
        GST_INFO("received End of Stream event, drain");
        gzdec_push_drain(filter);
        break;
    default:
        break;
    }
    GST_INFO("calling parent sink_event");
    return GST_BASE_TRANSFORM_CLASS(gst_gzdec_parent_class)->sink_event(trans, event);
}


static GstFlowReturn
gst_gzdec_transform_ip (GstBaseTransform * base, GstBuffer * outbuf)
{
    Gstgzdec *filter = GST_GZDEC (base);

    if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (outbuf)))
        gst_object_sync_values (GST_OBJECT (filter), GST_BUFFER_TIMESTAMP (outbuf));

    GstMapInfo in;
    if( !gst_buffer_map(outbuf, &in, GST_MAP_READWRITE) ) {
        GST_ERROR("FAILED TO MAP BUFFER INFO");
        return GST_FLOW_ERROR;
    }

    DECOMPRESS_FUN(decompress) = NULL;

    gboolean do_init = FALSE;
    // decide wether or not we should decompress and which type we should use if needed
    if(filter->decoder_type == NONE && in.data && in.size >= 2){
        // look for the header and the magic number
        filter->decoder_type = in.data[0] | in.data[1];
        do_init = TRUE;
    }

    switch(filter->decoder_type){
    case GZIP:
        decompress = gzdec_gzip_decompress;

        if(do_init){
            GST_DEBUG("decoding GZIP");
            int ret = inflateInit2(&filter->zstrm, MAX_WBITS + 16);
            if(ret != Z_OK ) {
                GST_ERROR("failed to init gzip decompress! reason: %s", zError(ret));
                return GST_FLOW_ERROR;
            }
        }
        break;
    case BZIP:
        decompress = gzdec_bzip_decompress;

        if(do_init) {
            GST_DEBUG("decoding BZIP");
            int ret = BZ2_bzDecompressInit(&filter->bstrm, 0 /*verbosity*/, 0 /*small*/);
            if(ret != BZ_OK ) {
                GST_ERROR("failed to init bzip decompress! reason: %s", zError(ret));
                return GST_FLOW_ERROR;
            }
        }
        break;
    default: // not covered, assume it's not compressed
        GST_INFO("data is neither GZIP nor BZIP, ignoring by passing it through");
    }

    int ret =0;
    if(decompress) {
        ret = decompress(filter, in.data, in.size);

        gzdec_send_decoded_bytes(filter, in.data, in.size);

        GST_DEBUG("finished decompressing buffer, %ld bytes remaining", filter->dec_buf_size);
    }

    gst_buffer_unmap(outbuf, &in);
    return gzdec_get_flow_return(filter, ret);
}

// called after receiving EOS event
// drain our internal buffer
static void
gzdec_push_drain(Gstgzdec * filter) {
    if(filter->dec_buf_size){
        GST_INFO("drain decoded bytes %ld", filter->dec_buf_size);
        GstBuffer* buff = gst_buffer_new_allocate(NULL, filter->dec_buf_size, NULL);
        GstMapInfo in;
        if( !gst_buffer_map(buff, &in, GST_MAP_WRITE) ) {
            GST_ERROR("FAILED TO MAP BUFFER INFO");
        }

        memmove(in.data, filter->dec_buf, filter->dec_buf_size);

        gst_buffer_unmap(buff, &in);
        gst_buffer_set_size(buff, filter->dec_buf_size);

        if( gst_pad_push(GST_BASE_TRANSFORM_SRC_PAD(filter), buff) == GST_FLOW_ERROR ){
            GST_ERROR("Failed to push buffer to src pad!");
        }
    }
    // now that we drained all data, let's free the pointer
    if(filter->dec_buf) {
        GST_DEBUG("freeing buffer");
        g_free(filter->dec_buf);
    }
    GST_DEBUG("end filters");
    //(void)BZ2_bzCompressEnd(&filter->bstrm);
    (void)inflateEnd(&filter->zstrm);
}

// helper function to map the return of decompress to GstFlowReturn
// @param filter: the gzdec filter
// @param ret: return of decompress
static GstFlowReturn
gzdec_get_flow_return(Gstgzdec * filter, int ret)
{
    switch (filter->decoder_type)
    {
    case GZIP:
    {
        switch (ret)
        {
        case Z_OK:
        case Z_STREAM_END:
            return GST_FLOW_OK;
        default:
            return GST_FLOW_ERROR;
        }
    }
    case BZIP:
    {
        switch (ret)
        {
        case BZ_OK:
        case BZ_STREAM_END:
            return GST_FLOW_OK;
        default:
            return GST_FLOW_ERROR;
        }
    }
    default:
        return GST_FLOW_OK;
    }
}

// send "size" bytes downstream
// and reallocate our current buffer
// @param filter: filter itself
// @param data: downstream buffer
// @param size: num of bytes to be sent
static void
gzdec_send_decoded_bytes(Gstgzdec * filter, guint8 * data, gsize size){

    GST_DEBUG("sending %ld bytes", size);
    // copy "in.size" bytes from our buffer of decoded bytes
    memcpy(data, filter->dec_buf, size);

    GST_DEBUG("buf_size %ld -> %ld", filter->dec_buf_size, filter->dec_buf_size - size);
    filter->dec_buf_size -= size;
    // replace the first "size" bytes, a.k.a clear it
    filter->dec_buf = memmove(filter->dec_buf, filter->dec_buf+size, filter->dec_buf_size);
}

// store decompressed bytes for future flush/send
// @param filter: filter itself
// @param data: new buffer fetched after decompression process
// @param decoded_bytes: number of bytes that were decoded
static void
gzdec_store_decoded_bytes(Gstgzdec* filter, guint8* decoded, gsize decoded_bytes){

    GST_DEBUG("storing %ld decompressed bytes", decoded_bytes);

    if(!filter->dec_buf) {
        // allocate a buffer
        GST_DEBUG("allocating %ld bytes", decoded_bytes);
        filter->dec_buf = g_malloc(decoded_bytes);
    }else {
        // reallocate with new size
        GST_DEBUG("realloc %ld -> %ld bytes", filter->dec_buf_size, filter->dec_buf_size + decoded_bytes);
        filter->dec_buf = g_realloc(filter->dec_buf, filter->dec_buf_size+decoded_bytes);
    }

    // move the remaining bytes to our buffer
    memmove(filter->dec_buf + filter->dec_buf_size, decoded, decoded_bytes);
    filter->dec_buf_size += decoded_bytes;
}

// do GZIP decompression
// and saves the decoded bytes for later send
// @param filter: filter itself
// @param data: input buffer
// @param size: input buffer size
static int
gzdec_gzip_decompress(Gstgzdec* filter, guint8* input, gsize insize)
{
    z_streamp strm = &filter->zstrm;

    strm->avail_in = (uInt) insize;
    GST_DEBUG("gzip decompressing %ld bytes", insize);

    int ret = 0;
    int input_idx = 0;
    {

        strm->next_in = input+input_idx;
        
        GST_DEBUG("strm->avail_in %d", strm->avail_in);
        Bytef output[CHUNK];
        gsize decoded_bytes = 0;
        strm->next_out = (Bytef*) output;
        strm->avail_out = (uInt) CHUNK;

        int ret = inflate(strm, Z_NO_FLUSH);
        // check for return errors
        switch (ret) {
        case Z_NEED_DICT:
            ret = Z_DATA_ERROR;
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
            GST_ERROR("inflate failed with %s", zError(ret));
            (void)inflateEnd(strm);
            return ret;
        }

        // strm.avail_out indicates how much free space we have in the outbuffer
        // so CHUNK minus strm.avail.out equals to the amount of data that was decompressed in this "turn"
        decoded_bytes = CHUNK - strm->avail_out;

        // save decompressed data to be later sent downstream
        if(decoded_bytes) gzdec_store_decoded_bytes(filter, output, decoded_bytes);
        input_idx = insize - strm->avail_in;

    }while(ret != Z_STREAM_END && strm->avail_in > 0);

    GST_DEBUG("inflate ret(%d):\"%s\"", ret, zError(ret));

    return ret;
}

// do BZIP decompression
// and saves the decoded bytes for later send
// @param filter: filter itself
// @param data: input buffer
// @param size: input buffer size
static int
gzdec_bzip_decompress(Gstgzdec* filter, guint8* input, gsize insize)
{
    bz_stream * strm = &filter->bstrm;
    GST_DEBUG("bzip decompressing %ld bytes", insize);

    int ret = 0;
    int consumed = 0;
    strm->avail_in = (uInt) insize;

    char output[CHUNK];

    do{
        strm->next_in = input+consumed;

        memset(output, 0, CHUNK);

        strm->next_out = (char*) output;
        strm->avail_out = CHUNK;

        char * prev = output;
        ret = BZ2_bzDecompress(strm);
        // check for return errors
        switch (ret) {
        case BZ_PARAM_ERROR:
        case BZ_DATA_ERROR:
            GST_ERROR("decompress failed with %d", ret);
            (void)BZ2_bzCompressEnd(strm);
            return ret;
        }

        gsize decoded_bytes = strm->avail_out - (16*1024);
        GST_DEBUG("dec bytes:%ld", decoded_bytes);
        // save decompressed data to be later sent downstream
        gzdec_store_decoded_bytes(filter, output, decoded_bytes);

        consumed = insize - strm->avail_in;
        GST_DEBUG("decompress ret:%d", ret);

    }while(ret != BZ_STREAM_END && strm->avail_in > 0);
    GST_DEBUG("decompress ret:%d final", ret);

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
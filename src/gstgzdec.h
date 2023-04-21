/* 
 * GStreamer
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
 
#ifndef __GST_GZDEC_H__
#define __GST_GZDEC_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <zlib.h>
#include <bzlib.h>

G_BEGIN_DECLS

#define DECOMPRESS_FUN(name) int(*name)(Gstgzdec*, guint8*, gsize)

#define CHUNK 262144 // 256k

// Two bytes magic numbers
// to decide compression method
enum
{
    NONE = 0,
    GZIP = 0x1F | 0x8B,
    BZIP = 'B' | 'Z',
};

#define GST_TYPE_GZDEC (gst_gzdec_get_type())
G_DECLARE_FINAL_TYPE (Gstgzdec, gst_gzdec,
    GST, GZDEC, GstBaseTransform)

struct _Gstgzdec {
    GstBaseTransform element;

    guint decoder_type;

    z_stream  zstrm;
    bz_stream bstrm;

    // buffer holding decoded data
    gpointer dec_buf;
    // buffer size
    gsize    dec_buf_size;
};

G_END_DECLS

#endif /* __GST_GZDEC_H__ */

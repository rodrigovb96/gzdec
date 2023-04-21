# GzDec - A Gstreamer Filter/Decompresser Plugin

As my Multimedia Engineer Assignment for Fluendo.

I made this Gstreamer plugin that decompresses gzip and bzip streams.

The plugin decides dynamically which type of the stream he is going to decode given
the 2byte-long magic number in the buffer header.
  * 1F 8b for gzip. https://en.wikipedia.org/wiki/Gzip
  * 'B' 'Z' for bzip. https://en.wikipedia.org/wiki/Bzip2

and then after that, decompresses the stream and send it downstream, if the data/buffer does not
match any of this two cases, the plugin assumes that this data is not compressed, and only act as
a pass-through.

it uses the "transform_ip" method that transforms the buffer in-place, given that the buffer size should be kept,
so the plugin decode how much it can in this "turn" and then send N bytes downstream, if the buffer size is smaller then
the quantity of bytes decoded, the plugin keep the decoded bytes in a internal buffer to be sent after, in other "turns" or
in EOS event, draining it downstream.

there are different ways of doing this process, using "transform_ip" was a programmer choice.

Decode Flow:
```
[Buffer(X bytes)] ---> [GZDEC] -- <Is first buffer?>---No--->[Decode] -->[Append N Bytes(internal buff)] ---> [Send first X bytes downstream]
                                        | yes                /\
                              [Decides the format] __________/
```
The internal buffer here behaves like a FIFO.

# System Requirements

This project requires:

* Zlib
* gstreamer-1.0
* gstreamer-base-1.0

To be installed in the system, and it won't build otherwise


# Building the project

```bash
./autogen.sh && \
./configure && \
make
```

## TODO & Caveats

here is a list of somethings that the I would want to add to this project:

1. Unit testing
    this was not possible to be done in the development time given the time that
    I took to understand the decompression libraries, and also doing refinement

2. Bzip decompression seems quite off
    even though reading and using the bzip2 manual the decompressed size seems to be wrong,
    ``` CHUNK - strm->avail_out ``` is a lot bigger than expected, adding garbage to the result.
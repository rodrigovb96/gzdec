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

# Building the project

```bash
./autogen.sh && \
./configure && \
make
```
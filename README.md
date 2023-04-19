# GzDec - A Gstreamer Filter/Decompresser Plugin

As my Multimedia Engineer Assignment for Fluendo.

I made this Gstreamer plugin that decompresses gzip and bzip streams.

The plugin decides dynamically which type of the stream he is going to decode given
the 2byte-long magic number in the buffer header.
  * 1F 8b for gzip. https://en.wikipedia.org/wiki/Gzip
  * 'B' 'Z' for bzip. https://en.wikipedia.org/wiki/Bzip2

and then after that, decompresses the stream and send it upstream, if the data/buffer does not
match any of this two cases, the plugin assumes that this data is not compressed, and only act as
a pass-through.

# Building the project
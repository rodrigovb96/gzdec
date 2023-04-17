#!/bin/bash

# clean previous generated files(if any) and build
echo "Removing previous autogen files:" && \
    git clean -dfX && \
    autoreconf --verbose --instal
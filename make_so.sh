#!/bin/bash

# You'll probably need to mess with some paths in this... since it references my home directory...
# Builds some SOs out of the archives for the various things fitz depends on.

make_so() {
	i686-pc-toaru-gcc -shared -Wl,-soname,lib$1.so -o lib$1.so -L. -L/home/klange/Projects/toaruos-ldiso/hdd/usr/lib -Wl,--whole-archive $2 -Wl,--no-whole-archive $3 -lc -lgcc
}

make_so jpeg /home/klange/Projects/third-party/mupdf-1.1-source/build/debug/libjpeg.a ""
make_so openjpeg /home/klange/Projects/third-party/mupdf-1.1-source/build/debug/libopenjpeg.a ""
make_so jbig2dec /home/klange/Projects/third-party/mupdf-1.1-source/build/debug/libjbig2dec.a ""
make_so fitz /home/klange/Projects/third-party/mupdf-1.1-source/build/debug/libfitz.a "-lfreetype -lpng15 -lz -ljbig2dec -lopenjpeg -ljpeg"

CC = i686-pc-toaru-gcc
CFLAGS = -std=c99 -U__STRICT_ANSI__ -O3 -m32 -Wa,--32 -L.
CPPFLAGS = -O3 -m32 -Wa,--32
EXTRAFLAGS = -s -I /home/klange/Projects/third-party/mupdf-1.1-source/fitz
EXECUTABLES = $(TARGETDIR)pdfviewer

BEG = ../util/mk-beg
END = ../util/mk-end
INFO = ../util/mk-info
ERRORS = 2>>/tmp/.`whoami`-build-errors || ../util/mk-error
ERRORSS = >>/tmp/.`whoami`-build-errors || ../util/mk-error

BEGRM = ../util/mk-beg-rm
ENDRM = ../util/mk-end-rm

FREETYPE_INC = -I ${TOOLCHAIN}/include/freetype2/
FREETYPE_LIB = -lfreetype
LIBPNG = -lpng15
LIBM = -lm
LIBZ = -lz

ljpeg = -ljpeg

TARGETDIR = ../hdd/bin/

LOCAL_LIBS = -ltoaru-decorations -ltoaru-yutani -ltoaru-shmemfonts -ltoaru-graphics -ltoaru-dlfcn -ltoaru-hashmap -ltoaru-pex -ltoaru-list -ltoaru-kbd
LOCAL_INC  = -I ../userspace/

.PHONY: all clean

all: ${EXECUTABLES}

clean:
	@-rm -f ${EXECUTABLES}

libmemalign.so: memalign.c
	@${CC} -fPIC -shared -Wl,-soname,libmemalign.so -o libmemalign.so memalign.c -lgcc -lc

$(TARGETDIR)pdfviewer: $(TARGETDIR)% : %.c | libmemalign.so
	@${BEG} "CC" "$@ $< [w/libs]"
	@${CC} ${CFLAGS} ${EXTRAFLAGS} ${FREETYPE_INC} ${LOCAL_INC} -o $@ $< ${LOCAL_LIBS} -lfitz  ${FREETYPE_LIB} ${LIBPNG} -ljbig2dec -lopenjpeg ${ljpeg} ${LIBZ} ${LIBM} -lmemalign ${ERRORS}
	@${END} "CC" "$< [w/libs]"


CC = i686-pc-toaru-gcc
CFLAGS = -std=c99 -U__STRICT_ANSI__ -O3 -m32 -Wa,--32
CPPFLAGS = -O3 -m32 -Wa,--32
EXTRAFLAGS = -s
EXECUTABLES = $(patsubst %.c,../hdd/bin/%,$(wildcard *.c))

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

ljpeg = /home/klange/Projects/third-party/mupdf-1.1-source/build/debug/libjpeg.a

TARGETDIR = ../hdd/bin/

LOCAL_LIBS = -ltoaru-decorations -ltoaru-yutani -ltoaru-shmemfonts -ltoaru-graphics -ltoaru-dlfcn -ltoaru-hashmap -ltoaru-pex -ltoaru-list -ltoaru-kbd
LOCAL_INC  = -I ../userspace/

.PHONY: all clean

all: ${EXECUTABLES}

clean:
	@-rm -f ${EXECUTABLES}

$(TARGETDIR)pdfviewer: $(TARGETDIR)% : %.c
	@${BEG} "CC" "$@ $< [w/libs]"
	@${CC} ${CFLAGS} ${EXTRAFLAGS} ${FREETYPE_INC} ${LOCAL_INC} -o $@ $< ${LOCAL_LIBS} ${FREETYPE_LIB} ${LIBPNG} -lfitz -ljbig2dec -lopenjpeg ${ljpeg} ${LIBZ} ${LIBM} ${ERRORS}
	@${END} "CC" "$< [w/libs]"


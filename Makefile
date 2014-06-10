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
FREETYPE_LIB = ${TOOLCHAIN}/lib/libfreetype.a
LIBPNG = ${TOOLCHAIN}/lib/libpng.a
LIBM = ${TOOLCHAIN}/lib/libm.a
LIBZ = ${TOOLCHAIN}/lib/libz.a

TARGETDIR = ../hdd/bin/

LOCAL_LIBS = $(patsubst %.c,%.o,$(wildcard ../userspace/lib/*.c))
LOCAL_INC  = -I ../userspace/

.PHONY: all clean

all: ${EXECUTABLES}

clean:
	@-rm -f ${EXECUTABLES}

$(TARGETDIR)pdfviewer: $(TARGETDIR)% : %.c ${LOCAL_LIBS}
	@${BEG} "CC" "$@ $< [w/libs]"
	@${CC} -flto ${CFLAGS} ${EXTRAFLAGS} ${FREETYPE_INC} ${LOCAL_INC} -o $@ $< ${LOCAL_LIBS} ${LIBM} ${FREETYPE_LIB} ${LIBPNG} -lfitz -ljbig2dec -lopenjpeg -ljpeg ${LIBZ} ${ERRORS}
	@${END} "CC" "$< [w/libs]"


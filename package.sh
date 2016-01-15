ZIPNAME=${1:?}

zip "ww2ogg$ZIPNAME.zip" \
  src/Bit_stream.h \
  src/codebook.cpp \
  src/codebook.h \
  src/crc.c \
  src/crc.h \
  src/errors.h \
  src/ww2ogg.cpp \
  src/wwriff.cpp \
  src/wwriff.h \
  CHANGELOG \
  COPYING \
  Makefile \
  Makefile.common \
  Makefile.mingw \
  notes.md \
  packed_codebooks.bin \
  packed_codebooks_aoTuV_603.bin \
  package.sh \
  README.md \
  ww2ogg.exe

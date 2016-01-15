ZIPNAME=${1:?}

zip "ww2ogg$ZIPNAME.zip" src/Bit_stream.h src/codebook.cpp src/codebook.h COPYING src/crc.c src/crc.h src/errors.h CHANGELOG Makefile Makefile.common Makefile.mingw packed_codebooks.bin packed_codebooks_aoTuV_603.bin src/ww2ogg.cpp ww2ogg.exe src/wwriff.cpp src/wwriff.h package.sh notes.md README

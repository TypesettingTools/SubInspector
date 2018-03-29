#!/usr/bin/env sh

cd "`git rev-parse --show-toplevel`"

PREFIX=`pwd`/deps/build
export CC=clang
export CXX=clang++
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:/usr/local/opt/icu4c/lib/pkgconfig"

pushd deps/fribidi && \
./autogen.sh --enable-static --disable-shared --with-glib=no --prefix=$PREFIX && \
make && make install
popd

pushd deps/harfbuzz && \
./autogen.sh && \
LDFLAGS='-L/usr/local/opt/icu4c/lib' CFLAGS='-I/usr/local/opt/icu4c/include' PKG_CONFIG_PATH='/usr/local/opt/icu4c/lib/pkgconfig' ./configure --enable-static --disable-shared --without-glib --without-fontconfig --without-cairo --with-icu --without-graphite2 --without-uniscribe --prefix=$PREFIX && \
make && make install
popd

pushd deps/freetype2 && \
./autogen.sh && \
./configure --enable-static --disable-shared --prefix=$PREFIX && \
make && make install
popd

pushd deps/fontconfig && \
LIBTOOLIZE=glibtoolize ./autogen.sh --enable-static --disable-shared --prefix=$PREFIX && \
make && make install
popd

pushd deps/libass && \
git apply ../patches/0001-Don-t-use-approximation-in-rasterizer.patch && \
./autogen.sh && \
./configure --enable-static --disable-shared --disable-enca --disable-fontconfig --prefix=$PREFIX && \
make && make install
popd

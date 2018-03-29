#!/bin/sh
cd `git rev-parse --show-toplevel`

pushd deps/fribidi
git clean -fdx
popd
pushd deps/harfbuzz
git clean -fdx
popd
pushd deps/freetype2
git clean -fdx
popd
pushd deps/fontconfig
git clean -fdx
popd
pushd deps/libass
git clean -fdx
popd

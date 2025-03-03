#!/bin/bash
set -e

PROJECT_DIR=$(realpath `dirname ${BASH_SOURCE[0]}`/..)
source "${PROJECT_DIR}/toolchain/common.sh"

#----------------------------------

readonly ZLIB_VERSION=1.2.13
readonly LIBPNG_VERSION=1.6.37
readonly FREETYPE_VERSION=2.10.0
readonly HARFBUZZ_VERSION=2.9.1


# build zlib
# ==========
# args:
#   <1>: target arch (i.e. 'x86_64')
#
# env:
#   ZLIB_BUILD_DIR   - build directory (default = <BUILD_DIR>/system/zlib)
#   ZLIB_INSTALL_DIR - install directory (default = <SYS_ROOT>/usr)
#   ZLIB_SYSROOT     - sysroot override (default = <SYS_ROOT>)
#
# example:
#   toolchain::zlib::build
toolchain::zlib::build() {
  toolchain::util::check_args 1 $@

  local arch="$1"
  shift 1
  case "$arch" in
    x86_64* | X64)
      arch="x86_64"
      ;;
    *)
      toolchain::util::error "unsupported arch: $arch"
      exit 1
      ;;
  esac

  local version="${ZLIB_VERSION}"
  local build_dir=${ZLIB_BUILD_DIR:-${BUILD_DIR}/system/zlib}
  local install_dir=${ZLIB_INSTALL_DIR:-${SYS_ROOT}/usr}
  local sysroot="${ZLIB_SYSROOT:-${SYS_ROOT}}"
  local tool_prefix="${sysroot}/usr/bin/${arch}-osdev-"
  local libdir="${sysroot}/usr/lib"
  local includedir="${sysroot}/usr/include"

  mkdir -p ${build_dir}
  pushd ${build_dir}
    if [ ! -d src ]; then
      mkdir -p src

      wget -nc https://github.com/madler/zlib/releases/download/v${version}/zlib-${version}.tar.xz
      tar -xf zlib-${version}.tar.xz -C src --strip-components=1
    fi

    mkdir -p build
    pushd build
      export CHOST="${arch}"
      export CC="${tool_prefix}gcc"
      export AR="${tool_prefix}gcc-ar"
      export RANLIB="${tool_prefix}gcc-ranlib"

      ../src/configure --prefix=${install_dir} --libdir=${libdir} --includedir=${includedir} --static
      rm -f libz.a
      make
      make install
    popd
  popd
}

#
# main
#

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  if [[ $# -eq 0 ]]; then
    exit 1
  fi

  command="$1"
  shift

  case "${command}" in
    build-zlib)
      toolchain::zlib::build $@
      ;;
    *)
      toolchain::util::error "unsupported command '${command}'"
      exit 1
      ;;
  esac
fi

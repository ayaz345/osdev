# Common functions used across toolchain scripts
#
# Environment variables:
#   PROJECT_DIR    - absolute path of the main project directory
#   BUILD_DIR      - absolute path of the build directory
#   SYS_ROOT       - system root for the os disk image
#
#   NPROC          - number of parallel jobs to use for make
#   SKIP_CONFIGURE - skip configure step if set
#   SKIP_BUILD     - skip build step if set
#   SKIP_INSTALL   - skip install step if set

BUILD_DIR=${BUILD_DIR:-${PROJECT_DIR}/build}
SYS_ROOT=${SYS_ROOT:-${BUILD_DIR}/sysroot}
NPROC=${NPROC:-$(nproc)}
MAKE="${MAKE:-make}"
MAKE_j="${MAKE:-make} -j${NPROC}"

# colors
CYAN="\033[36m"
RESET="\033[0m"

# toolchain::util::check_args
# args:
#   <1>: number required arguments
#   <2...N>: arguments to check
toolchain::util::check_args() {
  local context="${FUNCNAME[1]}"
  local n="$1"
  shift

  if [[ ${FUNCNAME[1]} == "main" ]]; then
    context="${BASH_SOURCE[1]}"
  fi

  count=0
  for arg in "$@"; do
    if [[ ( ${count} -le ${n} ) && ( -z "${arg}" ) ]]; then
      echo "${context}: argument $((count)) is empty" >&2
      exit 1
    fi
    ((count=count+1))
  done

  if [[ ${count} -lt ${n} ]]; then
    echo "${context}: expected ${n} argument(s), got $count" >&2
    exit 1
  fi
}

# toolchain::util::error
# args:
#   <1>: message
toolchain::util::error() {
  local context="${FUNCNAME[1]}"
  if [[ ${FUNCNAME[1]} == "main" ]]; then
    context="${BASH_SOURCE[1]}"
  fi
  echo "${context}: error: $1" >&2
}

# toolchain::util::symlink
# args:
#   <2>: src file
#   <1>: dest file
toolchain::util::symlink() {
  toolchain::util::check_args 2 $@

  local srcfile="$1"
  local destfile="$2"
  shift 2

  local destdir=$(dirname ${destfile})
  local destname=$(basename ${destfile})
  mkdir -p ${destdir}
  link=$(realpath --relative-to=${destdir} ${srcfile})
  ln -sf ${link} ${destdir}/${destname}
}


# toolchain::util::configure
# args:
#   <1>: configure path
#   <2..N>: options
toolchain::util::configure() {
  local configure="$1"
  shift

  if [[ -n ${SKIP_CONFIGURE} ]]; then
    echo "skipping configure"
    return 0
  fi

  ${configure} $@ ${COMMON_OPTIONS}
}

# toolchain::util::make_build
# args:
#   <1..N>: arguments
toolchain::util::make_build() {
  if [[ -n ${SKIP_BUILD} ]]; then
    echo "skipping build"
    return 0
  fi

  make -j${NPROC} $@
}

# toolchain::util::make_install
# args:
#   <1>: target
toolchain::util::make_install() {
  if [[ -n ${SKIP_INSTALL} ]]; then
    echo "skipping install"
    return 0
  fi

  make ${1:-install}
}


# toolchain::util::configure_step
# args:
#   <1..N>: args
toolchain::util::configure_step() {
  if [ "${SKIP_CONFIGURE}" == "1" ]; then
    echo "skipping configure"
    return 0
  fi
  $@
}

# toolchain::util::build_step
# args:
#   <1..N>: args
toolchain::util::build_step() {
  if [ "${SKIP_BUILD}" == "1" ]; then
    echo "skipping building"
    return 0
  fi
  $@
}

# toolchain::util::install_step
# args:
#   <1..N>: args
toolchain::util::install_step() {
  if [ "${SKIP_INSTALL}" == "1" ]; then
    echo "skipping install"
    return 0
  fi
  $@
}


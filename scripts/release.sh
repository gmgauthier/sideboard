#!/bin/sh
# Build release artifacts: tarball and .deb (no AppImage).
# Usage: ./scripts/release.sh [tarball|deb|all]

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

VERSION=$(sed -n "s/^  version: '\\(.*\\)',/\\1/p" meson.build | head -1)
DISTDIR="${ROOT}/dist"
JOB=${1:-all}

mkdir -p "$DISTDIR"

need_build() {
  if [ ! -f "${ROOT}/build/build.ninja" ]; then
    meson setup "${ROOT}/build" "$ROOT"
  fi
}

do_tarball() {
  need_build
  meson dist -C "${ROOT}/build" --no-tests --allow-dirty
  src="${ROOT}/build/meson-dist/sideboard-${VERSION}.tar.xz"
  if [ -f "$src" ]; then
    cp -f "$src" "$DISTDIR/"
    echo "tarball: ${DISTDIR}/sideboard-${VERSION}.tar.xz"
  else
    echo "meson dist did not produce sideboard-${VERSION}.tar.xz" >&2
    ls -la "${ROOT}/build/meson-dist" >&2 || true
    exit 1
  fi
}

do_deb() {
  dpkg-buildpackage -us -uc -b --no-sign
  mkdir -p "$DISTDIR"
  for f in "${ROOT}/../sideboard_${VERSION}"-*.deb \
           "${ROOT}/../sideboard-dbgsym_${VERSION}"-*.deb; do
    [ -e "$f" ] || continue
    mv -f "$f" "$DISTDIR/"
    echo "deb: $DISTDIR/$(basename "$f")"
  done
  for f in "${ROOT}/../sideboard_${VERSION}"-*.buildinfo \
           "${ROOT}/../sideboard_${VERSION}"-*.changes; do
    [ -e "$f" ] || continue
    mv -f "$f" "$DISTDIR/"
  done
}

case "$JOB" in
  tarball) do_tarball ;;
  deb) do_deb ;;
  all)
    do_tarball
    do_deb
    ;;
  *)
    echo "usage: $0 [tarball|deb|all]" >&2
    exit 2
    ;;
esac

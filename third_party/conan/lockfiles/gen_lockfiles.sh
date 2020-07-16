#!/bin/bash

REPO_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/../../../" >/dev/null 2>&1 && pwd )"

cleanup() {
  if [ -n "$CONAN_USER_HOME" -a -d "$CONAN_USER_HOME" ]; then
    rm -rf $CONAN_USER_HOME
  fi
}

trap cleanup EXIT

for os in windows; do
  export CONAN_USER_HOME="$(mktemp -d)"
  $REPO_ROOT/third_party/conan/configs/install.sh --assume-$os || exit $?

  if ! conan profile show default >/dev/null 2>&1; then
    conan profile new --detect default >/dev/null 2>&1 || exit $?
  fi

  if [ "$os" == "linux" ]; then
    build_profile="clang7_release"
  else
    build_profile="msvc2017_release"
  fi

  echo "Build profile:"
  conan profile show $build_profile

  ls -1 $REPO_ROOT/third_party/conan/lockfiles/$os/ | while read profile; do
    conan graph lock "$REPO_ROOT" -pr:b $build_profile -pr:h $profile \
       --build '*' "--lockfile=$CONAN_USER_HOME" || exit $?
    jq --indent 1 'del(.graph_lock.nodes."0".path)' < "$CONAN_USER_HOME/conan.lock" \
      > "$REPO_ROOT/third_party/conan/lockfiles/$os/$profile/conan.lock" || exit $?
  done

  rm -rf "$CONAN_USER_HOME" || exit $?
done

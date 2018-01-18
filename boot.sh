#! /bin/sh
autoreconf --install --force

if [ "$CC" == "clang" ]; then
    clang --version
fi


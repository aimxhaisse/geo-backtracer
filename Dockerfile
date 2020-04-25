# Dockerfile to build the Geo Backtracer on Linux.

# Meta.
FROM archlinux
LABEL description="Container for the Geo Backtracer"

RUN pacman -Sy

# Bt Deps
RUN pacman --noconfirm -S make
RUN pacman --noconfirm -S clang
RUN pacman --noconfirm -S boost-libs
RUN pacman --noconfirm -S google-glog
RUN pacman --noconfirm -S grpc
RUN pacman --noconfirm -S gflags
RUN pacman --noconfirm -S yaml-cpp
RUN pacman --noconfirm -S protobuf

# Rocksdb
RUN pacman --noconfirm -S git
RUN pacman --noconfirm -S snappy
RUN pacman --noconfirm -S zlib
RUN pacman --noconfirm -S bzip2
RUN pacman --noconfirm -S lz4
RUN pacman --noconfirm -S zstd
RUN pacman --noconfirm -S rocksdb

# Unit tests
RUN pacman --noconfirm -S cmake
RUN pacman --noconfirm -S gtest
RUN pacman --noconfirm -S libffi
RUN pacman --noconfirm -S which
RUN pacman --noconfirm -S boost

# Build Bt
ADD . /build
RUN cd /build && make all
RUN cd /build && make test

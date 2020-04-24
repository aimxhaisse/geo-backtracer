# Dockerfile to build the Geo Backtracer on Linux.

# Meta.
FROM ubuntu
LABEL description="Container for the Geo Backtracer"

RUN apt update

# Bt Deps
RUN apt install -y make
RUN apt install -y clang
RUN apt install -y libboost-all-dev
RUN apt install -y libgoogle-glog-dev
RUN apt install -y libgrpc-dev
RUN apt install -y clang-format
RUN apt install -y libgflags-dev
RUN apt install -y libyaml-cpp-dev
RUN apt install -y libprotobuf-dev
RUN apt install -y protobuf-compiler
RUN apt install -y protobuf-compiler-grpc
RUN apt install -y libgrpc++-dev

# Rocksdb Deps
RUN apt install -y git
RUN apt install -y libsnappy-dev
RUN apt install -y zlib1g-dev
RUN apt install -y libbz2-dev
RUN apt install -y liblz4-dev
RUN apt install -y libzstd-dev

# Compile & install custom rocksdb
RUN git clone https://github.com/facebook/rocksdb.git
RUN cd rocksdb && git checkout tags/v6.7.3
RUN cd rocksdb && make static_lib
RUN cp rocksdb/librocksdb.a /usr/local/lib/
RUN cp -r rocksdb/include/rocksdb /usr/local/include/

# Unit tests
RUN apt install -y cmake
RUN apt install -y googletest
RUN apt install -y libgtest-dev
RUN cd /usr/src/gtest && cmake CMakeLists.txt && make && cp *.a /usr/local/lib

# Build Bt
ADD . /build
RUN cd /build && make all test

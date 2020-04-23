# Dockerfile to build the Geo Backtracer for Unices.

FROM ubuntu
LABEL description="Container for the Geo Backtracer"
RUN apt update

RUN apt install -y make
RUN apt install -y clang
RUN apt install -y libboost-all-dev
RUN apt install -y libgoogle-glog-dev
RUN apt install -y libgrpc-dev
RUN apt install -y clang-format
RUN apt install -y libgflags-dev
RUN apt install -y librocksdb-dev
RUN apt install -y libyaml-cpp-dev
RUN apt install -y libprotobuf-dev
RUN apt install -y protobuf-compiler
RUN apt install -y protobuf-compiler-grpc
RUN apt install -y libgrpc++-dev

ADD . /build

RUN cd /build && make all test

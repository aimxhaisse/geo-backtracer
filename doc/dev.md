# Development

Development environment for now is on Mac OS, but the project compiles
and runs on Linux as well (see Dockerfile):

Install the following dependencies:

    brew install boost
    brew install clang-format
    brew install gflags
    brew install glog
    brew install grpc
    brew install protobuf
    brew install rocksdb
    brew install yaml-cpp
    brew install --HEAD https://gist.githubusercontent.com/Kronuz/96ac10fbd8472eb1e7566d740c4034f8/raw/gtest.rb

Once done, run the Makefile to get more help about available commands:

    $ make
    Help for Covid Backtracer:

    This is not production ready, some commands here are destructive.

    make            # this message
    make all        # build everything
    make test       # run unit tests
    make clean      # clean all build artifacts
    make re         # rebuild covid backtracer
    make server     # run a local instance of covid backtracer
    make client     # inject fixtures into local instance

As unit tests simulate clusters with multiple databases and gRPC
stubs, they can fail due to hitting limits on file descriptors
(default 256 on common setups), to increase the limit:

    ulimit -n 102

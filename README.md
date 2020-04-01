# Covid Backtrace

A service to correlate GPS user locations and backtrace the list of
potential Covid contaminations, given a specific contaminated user.

## Status

This is a work in progress.

## Development

Install the following dependencies:

    brew install boost
    brew install clang-format
    brew install gflags
    brew install glog
    brew install rocksdb
    brew install --HEAD https://gist.githubusercontent.com/Kronuz/96ac10fbd8472eb1e7566d740c4034f8/raw/gtest.rb

Once done, run the Makefile to get more help about available commands:

    make

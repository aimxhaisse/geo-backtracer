# Covid Backtrace

A service to correlate GPS user locations and backtrace the list of
potential Covid contaminations, given a specific contaminated user.

## Development Environment

    # install a few deps
    brew install clang-format
    brew install gflags
    brew install glog

    # (optional) install googletest
    brew install --HEAD https://gist.githubusercontent.com/Kronuz/96ac10fbd8472eb1e7566d740c4034f8/raw/gtest.rb

    # build
    make

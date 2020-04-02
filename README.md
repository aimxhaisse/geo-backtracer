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
    brew install grpc
    brew install protobuf
    brew install rocksdb
    brew install --HEAD https://gist.githubusercontent.com/Kronuz/96ac10fbd8472eb1e7566d740c4034f8/raw/gtest.rb

Once done, run the Makefile to get more help about available commands:

    make

## Ramblings & Quick Maths

This is just ballparking, numbers come from a macbook pro, writing to
flash, both clients and server are running on the same system. This is
still a work-in-progress and might evolve in many directions.

- inserting 50.000.000 with 5 clients takes 100 seconds, so insert rate is 500.000 points/sec,
- 50.000.000 take about 1.3G on flash, so average point size after compression is 27 bytes,
- assuming 1 point per-user every minute, insert currently supports 30 million active users.

If we aim for 90 days of horizon & a database of 1 TiB, it means we
can store about 45.000.000.000 points, that's 1 point per minute per
user for 90 days is 130.000 points, so in terms of size we'd scale to
350.000 users, this is the current bottleneck.

Things that will make these numbers lower:

- having two column families to support fast per-user look-ups,
- having read queries running at the same time,
- having compactions running in the background.

Things that will make these numbers higher:

- hosting on a real server,
- quantize time to store less points,
- tweaking the database for this real server,
- increasing the number of concurrent clients,
- multiple-values for a given period (i.e 1 point every hour instead of 60 points).

Before going into this land, let's handle reads as well so that we get
an idea of the time it takes to fetch the backtrace, and how hard the
algorithm is.

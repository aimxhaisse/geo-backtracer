# Geo Backtracer

A service to store GPS-like locations of O(million) users in
real-time, and provide a way to backtrace over a period of 30 days,
users that were close for more than 15 minutes. This is just one brick
that can be coupled with backends aggregating points from mobile
phones; it is meant to be scalable and solve this single problem.

## Status

This is a work in progress, at the moment, this service can handle
about 1 million active users sending 1 GPS point every minute.
Horizontal scaling will increase this limit by orders of magnitude.

## Features

The current features are implemented:

- API to push GPS points from users,
- API to fetch the timeline of a user,
- API to delete all data of a user (GDPR),
- API to seek correlations at scale,
- Garbage collection of points older than 14 days (can be configured).

## Contributors

![Protocol Labs](https://github.com/aimxhaisse/geo-backtracer/raw/master/assets/protocol-labs.png "Protocol Labs Logo")

This project is backed by [Protocol Labs](https://protocol.ai) as part
of the [COVID-19 Open Innovation Grants](https://research.protocol.ai/posts/202003-covid-grants/).

## How it works

The Geo Backtracer stores GPS points from user identifiers and, given
an identifier, is able to compute identifiers that were in a close
distance for more than 15 minutes in the last 14 days. To do so at a
large scale, it uses an internal database layout that optimizes
heavily to solve this single problem:

- high throughput is achieved with little database compaction as data
is written in a sorted fashion (so that writes are sequential),

- reads for the solver are localised by reading blocks containing all
information needed to compute correlations within a zone of 100x100
meters over 1000 seconds; the algorithm computes the distance between
all users in the area and outputs strong correlations.

## Development

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


## Integration with mobile data

One requirement for this approach to be effective is to have aligned
points as input: i.e: have mobile applications emit GPS points during
a narrow window of time (for instance, at second 30 of every minute).
This makes it possible to handle complex situations (i.e: a car
following a bus for instance) without having to take into account
directions and speed of users.

Typically, in pseudo-code, here is how a mobile app could implement
such an approach:

    kSendRateSeconds = 60

    while (true) {
	  # Get position at second 0 of the minute, all phones are
	  # calibrated on time with GPS data, so they have a somewhat
	  # consistent view of time. This means they can all get a point at
	  # second 0, in a synchronized way, with little effort.
	  #
	  # This makes correlation easy: we can assume that two points
	  # that are close were close at the same time and then movement
	  # doesn't matter: we don't need to compute speed or direction.
	  current_time_ms = now()
	  padded_time_ms = (current_time_ms / (1000 * kSendRateSeconds)) * (1000 * kSendRateSeconds)
	  sleep_ms(current_time_ms - padded_time_ms)

	  # Send GPS position.
	  current_position = get_position()
	  send_position(current_position)

	  # Move a bit the clock, so that we pick the next cycle.
	  sleep_ms((kSendRateSeconds / 2) * 1000)
    }

Ideally, a phone that is not moving should not be sending points, but
could be sending instead: here was my position for the last 10 hours ;
the API doesn't support this yet, but this is an improvement to be
done. This will reduce drastically the overall size of the database,
the above mentioned approach would need to be slightly adapted.

## Ramblings

### Assumptions

   - 30 days of history,
   - 1 point per user per minute.

### Status

Current numbers, inserting 200 000 000 points in an empty database:

   - 80 000 inserts / second,
   - 12.5 byte per GPS point.

### Target Insert rate

    |--------------+----------------|
    | active users | inserts/second |
    |--------------+----------------|
    | 1K users     | 16             |
    |--------------+----------------|
    | 10K users    | 160            |
    |--------------+----------------|
    | 100K users   | 1.6K           |
    |--------------+----------------|
    | 1M users     | 16K            |
    |--------------+----------------|
    | 10M users    | 160K           |
    |--------------+----------------|
    | 100M users   | 1.6M           |
    |--------------+----------------|

### Target Size

    |------------+-----------------+------------------+-------------------|
    | point size | 1 million users | 10 million users | 100 million users |
    |------------+-----------------+------------------+-------------------|
    | 1 byte     | 43G             | 430G             | 4.3T              |
    |------------+-----------------+------------------+-------------------|
    | 2 bytes    | 86G             | 860G             | 8.6T              |
    |------------+-----------------+------------------+-------------------|
    | 3 bytes    | 129G            | 1.3T             | 13T               |
    |------------+-----------------+------------------+-------------------|
    | 4 bytes    | 172G            | 1.7T             | 17T               |
    |------------+-----------------+------------------+-------------------|
    | 5 bytes    | 215G            | 2.2T             | 22T               |
    |------------+-----------------+------------------+-------------------|
    | 10 bytes   | 430G            | 4.3T             | 43T               |
    |------------+-----------------+------------------+-------------------|
    | 20 bytes   | 860G            | 8.6T             | 86T               |
    |------------+-----------------+------------------+-------------------|

# Geo Backtracer

A distributed backend to store GPS-like locations of users in
real-time and to provide a way to backtrace over a period of 15 days,
users that were close for more than 15 minutes. This is one brick that
can be coupled with backends aggregating points from mobile phones for
a COVID-like tracing app; it is meant to be scalable and solve this
single problem.

## Scalability

The Geo Backtracer is built to scale linearly with the number of
users: doubling the number of machines in the cluster handles twice
the load. Simulations with 100 000 to 50 million users were performed
with different cluster configurations to assess the scale at which it
can operate in a production-like environment.

## Components

- [C++ distributed service](https://github.com/aimxhaisse/geo-backtracer/tree/master/server) that stores and correlates GPS points
  (gRPC, RocksDB),
- [C++ client](https://github.com/aimxhaisse/geo-backtracer/tree/master/client) to run simulations of live users and interact with the cluster,
- [Ansible automation](https://github.com/aimxhaisse/geo-backtracer/tree/master/prod) to spin up a new cluster from scratch with
  encrypted traffic between nodes, automatic generation of sharding configurations based on the number of machines, and a few facilities for monitoring.

## Documentation

* [Documentation](https://github.com/aimxhaisse/geo-backtracer/tree/master/doc)
* [Talk at Paris P2P](https://www.youtube.com/watch?v=B-AAEBBkcak) (in French)
* [Slides from Paris P2P talk](https://docs.google.com/presentation/d/1LlakTzJiWInKrA37yrXhYvOEvYq8y4p55oXkuUxHy7w/)

## Contributors

![Protocol Labs](https://github.com/aimxhaisse/geo-backtracer/raw/master/assets/protocol-labs.png "Protocol Labs Logo")

This project is backed by [Protocol Labs](https://protocol.ai) as part
of the [COVID-19 Open Innovation Grants](https://research.protocol.ai/posts/202003-covid-grants/).

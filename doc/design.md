# Geo Bt's Design Notes

## Architecture

The Geo Backtracer is a distributed application composed of two units:

- workers, jobs grouped by shard that store GPS points for a
  configured zone,
- mixers, stateless jobs that are aware of the workers topology and
  route points to the corresponding shard.

![Protocol Labs](https://github.com/aimxhaisse/geo-backtracer/raw/master/doc/architecture.png "Architecture")

## Redundancy

Redundancy is achieved by having at least 2 workers per shard, in this
case, mixers route points to all workers within the shard in a
best-effort fashion, omitting non-operational workers, and only
failing the request when all workers aren't available. By splitting
workers on multiple machines, this allows to perform maintainance
tasks or to handle partial failures in the cluster.

There is no synchronisation between workers, when performing
correlation, the mixer requests data from all workers within the
shard, merges it to remove duplicates and then work on it. This
approach isn't very efficient computationally but reduces the
complexity as there is no need to have a protocol to synchronize
workers, and to reconcile dead spots.

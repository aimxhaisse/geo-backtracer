- [Performances](#performances)
  * [Single Node](#single-node)
    + [Start-2-S-SATA](#start-2-s-sata)
    + [GP1-XS](#gp1-xs)
    + [GP1-S](#gp1-s)
    + [GP1-M](#gp1-m)
    + [GP1-L](#gp1-l)
    + [GP1-XL](#gp1-xl)
    + [GP-BM1-S](#gp-bm1-s)
    + [HC-BM1-S](#hc-bm1-s)
  * [Cluster](#cluster)
    + [Medium-sized cluster with GP1-M machines](#medium-sized-cluster-with-gp1-m-machines)
      - [Configuration](#configuration)
      - [Expectations](#expectations)
      - [Results](#results)
    + [Cluster with GP1-L](#cluster-with-gp1-l)
      - [Configuration](#configuration-1)
      - [Expectations](#expectations-1)
      - [Results](#results-1)


# Performances

## Single Node

This section describes results obtained with a two-nodes cluster with
R=2 redundancy, goal is to assess the right kind of nodes needed for a
realistic cluster setup. QPS expressed here account for R=2 (i.e:
actual GPS point insert rate is twice this number).

### Start-2-S-SATA

- 1× Intel® C2350 (Avoton) - CPU - 2C/2T - 1.7 GHz
- 1 × 1 To SATA
- 4 Go DDR3

Insert rate over 1 hour: **3 100 QPS** per machine (186 000 moving users).

### GP1-XS

- 4 vCPUs
- 16G ram
- 150GB NVMe

Insert rate over 1 hour: **14 150 QPS** per machine (849 000 moving users).

### GP1-S

- 8 vCPUs
- 32G ram
- 300GB NVMe

Insert rate over 1 hour: **21 600 QPS** per machine (1 296 000 moving users).

### GP1-M

- 16 vCPUs
- 64G ram
- 300GB NVMe

Insert rate over 1 hour: **25 200 QPS** per machine (1 512 000 moving users).

### GP1-L

- 32 vCPUs
- 128G ram
- 600GB NVMe

Insert rate over 1 hour: **69 000 QPS** per machine (4 140 000 moving users).

### GP1-XL

- 48 vCPUs
- 256G ram
- 600GB NVMe

Insert rate over 1 hour: **65 000 QPS** per machine (3 900 000 moving users).

### GP-BM1-S

- 4C 8T - 3.7 GHz
- 32G ram
- 2x250GB SSD

Insert rate over 1 hour: **58 000 QPS** per machine (3 480 000 moving users).

### HC-BM1-S

- 20C 40T - 2.2 GHz
- 96G ram
- 2x1TB SSD NVMe

Insert rate over 1 hour: **105 000 QPS** per machine (6 300 000 moving users).

## Cluster

### Medium-sized cluster with GP1-M machines

#### Configuration:

- cluster with 10 machines,
- 8 shards per machine,
- default shard not used,
- redundancy R=2,
- initially 120 clients handling 1M users each, effectively, had to scale it down to 40 clients as we were hitting cluster limits.

#### Expectations

In single-node setup, GPS1-M outputs **25 200** QPS, we expect
with 10 machines to get around **250 000** QPS.

#### Results

After a simulation of 3 hours, we get an average QPS of **240 000
QPS** in the last hour, which aligns with expectations. This confirms
that we linearly scale with a cluster of 10 machines.

Size: on average, 90 bytes per query (**21MB/second** per machine for **240 000 QPS**).

### Cluster with GP1-L

#### Configuration

- cluster with 3 machines,
- 8 shards per machine,
- redundancy R=2,
- garbage collector set to 24h.

#### Expectations

Main goal is to see the behavior of the cluster while the GC is
actively doing passes on filled databases.


#### Results

A few observations after 48
hours of inserts:

- QPS is stable in overall (about 60K QPS for 48 hours),
- GC pass takes about 2 minutes and does not affect QPS,
- 750G of data (375G of points, replication=2),
- database size is controlled with GC,
- load on each machine is about 20 (for a 32 cores VM),
- this experiment shows different performances due to the number of
  shards (we used 16), higher QPS can be achieved by increasing it and
  approaching a load of 30, however, having slack is better.
- getting timeline of a non-existing user takes 2 seconds,
- solving 120 correlations takes about 30 minutes, and is CPU bound.

# Performances

## Single Node

This section describes results obtained with a single node cluster,
composed of N shards with redundancy R=2, goal is to assess the right
kind of nodes needed for a realistic cluster setup

Ideally, we want to find the thick line where workers start moving
from being CPU-bound to being IO-bound.

### Start-2-S-SATA

- 1× Intel® C2350 (Avoton) - CPU - 2C/2T - 1.7 GHz
- 1 × 1 To SATA
- 4 Go DDR3

Insert rate after 1 hour: 3100 QPS, CPU bound.

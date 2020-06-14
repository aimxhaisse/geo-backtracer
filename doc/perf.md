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

Insert rate over 1 hour: **3 100 QPS**.

### GP1-XS

- 4 vCPUs
- 16G ram
- 150GB NVMe

Insert rate over 1 hour: **28 300 QPS**.

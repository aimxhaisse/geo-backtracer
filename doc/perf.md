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

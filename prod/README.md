# Production

This directory contains an Ansible configuration to setup a cluster to
host the Geo-backtracer. It is a work in progress.

## Features

* automatic set up of users,
* common set of tools & packages on all nodes,
* encrypted traffic between nodes in the cluster via Tinc,
* firewall configuration to deny everything not-needed by default.

## Usage

This is a quick guide, which is going to be more complete once the
cluster is stable and performing as intended.

### Requirements

* SSH access to a bunch of bare-metal machines,
* a vanilla Ubuntu 18.04 installation,
* your unix user part of sudoers.

### Configuration

Edit a few configuration files & copy your public key to the server:

    $EDITOR hosts
    $EDITOR group_vars/all.yml
    cp ~/.ssh/id_rsa.pub roles/base/files/user-${USER}.pub

### Set up

    ansible-playbook site.yml

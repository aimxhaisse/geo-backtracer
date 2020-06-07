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
* a vanilla Ubuntu 20.04 installation,
* your unix user part of sudoers.

### Configuration

Edit a few configuration files & copy your public key to the server:

    $EDITOR hosts
    $EDITOR group_vars/all.yml
    cp ~/.ssh/id_rsa.pub roles/base/files/user-${USER}.pub

### Set up

If your Unix user requires a password to go sudo (default with
Ubuntu's vanilla install), you need to explicitly specify your
password (the set up will then configure sudo to avoid doing this each
time):

    ansible-playbook playbooks/setup.yml --extra-vars "ansible_sudo_pass=YOUR_PASSWORD"

Further runs can directly use:

    ansible-playbook playbooks/setup.yml

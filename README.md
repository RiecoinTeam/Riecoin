# Riecoin Core

This repository hosts the Riecoin Core source code, the software enabling the use of the Riecoin currency.

## Riecoin Introduction

Riecoin is a decentralized and open source peer to peer digital currency, released in 2014. It is a fork of Bitcoin, also Proof of Work (PoW) based, and proposes similar features. However, its PoW is better, and this is what distiguishes Riecoin from similar currencies.

In short, computers supporting the Bitcoin network to make it secure by validating transactions are called miners, and must constantly solve a problem defined by the Bitcoin’s PoW type. This is a heavy task, consuming a lot of energy. In the Bitcoin and most PoW altcoins cases, the problem is to find an hash that meets an arbitrary criteria. To solve this problem, miners are continuously generating random hashes until they find one meeting the criteria.

Riecoin miners are not looking for useless hashes, but prime constellations. These prime numbers are of interest to mathematicians and the scientific community, so Riecoin not only provides a secure payment network, but also valuable research data by making clever use of the computing power that is otherwise wasted. Additionally, work on its software may eventually lead to theoretical discoveries as developers and mathematicians improve it, for example by finding new algorithms for the miner.

Visit the [official website](https://riecoin.dev/) to find more resources about Riecoin.

## Build Riecoin Core

### Recent Debian/Ubuntu

Here are basic build instructions to generate the Riecoin Core binaries, including the Riecoin-Qt GUI wallet. For more detailed instructions and options, read [this](doc/build-unix.md).

First, get the build tools and dependencies, which can be done by running as root the following commands.

```bash
apt install build-essential libtool autotools-dev automake pkg-config bsdmainutils python3
apt install libevent-dev libboost-system-dev libboost-filesystem-dev libboost-test-dev libboost-thread-dev libdb-dev libdb++-dev libminiupnpc-dev libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libgmp-dev libsqlite3-dev
```

Get the source code.

```bash
git clone https://github.com/riecointeam/riecoin.git
```

Then,

```bash
cd riecoin
./autogen.sh
./configure --with-incompatible-bdb
make
```

Note that this will use the BDB library provided by your repositories (downloaded using the apt command above), which may cause incompatibilities if you use wallets built by others or using Gitian. In particular, if you already used Riecoin Core, you should make a backup of the old data to be safe. If you want to use the standard BDB 4.8, you need to install it yourself and run `configure` without `--with-incompatible-bdb`. You can also build using Gitian.

The Riecoin-Qt binary is located in `src/qt`. You can run `strip riecoin-qt` to reduce its size a lot.

#### Gitian Build

Riecoin can be built using Gitian. The process is more complicated, but also deterministic: everyone building this way should obtain the exact same binaries. Official binaries are produced this way, so anyone can ensure that they were not created with an altered source code by building themselves using Gitian. If needed, see the [instructions](https://github.com/bitcoin-core/docs/blob/master/gitian-building.md) for Bitcoin, though many things there are outdated or even broken. Below is a proposed way to build with Gitian.

First, install a fresh Ubuntu 20.04 in a Virtual Machine or actual hardware, using `gitianuser` as username (or adapting commands below). The disk/partition size should be at least 40 GB, and 8 GB or more of RAM is recommended.

Once done, boot the system and open a terminal. Get root privileges with `sudo su` or by logging in as root. Then, get the dependencies:

```bash
apt install git ruby apt-cacher-ng qemu-utils debootstrap lxc python-cheetah parted kpartx bridge-utils make curl firewalld
```

Still as root, apply some workarounds:

```bash
echo "%sudo ALL=NOPASSWD: /usr/bin/lxc-start" > /etc/sudoers.d/gitian-lxc
echo "%sudo ALL=NOPASSWD: /usr/bin/lxc-execute" >> /etc/sudoers.d/gitian-lxc
echo '#!/bin/sh -e' > /etc/rc.local
echo 'brctl addbr br0' >> /etc/rc.local
echo 'ip addr add 10.0.3.1/24 broadcast 10.0.3.255 dev br0' >> /etc/rc.local
echo 'ip link set br0 up' >> /etc/rc.local
echo 'firewall-cmd --zone=trusted --add-interface=br0' >> /etc/rc.local
echo 'exit 0' >> /etc/rc.local
chmod +x /etc/rc.local
echo 'export USE_LXC=1' >> /home/gitianuser/.profile
echo 'export GITIAN_HOST_IP=10.0.3.1' >> /home/gitianuser/.profile
echo 'export LXC_GUEST_IP=10.0.3.5' >> /home/gitianuser/.profile
```

Reboot.

Now, get the Riecoin Core source code and prepare Gitian. This will ask for your password to get temporary root privileges.

```bash
git clone https://github.com/riecointeam/riecoin.git
cp riecoin/contrib/gitian-build.py .
./gitian-build.py --setup
```

You can now do the Gitian Build.

```bash
export NAME=riemann
export VERSION=0.21.0
./gitian-build.py --detach-sign --no-commit -b $NAME $VERSION
```

It will be very long, do not be surprised if it takes hours. You can check the progress in the log files in `gitian-builder/var` (ignore the possible "failed to create directory" errors in the terminal). Windows and Linux binaries for several architectures will be generated in the `riecoin-binaries` folder.

### Other OSes

Either build using Gitian in a Debian or Ubuntu virtual or spare machine, or refer to the "build" documents in the [doc](doc) folder. The instructions were for Bitcoin but should be easily adaptable for Riecoin.

## License

Riecoin Core is released under the terms of the MIT license. See [COPYING](COPYING) for more information or see https://opensource.org/licenses/MIT.
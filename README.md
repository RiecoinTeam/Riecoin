# Riecoin Core

![Riecoin Logo](https://riecoin.dev/files/w/thumb.php?f=Riecoin.svg&width=128)

This repository hosts the Riecoin Core source code, the software enabling the use of the Riecoin currency.

## Riecoin Introduction

Riecoin is a decentralized and open source peer to peer digital currency, released in 2014. It is a fork of Bitcoin, also Proof of Work (PoW) based, and proposes similar features. However, its PoW is better, and this is what distiguishes Riecoin from similar currencies.

In short, computers supporting the Bitcoin network to make it secure by validating transactions are called miners, and must constantly solve a problem defined by the Bitcoin’s PoW type. This is a heavy task, consuming a lot of energy. In the Bitcoin and most PoW altcoins cases, the problem is to find an hash that meets an arbitrary criteria. To solve this problem, miners are continuously generating random hashes until they find one meeting the criteria.

Riecoin miners are not looking for useless hashes, but prime constellations. These prime numbers are of interest to mathematicians and the scientific community, so Riecoin not only provides a secure payment network, but also valuable research data by making clever use of the computing power that is otherwise wasted. Additionally, work on its software may eventually lead to theoretical discoveries as developers and mathematicians improve it, for example by finding new algorithms for the miner.

Visit the [official website](https://riecoin.dev/) to learn more about Riecoin.

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

Note that this will use the BDB library provided by your repositories (downloaded using the apt command above), which may cause incompatibilities if you use wallets built by others or using Guix. In particular, if you already used Riecoin Core, you should make a backup of the old data to be safe. If you want to use the standard BDB 4.8, you need to install it yourself and run `configure` without `--with-incompatible-bdb`. You can also build using Guix.

The Riecoin-Qt binary is located in `src/qt`. You can run `strip riecoin-qt` to reduce its size a lot.

#### Guix Build

Riecoin can be built using Guix. The process is longer, but also deterministic: everyone building this way should obtain the exact same binaries. Distributed binaries are produced this way, so anyone can ensure that they were not created with an altered source code by building themselves using Guix. It is recommended to take a look at the much more complete [guide](https://github.com/bitcoin/bitcoin/blob/22.x/contrib/guix/README.md) from Bitcoin.

You should have a lot of free disk space (at least 40 GB), and 16 GB of RAM or more is recommended.

Install Guix on your system, on Debian 11 this can be done as root with

```bash
apt install guix
```

Still as root, make a build user pool and start the daemon,

```bash
groupadd --system guixbuild
for i in $(seq -w 1 10);
do
	useradd -g guixbuild -G guixbuild -d /var/empty -s $(which nologin)  -c "Guix build user $i" --system  guixbuilder$i;
done
guix-daemon
```

Now, get the Riecoin Core source code.

```bash
git clone https://github.com/riecointeam/riecoin.git
```

Start the Guix build. The environment variable will set which binaries to build (here, Linux x64, Linux Arm64, and Windows x64, but it is possible to add other architectures or Mac with an SDK).

```bash
export HOSTS="x86_64-linux-gnu aarch64-linux-gnu x86_64-w64-mingw32"
cd riecoin
./contrib/guix/guix-build
```

It will be very long, do not be surprised if it takes an hour or more. The binaries will be generated in a `guix-build-.../output` folder.

### Other OSes

Either build using Guix in a spare machine, or refer to the "build" documents in the [doc](doc) folder. The instructions were for Bitcoin but should be easily adaptable for Riecoin.

## License

Riecoin Core is released under the terms of the MIT license. See [COPYING](COPYING) for more information or see https://opensource.org/licenses/MIT.

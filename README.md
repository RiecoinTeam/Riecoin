# Riecoin Core

![Riecoin Logo](https://riecoin.dev/files/w/thumb.php?f=Riecoin.svg&width=128)

This repository hosts the Riecoin Core source code, the software enabling the use of the Riecoin currency.

Guides and release notes are available on the [project's page on Riecoin.dev](https://riecoin.dev/en/Riecoin_Core).

## Riecoin Introduction

Riecoin is a decentralized and open source peer to peer digital currency, released in 2014. It is a fork of Bitcoin, also Proof of Work (PoW) based, and proposes similar features. However, its PoW is better, and this is what distiguishes Riecoin from similar currencies.

In short, computers supporting the Bitcoin network to make it secure by validating transactions are called miners, and must constantly solve a problem defined by the Bitcoin’s PoW type. This is a heavy task, consuming a lot of energy. In the Bitcoin and most PoW altcoins cases, the problem is to find an hash that meets an arbitrary criteria. To solve it, miners are continuously generating random hashes until they find one meeting the criteria.

Riecoin miners are not looking for useless hashes, but doing actual scientific number crunching, like in Folding@Home or the GIMPS (currently, they are looking for prime constellations).

The project broke and holds several number theory world records, and demonstrated that scientific computations can be done using the PoW concept, and at the same time power a secure and practical international currency. It effectively solves the Bitcoin's power consumption issue without resorting to ideas like PoS that enrich the richer by design and makes value out of thin air.

Visit [Riecoin.dev](https://riecoin.dev/) to learn more about Riecoin.

## Build Riecoin Core

### Recent Debian/Ubuntu

Here are basic build instructions to generate the Riecoin Core binaries, including the Riecoin-Qt GUI wallet. For more detailed instructions and options, read [this](doc/build-unix.md).

First, get the build tools and dependencies, which can be done by running as root the following commands.

```bash
apt install build-essential libtool autotools-dev automake pkg-config bsdmainutils python3
apt install libevent-dev libboost-system-dev libboost-filesystem-dev libboost-test-dev libboost-thread-dev libdb-dev libdb++-dev libminiupnpc-dev libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libgmp-dev libsqlite3-dev libqrencode-dev
```

Get the source code.

```bash
git clone https://github.com/RiecoinTeam/Riecoin.git
```

Then,

```bash
cd Riecoin
./autogen.sh ; ./configure ; make
```

The Riecoin-Qt binary is located in `src/qt`. You can run `strip riecoin-qt` to reduce its size a lot.

#### Guix Build

Riecoin can be built using Guix. The process is longer, but also deterministic: everyone building this way should obtain the exact same binaries. Distributed binaries are produced this way, so anyone can ensure that they were not created with an altered source code by building themselves using Guix. It is recommended to take a look at the much more complete [guide](https://github.com/bitcoin/bitcoin/blob/22.x/contrib/guix/README.md) from Bitcoin.

You should have a lot of free disk space (at least 40 GB), and 16 GB of RAM or more is recommended.

Install Guix on your system, on Debian 12 this can be done as root with

```bash
apt install guix
```

Still as root, start the daemon,

```bash
guix-daemon
```

Now, get the Riecoin Core source code.

```bash
git clone https://github.com/RiecoinTeam/Riecoin.git
```

Start the Guix build. The environment variable will set which binaries to build (here, Linux x64, Linux Arm64, and Windows x64, but it is possible to add other architectures or Mac with an SDK).

```bash
export HOSTS="x86_64-linux-gnu aarch64-linux-gnu x86_64-w64-mingw32"
cd Riecoin
./contrib/guix/guix-build
```

It will be very long, do not be surprised if it takes an hour or more, even with a powerful machine. The binaries will be generated in a `guix-build-.../output` folder.

### Other OSes

Either build using Guix in a spare machine, or refer to the [Bitcoin's Documentation (build-... files)](https://github.com/bitcoin/bitcoin/tree/master/doc) and adapt the instructions for Riecoin if needed.

## License

Riecoin Core is released under the terms of the MIT license. See [COPYING](COPYING) for more information or see https://opensource.org/licenses/MIT.

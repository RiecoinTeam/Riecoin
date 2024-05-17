# Riecoin Core

![Riecoin Logo](https://riecoin.xyz/Logos/Riecoin128.png)

This repository hosts the Riecoin Core source code. Riecoin Core connects to the Bitcoin peer-to-peer network to download and fully validate blocks and transactions. It also includes a wallet and graphical user interface, which can be optionally built.

Guides and release notes are available on the [project's page on Riecoin.xyz](https://riecoin.xyz/Core/).

## Riecoin Introduction

Riecoin is a currency based on Bitcoin, and follows in its footsteps into becoming a world currency. The Project supports and concretizes the idea that the gigantic mining resources can also serve scientific research, thus power a world currency of greater value for the society.

Riecoin miners are not looking for useless hashes, but doing actual scientific number crunching, like in Folding@Home or the GIMPS (currently, they are looking for prime constellations).

The project broke and holds several number theory world records, and demonstrated that scientific computations can be done using the PoW concept, and at the same time power a secure and practical international currency. It effectively solves the Bitcoin's power consumption issue without resorting to ideas like PoS that enrich the richer by design and makes value out of thin air.

Visit [Riecoin.xyz](https://riecoin.xyz/) to learn more about Riecoin.

## Build Riecoin Core

### Recent Debian/Ubuntu

Here are basic build instructions to generate the Riecoin Core binaries, including the Riecoin-Qt GUI wallet.

First, get the build tools and dependencies, which can be done by running as root the following commands.

```bash
apt install build-essential libtool autotools-dev automake pkg-config bsdmainutils python3
apt install libevent-dev libboost-system-dev libboost-filesystem-dev libboost-test-dev libboost-thread-dev libminiupnpc-dev libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libgmp-dev libsqlite3-dev libqrencode-dev
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

Riecoin can be built using Guix. The process is longer, but also deterministic: everyone building this way should obtain the exact same binaries. Distributed binaries are produced this way, so anyone can ensure that they were not created with an altered source code by building themselves using Guix. Read the [Guix Guide](contrib/guix/README.md) for more details and options.

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

Either build using Guix as explained above in a spare physical or virtual machine, or refer to the [Bitcoin's Documentation (build-... files)](https://github.com/bitcoin/bitcoin/tree/master/doc) and adapt the instructions for Riecoin if needed.

## License

Riecoin Core is released under the terms of the MIT license. See [COPYING](COPYING) for more information or see https://opensource.org/licenses/MIT.

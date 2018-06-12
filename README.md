# dpaste

A simple pastebin for light values (max 64KB) using OpenDHT distributed hash table.

## Example

Let a file `A.md` you want to share.
```sh
$ dpaste < A.md
DPASTE: Pasting data...
dpaste:74236E62
```

You can share this PIN (one can omit the string `dpaste:`). It is used to
retrieve the pasted file within 10 minutes by simply doing:
```sh
$ dpaste -g dpaste:74236E62
```

Use respectively options `-e` and `-s` to encrypt and sign messages using the
OpenPGP protocol (a working gpg configuration needs to be found on the system).

## How to build

Assuming you have the installed the project dependencies prior to this, you can
either use CMake or GNU Autotools to build.

### Using GNU Autotools

```sh
$ ./autogen.sh
$ ./configure
$ make
```

You'll then find the binary under `src/` directory.

### Using CMake

```sh
$ mkdir build && cd build
$ cmake ..
$ make
```

You'll then find the binary `dpaste` under `build` directory.

## Package

Archlinux AUR: https://aur.archlinux.org/packages/dpaste/

Milis Linux:   mps kur dpaste  (https://github.com/milisarge/malfs-milis/blob/master/talimatname/genel/dpaste/talimat)

## Dependencies

- [OpenDHT](https://github.com/savoirfairelinux/opendht/) (minimal version: 1.2.0)
- [msgpack-c](https://github.com/msgpack/msgpack-c)
- [gpgmepp](https://github.com/KDE/gpgmepp)
- [json.hpp](https://github.com/nlohmann/json)
- [cURLpp](https://github.com/jpbarrette/curlpp) (minimal version: 0.8.1)
- [glibmm](https://github.com/GNOME/glibmm)
- [libb64](http://libb64.sourceforge.net/)
- Getopt

## Pastebin over DHT

A DHT is efficient and requires no infrastructure. In practice, you can always
count on the network to host your data for you since a distributed network is
not likely to be "down".

## Roadmap

- Password based encryption (AES using gnutls);
- Add support for values with size greater than 64Ko (splitting values across
  multiple locations);
- ~~Add user configuration file system;~~
- ~~Support RSA encrypt/sign using user's GPG key;~~
- ~~Support running the DHT node as service for executing dpaste operations.~~

## Author

- Simon Désaulniers <sim.desaulniers@gmail.com>
- Adrien Béraud <adrien.beraud@savoirfairelinux.com>


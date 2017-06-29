# dpaste

A simple pastebin for light values (max 64KB) using OpenDHT distributed hash table.

## Example

Let a file `A.md` you want to share.
```sh
$ cat A.md | dpaste
74236E62
```

You can share this PIN. It is used to retrieve the pasted file within 10 minutes
by simply doing:
```sh
$ dpaste -g 74236E62
```

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

## Dependencies

- [OpenDHT](https://github.com/savoirfairelinux/opendht/) (minimal version: 1.2.0)
- [json.hpp](https://github.com/nlohmann/json) (minimal version: unknown)
- [cURLpp](https://github.com/jpbarrette/curlpp) (minimal version: unknown)
- [glibmm](https://github.com/GNOME/glibmm) (minimal version: unknown)
- [libb64](http://libb64.sourceforge.net/) (minimal version: unknown)
- Getopt (minimal version: unknown)

## Pastebin over DHT

A DHT is efficient and requires no infrastructure. In practice, you can always
count on the network to host your data for you since a distributed network is
not likely to be "down".

## Roadmap

- ~~Add user configuration file system;~~
- Support RSA encrypt/sign using user's GPG key;
- ~~Support running the DHT node as service for executing dpaste operations.~~

## Author

- Simon Désaulniers <sim.desaulniers@gmail.com>
- Adrien Béraud <adrien.beraud@savoirfairelinux.com>


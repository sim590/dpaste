# dpaste

A simple pastebin for light values (max 64KB) using OpenDHT distributed hash table.

## Example

Let a file `A.md` you want to share.
```sh
$ cat A.md | dpaste
be1ee067b3bbea12a7d2b6cb8e4838d11fe9c23d
```

You can share this hash. You can retrieve the pasted file within 10 minutes by
simply doing:
```sh
$ dpaste -g be1ee067b3bbea12a7d2b6cb8e4838d11fe9c23d
```

## How to build

```sh
$ ./autogen.sh
$ ./configure
$ make
```

You then can find binaries under `src/` directory.

## Package

Archlinux AUR: https://aur.archlinux.org/packages/dpaste/

## Dependencies

- [OpenDHT](https://github.com/savoirfairelinux/opendht/) (minimal version: 1.2.0)
- [glibmm](https://github.com/GNOME/glibmm) (minimal version: unknown)
- Getopt (minimal version: unknown)

## Pastebin over DHT

A DHT is efficient and requires no infrastructure. In practice, you can always
count on the network to host your data for you since a distributed network is
not likely to be "down".

## Roadmap

- Add user configuration file system;
- Support RSA encrypt/sign using user's GPG key;
- Support running the DHT node as service for executing dpaste operations.

## Author

- Simon Désaulniers <sim.desaulniers@gmail.com>

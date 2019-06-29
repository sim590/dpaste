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

## Encryption

One can encrypt his document using the option `--aes-encrypt` or
`--gpg-encrypt -r {recipient}`. In the former case, AES-CBC is used and in
the latter it is simple GPG encryption. One can also *sign-then-encrypt* his
message by adding the flag `-s` (a working gpg configuration needs to be found
on the system). If both `--aes-encrypt` and `--gpg-encrypt` (or `-s`) options
are present, aes encryption method is used.

### AES

When using `--aes-encrypt`, `dpaste` will generate a random 32-bit passphrase
which will then be stretched using [argon2][] crypto library. This is all
handled by OpenDHT crypto layer. After pasting the blob, the returned PIN will
be 64 bits long instead of the classic 32 bits. Indeed, the generated 32-bit
password is appended to the location code used to index on the DHT. For e.g.:

```sh
$ dpaste --aes-encrypt < ${some_file}
DPASTE: Encrypting (aes-cbc) data...
DPASTE: Pasting data...
dpaste:B79F2F91C811D5DC
```

Therefore, the blob will be pasted on `HASH("B79F2F91")` and encrypted with
a key derived from the passphrase `C811D5DC`.

[argon2]: https://github.com/P-H-C/phc-winner-argon2

## How to build

Assuming you have the installed the project dependencies prior to this, you can
either use CMake or GNU Autotools to build. A `c++17` compliant compiler is
required to compile the program.

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
- [json.hpp](https://github.com/nlohmann/json) (required version for CMake: 2.1.1)
- [cURLpp](https://github.com/jpbarrette/curlpp) (minimal version: 0.8.1)
- [glibmm](https://github.com/GNOME/glibmm)
- [libb64](http://libb64.sourceforge.net/)
- Getopt
- [catch](https://github.com/catchorg/Catch2) for unit tests

## Pastebin over DHT

A DHT is efficient and requires no infrastructure. In practice, you can always
count on the network to host your data for you since a distributed network is
not likely to be "down".

## Roadmap

- Add support for values with size greater than 64Ko (splitting values across
  multiple locations);
- ~~Password based encryption (AES using gnutls)~~;
- ~~Add user configuration file system;~~
- ~~Support RSA encrypt/sign using user's GPG key;~~
- ~~Support running the DHT node as service for executing dpaste operations.~~

## Author

- Simon Désaulniers <sim.desaulniers@gmail.com>
- Adrien Béraud <adrien.beraud@savoirfairelinux.com>


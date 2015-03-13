# How to build

This project should be cross-compiled with Vagrant. See [otto-creator](https://github.com/NextThingCo/otto-creator) for details.

From the root directory make a `build/` directory and run `cmake` from inside it.

```
mkdir build && cd build
CC=arm-stak-linux-gnueabihf-gcc CXX=arm-stak-linux-gnueabihf-g++ cmake ..
```

Then from the build directory you can run `make`.

```
make
```

# Running

Run the otto-sdk `main` with the menu and mode libs:

```
sudo /stak/sdk/otto-sdk/build/main /stak/sdk/otto-menu/build/libotto_menu.so <mode-library.so>
```

Note that `otto-menu` and `otto-sdk` must be located at `/stak/sdk` on your Pi.
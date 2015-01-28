## How to build

This project requires a C++ compiler and standard library that support C++14. We are currently testing it with `clang 3.5.1` and `libc++ 3.5.1` on Arch linux.

From the root directory make a `build/` directory and run `cmake` from inside it.

```
mkdir build && cd build
cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ ..
```

Then from the build directory you can run `make`.

```
make
```

## Running

```
sudo deps/otto-sdk/build/main `pwd`/build/libotto_test_mode.so
```

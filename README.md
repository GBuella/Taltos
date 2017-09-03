Taltos
======

Master:
[![Build Status](https://travis-ci.org/GBuella/Taltos.svg?branch=master)](https://travis-ci.org/GBuella/Taltos)
[![Build status](https://ci.appveyor.com/api/projects/status/dhl9bnffws0fep3h/branch/master?svg=true)](https://ci.appveyor.com/project/GBuella/taltos/branch/master)

Experimental chess engine for learning and fun

# How to build

- Needs a recent C11 compiler to build ( tested GCC, clang )

On POSIX-ish systems:
```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

On Windows:
```
mkdir build
cd build
cmake -TLLVM-vs2014 -G "Visual Studio 14 2015 Win64" -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

Many thanks to the Chess Programming Wiki http://chessprogramming.wikispaces.com/

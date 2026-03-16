# Installation on Linux/WSL2

## Prerequisites:

```bash
sudo apt update
sudo apt install build-essential autoconf automake pkg-config \
                 libsdl1.2-dev libsdl-net1.2-dev libsdl-mixer1.2-dev \
                 libsdl-image1.2-dev libsamplerate0-dev libpng-dev zlib1g-dev
```

## Build:

```bash
./autogen.sh
./configure
make
```

## Run:

```bash
src/chocolate-doom -iwad ../Doom_STM32N6570_DK/wad/doom1.wad -window -3
```


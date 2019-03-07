## SDL2 for Wii U

### About  
This project is a port of the **SDL** software development library to the Nintendo Wii U video game console, built for the wut toolchain.
Currently it implements support for audio, joystick (gamepad), touchscreen (gamepad), video (gx2), hardware accelerated rendering (gx2), timers and threading.

### Installing  
Prebuilt versions of sdl2, along with other sdl2 libraries (gfx, image, mixer, ttf), are available on the wiiu-fling pacman repository.
Please reffer to [these](https://gitlab.com/QuarkTheAwesome/wiiu-fling) instructions to set up wiiu-fling.

### Building on Linux
In order to build sdl2 for wiiu, you'll need to install some prerequisites:
   -  devkitPPC (make sure to export $DEVKITPRO)
   - wut (make sure to export $WUT_ROOT)
   - cmake

Clone and enter the wiiu sdl repo:

    $ git clone https://github.com/yawut/SDL.git
    $ cd SDL
Prepare for the build:

    $ mkdir build
    $ cd build
Build:

    $ cmake ../ -DCMAKE_TOOLCHAIN_FILE=$WUT_ROOT/share/wut.toolchain.cmake -DCMAKE_INSTALL_PREFIX=$DEVKITPRO/portlibs/wiiu
    $ make
Install (might need to run as sudo depending on `$DEVKITPRO/portlibs/wiiu`permissions):

    $ make install


### Credits:
- rw-r-r-0644, quarktheawesome, exjam: wiiu sdl2 port and libraries
- wiiu homebrew contributors
- sdl mantainters
- inspired by libnx/libtransistor sdl2 ports


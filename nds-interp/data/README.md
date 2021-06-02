`data.7z` contains the binary files used to validate the line interpolation. These were generated from the programs in the `bin` folder, which were built with [devkitARM](https://devkitpro.org/wiki/Getting_Started) from the sources in `src`. The files contain a simplified screen dump of the lines produced by every possible interpolation, with the line origin at the top left (TL), top right (TR), bottom left (BL) or bottom right (BR). In order to run the main program you'll need to extract the contents of `data.7z` into this directory.

At the top of [`src/source/main.cpp`](src/source/main.cpp) there are some adjustable parameters. The program can be configured to either generate or validate line interpolation through the `generateData` flag. In validation mode, the program expects to find a `data.bin` file in NitroFS. In order to embed the generated test data, simply create a `nitrofiles` folder in this directory, copy the desired data file into it and call it `data.bin`. The other parameters allow adjusting the origin of the line (top left, top right, bottom left or bottom right) and the extent of the area to test, which are only used when generating the data files. By default, the program will generate lines spanning the entire screen. You can also take a screen capture (a VRAM dump) of the last frame with `screencap` -- this will generate a file named `linetest-screencap.bin` which can then be converted to a TGA file with the main program at the root of this repository.

The `images` folder contains compressed files that contain screen captures of every possible slope the Nintendo DS can generate for each of the four origin points.
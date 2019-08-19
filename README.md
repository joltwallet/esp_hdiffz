A library to apply hdiffz patches to the esp32

# HDiffz

[HDiffPatch](https://github.com/sisong/HDiffPatch) is a binary difference tool 
that generates compact, compressible patches to update data.

On the esp32, this could be used to perform OTA updates of firmware, or for
updating binary data on the filesystem. Currently this library provides:

    * zlib decompression of patch data via builtin miniz. This is always applied
      since it is assumed that the programmer wants the smallest payload possible
      while leveraging ESP32 ROM code.
    * Convenience functions for OTA update via streamed diff data.
    * Convenience functions for file update via streamed diff data.

NOTE: Currently this library only applies patches, it does not generate them 
on the ESP32. This functionality may be added later if desired and compute 
resources are feasible.

# Why use it?

A firmware update for the ESP32 is typically a few megabytes. However, the new 
firmware typically is very similar to the old firmware, so its wasteful to 
send the complete firmware over. The difference between the 2 firmwares might 
be on the order of 10's or 100's of kilobytes. This diff data is also compressed
to reduce the size even further. The result is a firmware update that might be
more than 20x faster than naively sending over the complete binary.

Including this component will typically add about 8988 bytes to your firmware's
binary.

TODO: report above number when in release mode.

# Installation

In your project's `component/` folder, clone this repo (or add as a submodule).

```
git clone --recursive https://github.com/joltwallet/esp_hdiffz.git
```

This project relies on `esp_full_miniz` for the higher level compression/decompression 
interface. Clone this isn't your `component/` directory as well:

```
git clone https://github.com/joltwallet/esp_full_miniz.git
```

# TODO

* Example Code
* Unit Tests

# Usage

* NOTE: This library does not do any validity checks on the old data it is applying 
  the patch to. You must do this externally either via versioning, hash checks, 
  er some other method.

TODO

# Generating a Compressed Patch

This library uses the built in `miniz` on the ESP32's ROM. Unlike typical 
default zlib settings, we use a dictionary size of 4KB. This reduces the 
required buffer from 32KB to 4KB while decompressing while having minimal 
impact on the resulting compressed data size. Because of this, we use a 
fork of HDiffPatch that has windowBits set to 12.

To build the hdiffz binary:

```
cd HDiffPatch
make
```

Then a diff file can be created via:

```
hdiffz -c-zlib old_firmware.bin new_firmware.bin firmware_update_patch.bin
```

# Unit Tests

Set up a folder with the projects as follows:

```
components/
├── esp_hdiffz/
│   ├── component.mk
│   ├── HDiffPatch/
│   ├── include/
│   ├── src/
│   └── test/
└── esp_full_miniz
    ├── component.mk
    ├── include/
    └── src/
```

Run the `flash-unit-test.sh` script from the `esp_hdiffz` directory.


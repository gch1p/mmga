# mmga: Make MacBook Great Again
**mmga** is a script to help flashing coreboot on some MacBook Air and Pro models without using external SPI programmer.

#### Supported models
As of time of writing, following models are supported in coreboot. Other models might be supported in future.
* MacBook Air 4,2 (13'' Mid 2011) (`macbookair4_2`)
* MacBook Air 5,2 (13'' Mid 2012) (`macbookair5_2`)
* MacBook Pro 8,1 (13'' Early 2011) (`macbookpro8_1`)
* MacBook Pro 10,1 (15'' Mid 2012 Retina) (`macbookpro10_1`)

#### System requirements
* Recent Linux booted with `iomem=relaxed` kernel parameter (needed for internal flashing to work);
* Build dependencies. Here's a list for Ubuntu 16.04:
    ```
    # apt install bison build-essential curl flex git gnat libncurses5-dev m4 zlib1g-dev make libpci-dev libusb-1.0-0-dev
    ```
    On other distros package names might differ. Be sure to install **gnat** prior to building coreboot toolchain.

#### Building flashrom
First of all, grab recent flashrom source tree and build it:
```
$ git clone https://review.coreboot.org/flashrom.git && cd flashrom
$ make
```
Optionally you can install it to `/usr/local/sbin`:
```
$ sudo make install
```

## How it works
The firmware is stored on SPI chip. On Intel platforms it consists of various regions: `fd` (Flash Descriptor), `me` (Intel ME), `bios` (the BIOS, or, in case of MacBooks, EFI), and some other (such as `gbe` for Gigabit Ethernet). The most important region in context of this story is FD, the Intel Flash Descriptor.

The **Intel Flash Descriptor** is a data structure stored on the flash chip; it contains information such as space allocated for each region of the flash image (also called a layout), read-write permissions for each region and many more. The Flash Descriptor is located at the **first 4K** of the SPI chip (`0x0000-0x0fff`).

Here's flash chip layout used in MacBook Air 5,2 (which has 8M flash chip), extracted from original firmware dump with ifdtool:
```
00000000:00000fff fd
00190000:007fffff bios
00001000:0018ffff me
```
As you can see,
- the first region (`0x0000-0x0fff`) is used for a Flash Descriptor (as it always is);
- right next to it (`0x1000-0x18ffff`) Intel ME firmware is stored;
- and the last and largest region (`0x190000-0x7fffff`) is the BIOS, or Apple's EFI in our case.

Normally, the FD should be read-only, but this is not the case with MacBooks. Apparently, Apple's "Think Different" (TM) thing applies to firmware security as well.

You can check access permissions on your MacBook by running `flashrom -p internal` (if it doesn't work, make sure you have booted with `iomem=relaxed` and/or use `-p internal:laptop=force_I_want_a_brick` instead). This is what it shows on MacBook Air 5,2 with latest Mojave firmware updates:
```
# flashrom -p internal
flashrom v1.1-rc1-3-g4ca575d on Linux 4.9.0-9-amd64 (x86_64)
flashrom is free software, get the source code at https://flashrom.org

Using clock_gettime for delay loops (clk_id: 1, resolution: 1ns).
No DMI table found.
Found chipset "Intel QS77".
Enabling flash write... SPI Configuration is locked down.
PR0: Warning: 0x00190000-0x0066ffff is read-only.
PR1: Warning: 0x00692000-0x01ffffff is read-only.
At least some flash regions are write protected. For write operations,
you should use a flash layout and include only writable regions. See
manpage for more details.
OK.
Found Micron/Numonyx/ST flash chip "N25Q064..3E" (8192 kB, SPI) mapped at physical address 0x00000000ff800000.
No operations were specified.
```
As you can see, only the `bios` region is read-only, and not even whole `bios` region, because `0x670000-0x681fff` is writable for some reason:
```
PR0: Warning: 0x00190000-0x0066ffff is read-only.
PR1: Warning: 0x00692000-0x01ffffff is read-only.
```
It's interesting that this behavior is reproducible only after cold boot. If you suspend to S3, resume and run flashrom again, `fd` will be read-only:
```
PR0: Warning: 0x00000000-0x00000fff is read-only.
PR1: Warning: 0x00190000-0x0066ffff is read-only.
PR2: Warning: 0x00692000-0x01ffffff is read-only.
```
Looks like a bug in Apple's firmware. Obviously it should always be read-only.

Anyway, that means that after cold boot **`fd` and `me` regions are writable**, and that gives us around 1.5M of writable space. Since we can rewrite FD, we can write a new FD with custom layout. So **the idea is to repartition the flash chip** and flash new bios to a writable space.

Let's write a new layout (I decided to use `0x00000-0xfffff` region for convenience):
```
00000000:00000fff fd
00001000:00020fff me
00021000:000fffff bios
00100000:007fffff pd
```


In this layout, we allocate 128K for `me` and 892K for `bios`. To fit the original 1.5M ME image into the 128K region, it has to be truncated with [me_cleaner](https://github.com/corna/me_cleaner) with `-t` and `-r` arguments, the size of resulting image is ~92K. We also have to allocate the remaining `0x100000-0x7fffff` region for something to be able to address and flash it in future, otherwise flashrom will give us a "Transaction error". So we just mark it as `pd`, which stands for "Platform Data".

After the new layout is ready, we build small coreboot ROM that fits into the allocated 892K bios region. We do that, then flash **`fd`** (`0x0000-0x0fff`), **`me`** (`0x1000-0x20fff`) and **`bios`** (`0x21000-0xfffff`) according to the new layout. On the next cold boot, coreboot will be loaded from the `0x21000-0xfffff` region, and old firmware, which still resides in `0x190000-0x7fffff`, will be ignored. This is **stage1**.

After we boot into just-flashed coreboot, we're able to flash the whole 8M chip, because it's not write-protected anymore. We repartition the chip again, and the new layout looks like this:
```
00000000:00000fff fd
00001000:00020fff me
00021000:007fffff bios
```
It's almost the same, except that `bios` fills all the remaining space. Then we build coreboot again, flash `fd`, `me` and `bios` and shut down again. On the next cold boot we will have completely corebooted MacBook. This is **stage2**.

## Usage instructions
The **mmga** script automates steps described above and does all dirty work.

##### Usage:
```
./mmga <options> ACTION
```

##### Options:
```
-h, --help: show help
```

##### stage1 actions:
```
          dump: dump flash contents
         fetch: fetch board tree from Gerrit (if needed)
prepare-stage1: patch IFD, neutralize and truncate ME
 config-stage1: make coreboot config (for manual use)
  build-stage1: make config and build ROM (for auto use)
  flash-stage1: flash ROM ($COREBOOT_PATH/build/coreboot.rom)
```

##### stage2 actions:
```
prepare-stage2: patch IFD (if needed)
 config-stage2: make coreboot config (for manual use)
  build-stage2: make config and build ROM (for auto use)
  flash-stage2: flash ROM ($COREBOOT_PATH/build/coreboot.rom)
```

##### other actions:
```
     flash-oem: flash OEM firmware back
```

#### Configuration
Before you start, you have to update variables the `config.inc` file:
- **`PAYLOAD`**: which payload to use, supported values are `grub` and `seabios`
- **`MODEL`**: put your macbook model here, example: `macbookair5_2`
- **`GRUB_CFG_PATH`**: only if you use `grub` payload; if empty, default grub.cfg will be used
- **`COREBOOT_PATH`**: path to cloned coreboot repository (see below)
- **`FLASHROM`**: path to flashrom binary
- **`FLASHROM_ARGS`**: in case if flashrom detects multiple flash chips, put `"-c CHIP_MODEL"` here
- **`STAGE2_USE_FULL_ME`**: if you want to use original Intel ME image in the final ROM for some reason, set to `1`

#### stage1

Get coreboot:
```
$ git clone --recurse-submodules https://review.coreboot.org/coreboot.git && cd coreboot
```
Build coreboot toolchain. You must have gnat compiler installed as it's required for graphics initialization to work (libgfxinit is written in Ada):
```
$ make crossgcc-i386 CPUS=$(nproc)
$ make iasl
```
Dump the flash chip contents:
```
./mmga dump
```
Create patched FD and neutralize ME:
```
./mmga prepare-stage1
```
If your board's port hasn't been merged to coreboot master yet or you don't know, run:
```
./mmga fetch
```
Create coreboot config and build the ROM:
```
./mmga build-stage1
```
(If you're experienced coreboot user or developer, you may want to configure and build coreboot yourself. In that case, run `config-stage1` instead of `build-stage1`. It will create config that you can then copy to `$COREBOOT_PATH`, make your changes and build. Please be aware that `build-stage1` applies SeaBIOS patch and you will have to apply it manually.)

Flash it:
```
./mmga flash-stage1
```
If it's done and you didn't see any errors, you have to **shutdown** the laptop. It's important: DO NOT REBOOT, shut it down. If you reboot, old FD will still be used and you will boot the Apple's firmware, because it was left untouched in the `0x190000-0x7fffff` region. But since you partly replaced the old `me` region, it may lead to undefined behaviour. So again, do not reboot, shut it down.

#### stage2
Create patched FD for the next flash:
```
./mmga prepare-stage2
```
Create new coreboot config and build the ROM (for experienced users, `config-stage2` is also available):
```
./mmga build-stage2
```
Flash it:
```
./mmga flash-stage2
```
This may take a while, please don't interrupt and let it finish.

If you again didn't see any errors after it's done, you have to **shutdown** the laptop again. DO NOT REBOOT, shut it down. It's even more important now: if you reboot, old FD will be used, the one that describes `bios` region as `0x21000-0xfffff`. And since you just flashed `bios` to `0x21000-0x7fffff`, this `0x21000-0xfffff` will most likely just contain `FF`s, so the laptop won't boot and will look like a brick. In that case you will need to press and hold power button for ~10 seconds to hard reset. To avoid all that, just do not reboot, shut it down.

## Misc

The script was tested on MacBook Air 5,2 and MacBook Pro 10,1. If you have successfully corebooted your macbook with it and it worked, please let me know. If you have any problems, contact me via GitHub issues or by email (see the copyright header in the script).


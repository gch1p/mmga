# mmga: Make MacBook Great Again

**mmga** is a script to help flashing coreboot on some MacBook Air and Pro
models without using external SPI programmer. See
[this blog post](https://ch1p.io/coreboot-macbook-internal-flashing/) on how to
do the same manually.

### Supported devices

As of time of writing, following devices are supported in coreboot. Other models
might be supported in future.

* MacBook Pro 8,1 (13'' Early 2011) (`macbookpro8_1`)<br>
* MacBook Pro 10,1 (15'' Mid 2012 Retina) (`macbookpro10_1`)
  
  **Attention!** Not all memory configurations are supported, see
  [here](#ram-configurations).<br>

* MacBook Air 5,2 (13'' Mid 2012) (`macbookair5_2`)

  **Attention!** Not all memory configurations are supported, see
  [here](#ram-configurations).<br>

* MacBook Air 4,2 (13'' Mid 2011) (`macbookair4_2`).
  
  **Attention!** Not all memory configurations are supported, see
  [here](#ram-configurations).

iMac 13,1 is a candidate for support too, but [coreboot port](https://review.coreboot.org/c/coreboot/+/38883)
for this device is not actively maintained at the moment and it may fail to build.
I'll add iMac 13,1 support later when it's fixed.

### RAM configurations

Models with soldered RAM are sold with different memory modules, manufactured by 
different manufacturers. Not all of them are supported currently.

To determine which memory you have in your MacBook, you can use `inteltool`
and [this script](https://github.com/gch1p/get_macbook_ramcfg). You need to run
them on the target machine.

First, [download coreboot](#stage1) and build `inteltool`:
```console
$ cd util/inteltool
$ make -j4
```

Download the script and make it executable. Then run:
```console
$ sudo ./inteltool -g | /path/to/get_macbook_ramcfg -m MODEL
```

Replace `MODEL` with your MacBook model: `mbp101` for MacBook Pro 10,1, `mba52`
for MacBook Air 5,2 and `mba42` for MacBook Air 4,2.

Then check the tables below.

#### MacBook Pro 10,1

| RAM configuration | Supported |
| ------------------|-----------|
| 4g_hynix_1600s    | 🚫 No     |
| 1g_samsung_1600   | 🚫 No     |
| 4g_samsung_1600s  | 🚫 No     |
| 1g_hynix_1600     | 🚫 No     |
| 4g_elpida_1600s   | 🚫 No     |
| 2g_samsung_1600   | 🚫 No     |
| 2g_samsung_1333   | 🚫 No     |
| 2g_hynix_1600     | ✅ Yes    |
| 4g_samsung_1600   | 🚫 No     |
| 4g_hynix_1600     | ✅ Yes    |
| 2g_elpida_1600s   | 🚫 No     |
| 2g_elpida_1600    | 🚫 No     |
| 4g_elpida_1600    | 🚫 No     |
| 2g_samsung_1600s  | 🚫 No     |
| 2g_hynix_1600s    | 🚫 No     |

#### MacBook Air 5,2

| RAM configuration | Supported |
|-------------------|-----------|
| 4g_hynix          | ✅ Yes    |
| 8g_hynix          | 🚫 No     |
| 4g_samsung        | 🚫 No     |
| 8g_samsung        | 🚫 No     |
| 4g_elpida         | 🚫 No     |
| 8g_elpida         | 🚫 No     |

#### MacBook Air 4,2

| RAM configuration | Supported |
|-------------------|-----------|
| 2g_hynix          | 🚫 No     |
| 4g_hynix          | 🚫 No     |
| 2g_samsung        | 🚫 No     |
| 4g_samsung        | ✅ Yes    |
| 2g_micron         | 🚫 No     |
| 4g_elpida         | 🚫 No     |

---

If your found out that your MacBook's memory is not supported, you can help 
supporting it. Run `sudo inteltool -m`, save output to a text file and create a
new issue specifying your MacBook model, memory configuration name with the text
file attached.


### System requirements

* Recent Linux distribution booted with `iomem=relaxed` kernel parameter
  (required for internal programmer to work);
* Build dependencies. Here's a list for Debian-based distros:
    ```
    # apt install bison build-essential curl flex git gnat libncurses5-dev m4 zlib1g-dev make libpci-dev libusb-1.0-0-dev
    ```

    If you plan to use GRUB2 as a payload:
    ```
    # apt install libfreetype-dev unifont autoconf
    ```

    On other distros package names might differ. Be sure to install **gnat**
    prior to building coreboot toolchain.

### Building flashrom

First of all, grab recent flashrom sources and build it:
```
$ git clone https://review.coreboot.org/flashrom.git && cd flashrom
$ make
```

Optionally, install it to `/usr/local/sbin`:
```
$ sudo make install
```

## How it works

The firmware of the devices covered by this project is stored on SPI chip. It
consists of various regions: `fd` (Flash Descriptor), `me` (Intel ME) and `bios`
(BIOS, or Apple EFI). Sometimes there are more regions, for example there may be
`gbe` region for Gigabit Ethernet or `ec` region with EC firmware, but for now,
let's focus on our MacBooks.

The most important region in context of this story is `fd`, the Intel Flash
Descriptor.

The Intel Flash Descriptor is a data structure of fixed size (4KB) stored on
the flash chip (resides in `0x0000-0x0fff`), that contains various information
such as space allocated for each region on the flash, access permissions, some
chipset configuration and more. In particular, it contains access permissions
for `fd` and `me` regions.

This is the flash chip layout used in MacBook Air 5,2 (which has 8 MiB flash chip).
It can be extracted from stock ROM image with `ifdtool`:
```
00000000:00000fff fd
00190000:007fffff bios
00001000:0018ffff me
```

Normally, the `fd` and `me` regions should be read-only in production, but this
is not the case with MacBooks. Apparently, Apple's "Think Different" thing
applies to firmware security as well.

Instead, they decided to use SPI Protected Range Registers (PR0-PR4) to set
protection over `fd`, but here they failed again. Due to a bug (I hope),
`0x0000-0x0fff` is not write-protected after cold boot and becomes read-only
only after resuming from S3.

You can dump PRx protections on your device by running `flashrom -p internal`.
If it doesn't work, make sure to boot with `iomem=relaxed` or try
`-p internal:laptop=force_I_want_a_brick`.

This is what you should see after a cold boot (and, if so, mmga should work on
your device):
```
PR0: Warning: 0x00190000-0x0066ffff is read-only.
PR1: Warning: 0x00692000-0x01ffffff is read-only.
```

And this is after resuming from S3:
```
PR0: Warning: 0x00000000-0x00000fff is read-only.
PR1: Warning: 0x00190000-0x0066ffff is read-only.
PR2: Warning: 0x00692000-0x01ffffff is read-only.
```

So, after cold boot flash descriptor is protected neither by PRx registers nor
by access permission bits on the flash descriptor itself. Under certain
circumstances, **writable flash descriptor allows flashing whole SPI flash** by
using a couple of neat tricks, and that is what mmga script does.

Writable `me` region gives us around 1.5 MiB of writable space. The idea is that
we can shrink ME firmware image with me_cleaner to about ~128 KiB and use the
freed space for a small temporary coreboot image. Writable `fd` gives us ability
to change flash layout and move reset vector. We combine all this, flash modified
regions, then power off (new flash descriptor becomes active on cold boot, so
reboot won't work). Then boot our small temporary coreboot and flash the whole
SPI chip, as there will be no more PRx protections set. So this is a two-stage
process.

Let's write a new layout:
```
00000000:00000fff fd
00001000:00020fff me
00021000:000fffff bios
00100000:007fffff pd
```

In this layout, we allocate 128 KiB for `me` and 892 KiB for `bios`. To fit the
original 1.5 MiB ME image into the 128 KiB region, it has to be truncated with
[me_cleaner](https://github.com/corna/me_cleaner) with `-t` and `-r` arguments,
the size of resulting image is ~92 KiB. We also have to allocate the remaining
`0x100000-0x7fffff` region for *something*, to be able to address and flash it
in future. So we just mark it as `pd`, which is commonly used for "Platform Data".

After the new layout is ready, we build small coreboot ROM that fits into the
allocated 892 KiB bios region. Then we flash **`fd`** (`0x0000-0x0fff`),
**`me`** (`0x1000-0x20fff`) and **`bios`** (`0x21000-0xfffff`) according to the
new layout. On the next cold boot, coreboot will be loaded from the
`0x21000-0xfffff` region, and the old firmware, which still resides in
`0x190000-0x7fffff`, will be ignored. This is **stage1**.

After we boot with small temporary coreboot ROM, we're able to flash the whole
8 MiB chip, because there are no more PRx protections set. We repartition the
chip again, and the new layout looks like this:
```
00000000:00000fff fd
00001000:00020fff me
00021000:007fffff bios
```

It's almost the same, except that `bios` fills all the remaining space. Then we
build coreboot again, flash `fd`, `me` and `bios` and power off again. On the
next cold boot we will have completely corebooted MacBook. This is **stage2**.

## Usage instructions

The **mmga** script automates steps described above and does all the dirty work.

### Usage:

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

### Warning

You **should** have external means of flashing for a backup, **just in case**.
The procedure described above is quite delicate and error-prone and any mistake
may lead to a brick. In that case, you should have a copy of your original ROM
**on external drive**. Please make a backup of `work/oem/dump.bin` after
running `mmga dump`, or just copy the whole mmga directory.

These posts may be a little helpful if you ever need to flash externally:

- [MacBook Air 5,2](https://ch1p.io/coreboot-mba52-flashing/)
- [MacBook Pro 10,1](https://ch1p.io/coreboot-mbp101-flashing/)

### Choosing the payload

Currently, SeaBIOS and GRUB are supported by mmga. SeaBIOS supports legacy boot,
it will most probably boot from an old school MBR partition. On the other hand,
it may not boot from GPT (I'm not sure about it, correct me if you know more)
and certainly it will not boot an EFI installation.

Sometimes GRUB is a better choice, but it all depends on your system. If you are
not sure, do some research before flashing or seek for help.

It may be a good idea to prepare a USB drive with some live system. I can imagine
a situation where you have chosen the wrong payload and cannot boot into your
system after power off/on cycle to complete the second stage, because, for instance,
SeaBIOS doesn't recognize your partition. In that case, you could try to boot
from live USB to fix your system (if possible) or to complete second stage and
change the payload.

Of course, in order to do that, you should backup the whole mmga directory after
the completion of the first stage but **before** the reboot.

**Attention!** Recent SeaBIOS versions break internal keyboard and touchpad on
MacBooks, for now it's recommended to use GRUB until it's fixed.

### Configuration

Before you start, you have to update variables the `config.inc` file:
- **`PAYLOAD`**: which payload to use, supported values are `grub` and `seabios`
- **`MODEL`**: put your macbook model here, example: `macbookair5_2`
- **`GRUB_CFG_PATH`**: only if you use `grub` payload; if empty, default
  `grub.cfg` will be used
- **`COREBOOT_PATH`**: path to cloned coreboot repository (see below)
- **`FLASHROM`**: path to flashrom binary
- **`FLASHROM_ARGS`**: in case if flashrom detects multiple flash chips, put
  `"-c CHIP_MODEL"` here
- **`STAGE2_USE_FULL_ME`**: if you want to use original Intel ME image in the
  final ROM for some reason, set to `1`

#### stage1

Get coreboot:
```
$ git clone --recurse-submodules https://review.coreboot.org/coreboot.git && cd coreboot
```

Build coreboot toolchain. You must have gnat compiler installed, it is required
for graphics initialization (libgfxinit is written in Ada):
```
$ make crossgcc-i386 CPUS=$(nproc)
$ make iasl
```

Dump the flash chip contents:
```
# ./mmga dump
```

**Create a backup of the stock ROM dump on external drive!**

Create patched FD and neutralize ME:
```
$ ./mmga prepare-stage1
```

If your board's port hasn't been merged to coreboot master yet or you don't know,
run:
```
$ ./mmga fetch
```

Create coreboot config and build the ROM:
```
$ ./mmga build-stage1
```

(If you're experienced coreboot user or developer, you may want to configure and
build coreboot yourself. In that case, run `config-stage1` instead of
`build-stage1`. It will create config that you can then copy to `$COREBOOT_PATH`,
make your changes and build.)

Flash it:
```
# ./mmga flash-stage1
```

If it's done and there were no errors, you have to **shutdown** the laptop.
Do not reboot, the new flash descriptor is only active after cold boot, so it
won't work and may lead to weird stuff as you've just messed with firmware. Wait
a few seconds and power it back on.

#### stage2

Make patched flash descriptor for the next flash:
```
$ ./mmga prepare-stage2
```

Create new coreboot config and build the ROM (for experienced users,
`config-stage2` is also available):
```
$ ./mmga build-stage2
```

Flash it:
```
# ./mmga flash-stage2
```

This may take a while, don't interrupt it and let it finish.

Again, if there were no errors, power off the machine again, wait a few seconds,
and power on.

## FAQ

**My device is not listed, will it work?**

No, but it might be possible to support it.

**Will macOS continue to work with coreboot?**

No. It's theoretically possible to turn a corebooted MacBook into a hackintosh
by using TianoCore and Clover or OpenCore. Basically, if you want to use macOS,
don't install coreboot.

**Switching between iGPU and dGPU on MacBook Pro 10,1**

Last time I checked, [hacks](https://wiki.archlinux.org/index.php/MacBookPro10,x#Graphics_2)
were needed to use Linux with integrated GPU on this model. It seems that Apple EFI
forces dGPU when OS is not macOS. I've integrated hybrid graphics driver into
coreboot, it automatically switches to the configured GPU and you don't need to
care about it anymore. By default, integrated GPU is used. The setting is stored
in CMOS and you can change it with `nvramtool`.

Note that to use discrete GPU you need to extract VGA ROM from the stock firmware
dump and add it to CBFS, and configure coreboot to run VGA Option ROMs.

## TODO

- Support custom FMAP for larger first-stage `bios` partition
- Support multiple payloads at once
- Support TianoCore as a payload

## Misc

The script was tested on MacBook Air 5,2, MacBook Pro 8,1 and MacBook Pro 10,1.
If you have successfully corebooted your macbook with it and it worked, please
let me know.

If you have any problems, contact me via GitHub issues or by email (see the
copyright header in the script).

## License

GPLv2

# Which payload to use. Supported payloads: grub, seabios
PAYLOAD=grub

# Example: macbookair5_2
MODEL=

# If empty, default grub.cfg (from this repo) will be used. Skip if you use SeaBIOS.
GRUB_CFG_PATH=

# Path to cloned coreboot repo.
COREBOOT_PATH=

# Path to flashrom binary. Minimal required flashrom version is 1.0.
# Do not use flashrom from Debian repos. Instead, get the latest version from git and build:
#
# git clone https://review.coreboot.org/flashrom.git
# cd flashrom
# make -j$(nproc)
#
FLASHROM=/usr/local/sbin/flashrom

# In case if flashrom detects multiple chips, put "-c CHIP_MODEL" here.
FLASHROM_ARGS=

# If you want to use original Intel ME image in the final (stage2) ROM, set to 1
STAGE2_USE_FULL_ME=0

# Other required utilities. Note, that they need to be built. To do that,
# `cd` to each util's directory and run `make`.
# If COREBOOT_PATH is set correctly, you don't need to edit these values.
IFDTOOL=${COREBOOT_PATH}/util/ifdtool/ifdtool
CBFSTOOL=${COREBOOT_PATH}/util/cbfstool/cbfstool

# This should just work once COREBOOT_PATH is set correctly.
ME_CLEANER=${COREBOOT_PATH}/util/me_cleaner/me_cleaner.py

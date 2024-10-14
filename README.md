
<div align="center">
  <img alt="JetKVM Logo" src="https://jetkvm.com/logo-blue.png" height="40">

**Firmware**

[Website](https://jetkvm.com) • [Issues](https://github.com/jetkvm/kvm/issues) • [Docs](https://jetkvm.com/docs) • [Discord](https://jetkvm.com/discord)

</div>

An official build system for JetKVM firmware, enabling you to compile and customize every aspect of your JetKVM device - from bootloader to applications. This repository provides a comprehensive SDK for building firmware images tailored to your JetKVM device. It handles U-Boot, kernel, root filesystem, media, and optional applications - all under a single repository. 

---

## Quick Start

#### 1. Install dependencies

```bash
sudo apt-get update &&
sudo apt-get install -y --no-install-recommends \
  build-essential \
  device-tree-compiler \
  gperf g++-multilib gcc-multilib \
  libnl-3-dev libdbus-1-dev libelf-dev libmpc-dev dwarves
```

#### 2. Select the Board Configuration

```bash
./build.sh lunch BoardConfig_IPC/BoardConfig-EMMC-NONE-RV1106_JETKVM_V2.mk
```

#### 3. Automatic Compilation

```bash
./build.sh
```

This single command builds U-Boot, the kernel, the root filesystem, and any additional media or applications configured for your JetKVM device. The final images will appear in output/image/.

## Detailed Build Steps

For incremental development or debugging, you can build individual components:

### Build U-Boot

```bash
./build.sh clean uboot
./build.sh uboot
```

Generates:

```
output/image/MiniLoaderAll.bin
output/image/uboot.img
```

### Build Kernel

```bash
./build.sh clean kernel
./build.sh kernel
```

Generates: `output/image/boot.img`

### Build Rootfs

```
./build.sh clean rootfs
./build.sh rootfs
```

Generates: `output/image/rootfs.img`

Then, to package everything (including rootfs.img) into your final JetKVM firmware:

```bash
./build.sh firmware
```

### Build Media

```bash
./build.sh clean media
./build.sh media
```

Generates media-related libraries or drivers in: `output/out/media_out`

### Build Reference Applications

```bash
./build.sh clean app
./build.sh app
```

Generates application binaries in: `output/out/app_out`

> [!NOTE]  
> Reference applications may depend on the media build.

### Firmware Packaging

```bash
./build.sh firmware
```

Assembles all components into flashable images, placed in `output/image/`.

### Output Directory

After building, your output/ directory should look like this:

```
output/
├── image
│   ├── download.bin
│   ├── env.img
│   ├── idblock.img
│   ├── uboot.img
│   ├── boot.img
│   ├── rootfs.img
│   └── userdata.img
└── out
    ├── app_out
    ├── media_out
    ├── rootfs_xxx
    ├── S20linkmount
    ├── sysdrv_out
    └── userdata
```

- `image/` -  Contains final firmware images for flashing onto your JetKVM device.
- `out/` - Holds intermediate build artifacts like compiled apps, media components, and the rootfs packaging directory.

## Configuration Options

All variables for customizing builds—like CPU architecture, boot medium, partitioning, or optional features—are documented in cfg-all-items-introduction.txt

**A few notable examples:**

| Item                | Description |
|-------------------------|---------|
| **RK_ARCH**            | Builds 32-bit (`arm`) or 64-bit (`arm64`) firmware. |
| **RK_BOOT_MEDIUM**     | Specifies the storage medium: `emmc`, `spi_nor`, `spi_nand`, or `slc_nand`. |
| **RK_UBOOT_DEFCONFIG** | Specifies the U-Boot defconfig file. |
| **RK_KERNEL_DEFCONFIG** | Specifies the Kernel defconfig file. |
| **RK_KERNEL_DTS**      | Target DTS (Device Tree) for kernel builds. |
| **RK_PARTITION_CMD_IN_ENV** | Defines partition layouts for the firmware. |
| **RK_APP_TYPE**        | Determines which reference apps to include. |
| **RK_ENABLE_WIFI**     | Enables Wi-Fi functionality for the JetKVM firmware. |
| **RK_ARCH**            | `arm` or `arm64`: Defines whether to compile a 32-bit or 64-bit program. |
| **RK_CHIP**            | **Cannot be modified**: Different chips correspond to different SDKs. |
| **RK_TOOLCHAIN_CROSS** | **Cannot be modified**: Defines the cross-compilation toolchain. |

See the [cfg-all-items-introduction.txt](https://github.com/BuildJet/rv1106-sdk/blob/main/project/cfg-all-items-introduction.txt) file for a complete list of config variables

## Kernel Config
To start the kernel configuration,

```bash
cp ./arch/arm/configs/rv1106-jetkvm-v2_defconfig .config
make ARCH=arm menuconfig
```

After the kernel configuration is complete, you can save the configuration file as follows:

```bash
make ARCH=arm savedefconfig
cp defconfig ./arch/arm/configs/rv1106-jetkvm-v2_defconfig
```

## Contributing

We welcome contributions from the community! Whether it's improving the firmware, adding new features, or enhancing documentation, your input is valuable. We also have some rules and taboos here, so please read this page and our [Code of Conduct](/CODE_OF_CONDUCT.md) carefully.

Please also review our Code of Conduct to keep the community welcoming and constructive.

## Notices

- **Avoid Copying from Windows** - Moving the source code on Windows might break file permissions or symbolic links. Always handle the codebase in a native Linux environment.

Happy Building - With this SDK, you’ll have full control of your JetKVM firmware—tailoring it to your hardware, storage preferences, and custom application needs.

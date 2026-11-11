# Building an initramfs for the kernel

This repo contains the kernel and **only the kernel**, therefore an initramfs must be made by the user in order for the `make`-generated ISO file to be bootable and usable.

## Getting the initramfs

First, you need to obtain an initramfs. You can either build it or download it. Here, I describe two options, choose one.

### Fsstrap

Initramfs building based on Debian packages can be done by [fsstrap](https://github.com/MoreUtopia/fsstrap).

Clone the repository:

```
git clone https://github.com/MoreUtopia/fsstrap
```

Since we are not contributing back to `fsstrap`, we can keep our changes to `Fsstrapfile` local, so edit it with your preffered code editor:

```
# Default one:
$EDITOR Fsstrapfile

# VSCode:
code Fsstrapfile

# Neovim:
nvim Fsstrapfile
```

Look for lines like `AddPackage("xxx")` and `SetInitFile("xxx");` and change them accordingly.

The first one adds a Debian package with a specified name to the initramfs. The second one sets the init file relative to the initramfs; e.g. if you want to launch `bash`, you set it to `SetInitFile("/usr/bin/bash")`.

By default, Fsstrapfile lists two supported by Utopia computer architectures. It is recommended to delete the one that will be unused, for example `riscv64`.

**Ensure `Build()` is the last instruction or else the building will ignore some of your instructions.**

Then run `./fsstrap.sh eval`.

No extra init building step needs to be done, as fsstrap uses [finit](https://github.com/MoreUtopia/finit) as a Utopia-native system init.

### Downloading the initramfs

You can get initramfses or "rootfs"es for different Linux distributions online. Some recommend the rootfs provided by Alpine Linux.

Extract the downloaded initramfs into any folder.

Then create a small Assembly/C wrapper to launch a Linux init executable. I won't get into the details of this, but [finit](https://github.com/MoreUtopia/finit) is a great reference implementation (you may even go and reuse it). Next, compile that wrapper into a file called `init` in the root of your initramfs.

## Copying the initramfs

The initramfs(es) need(s) to be copied into a folder called `initramfs-<arch>` in the Utopia's project root.

## Final touches

As said in the `docs/BUILDING.md`, the bootloader configuration needs to be rebuilt in order for Utopia to detect the initramfses given. Please follow instructions about this topic from that file.

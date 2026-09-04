#!/run/current-system/sw/bin/sh

nix-shell -E '
  with import <nixpkgs> {
    localSystem = "x86_64-linux";
    crossSystem = "riscv64-linux";
  };
  mkShell {}
' --extra-experimental-features flakes

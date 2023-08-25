# Packaging files for various Linux distributions here.
 - ArchLinux https://aur.archlinux.org/packages/translatelocally-git/ maintained by [XapaJIaMnu](https://github.com/XapaJIaMnu).
 - Ubuntu/Debian packaging is done automatically via `cpack` maintained by [XapaJIaMnu](https://github.com/XapaJIaMnu). To produce a .deb do: 
   ```bash
   mkdir build
   cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
   make -j2
   cpack
   ```

# Nix Flake development shell

This directory contains a Nix Flake (`flake.nix`) providing a development shell,
which can be entered with the command `nix develop` in this directory.
Development dependencies will be available until this shell is exited.
Build is done as usual.

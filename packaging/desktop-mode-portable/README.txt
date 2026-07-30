Portable Desktop Mode
=====================

From the mounted iPod, double-click:

  Open Desktop Mode.command  on macOS
  Open Desktop Mode.cmd      on Windows

The launchers run entirely from the iPod. RockPod and a local Rockbox
installation are not required. Desktop Mode reads Music, Videos, Live TV,
tagcache, and videolist data from this mounted volume.

The native host runtime is kept under .rockbox/desktop-host. Its native
plugins and UI resources are isolated from the ARM plugins used by the
physical iPod. Do not move files out of that directory.

On macOS, the package carries separate Apple Silicon and Intel runtimes and
selects the correct one automatically. Windows uses the x86-64 runtime.

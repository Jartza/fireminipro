# minipro CLI (bundled with FireMinipro)

- **Version:** 0.7.4  
- **Commit:** 3088aecb6adac99b6a919b0382e01b2db7a718  
- **Upstream:** <https://gitlab.com/DavidGriffith/minipro>  
- **License:** GPLv3 (see `LICENSE_minipro`)

## Source archive

Place the official source archive at:

```
thirdparty/minipro-0.7.4.tar.bz2
```

This tarball is kept in the repository to satisfy the GPL requirement to
distribute the corresponding source alongside the binary.

## Rebuild instructions (macOS 12)

1. Extract the tarball, e.g.  
   `tar xf thirdparty/minipro-0.7.4.tar.bz2`.
2. Build the bundled copy of libusb (see `README_libusb.md`) first, so the
   correct headers and dylib are available.
3. From the extracted `minipro` source directory:

   ```bash
   export PKG_CONFIG_PATH="$LIBUSB_STAGING/lib/pkgconfig"
   export CFLAGS="$CFLAGS -I$LIBUSB_STAGING/include"
   export LDFLAGS="$LDFLAGS -L$LIBUSB_STAGING/lib"
   make
   ```

4. Copy the resulting `minipro` binary to `Contents/MacOS/`.
5. Copy `logicic.xml`, `infoic.xml`, and other data files to
   `Contents/Resources/minipro/`.
6. Include `LICENSE_minipro` and this README inside
   `Contents/Resources/thirdparty/minipro/`.

These steps reproduce the bundled CLI that ships with FireMinipro.

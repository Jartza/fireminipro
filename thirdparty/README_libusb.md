# libusb (bundled dependency)

- **Version:** 1.0.29  
- **Upstream:** <https://github.com/libusb/libusb>  
- **License:** LGPL-2.1 (see `LICENSE_libusb`)

## Source archive

Source code has been made available at:

```
https://github.com/Jartza/fireminipro/raw/refs/heads/main/thirdparty/libusb-1.0.29.tar.bz2
```

Keeping this tarball in the repository ensures the corresponding source is
available while distributing the binary, as required by the LGPL.

## Rebuild instructions (macOS 12)

1. Extract the tarball, e.g.  
   `tar xf thirdparty/libusb-1.0.29.tar.bz2`.
2. Configure with a staging prefix to avoid system installation:

   ```bash
   ./configure --prefix="$PWD/install"
   make
   make install
   ```

3. Copy the resulting library to the app bundle:

   ```bash
   cp install/lib/libusb-1.0.0.dylib \
      fireminipro.app/Contents/Frameworks/
   ```

4. Update the library ID and the `minipro` binary so the loader finds the
   bundled dylib:

   ```bash
   install_name_tool -id \
     @executable_path/../Frameworks/libusb-1.0.0.dylib \
     fireminipro.app/Contents/Frameworks/libusb-1.0.0.dylib

   install_name_tool -change /usr/local/lib/libusb-1.0.0.dylib \
     @executable_path/../Frameworks/libusb-1.0.0.dylib \
     fireminipro.app/Contents/MacOS/minipro
   ```

5. Include `LICENSE_libusb` and this README inside
   `Contents/Resources/thirdparty/libusb/`.

These steps reproduce the dylib that ships with FireMinipro (AppImage and DMG).

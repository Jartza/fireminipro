# Qt 6 licensing notes

`fireminipro` links dynamically against the following Qt 6 modules:

- Qt6::Core  
- Qt6::Gui  
- Qt6::Widgets  
- Qt6::DBus

These modules are available under the GNU Lesser General Public License v3 (LGPLv3).
When distributing binaries (e.g. AppImage, DMG), make sure the following obligations
are covered:

1. Provide the full text of the LGPLv3 license alongside your build (for example in
   the package `NOTICE` file or `thirdparty` directory).
2. Offer recipients a clear way to obtain the exact Qt sources you used. Pointing to
   Qt's official download mirrors or a copy in this repository is acceptable. If you
   apply patches to Qt, publish those patches under the LGPLv3 as well.
3. Keep Qt as shared libraries/frameworks so users can relink with their own builds.
   Avoid DRM or technical restrictions that would block running with a replacement
   Qt build.

The Qt Project provides source archives at https://download.qt.io/official_releases/.
Document the specific version used in your release notes so users can retrieve the
matching source package easily.


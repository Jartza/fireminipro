# Qt 6 licensing notes

`fireminipro` links dynamically against the following Qt 6 modules:

- Qt6::Core  
- Qt6::Gui  
- Qt6::Widgets  
- Qt6::DBus

The modules above fall under the GNU Lesser General Public License v3 (LGPLv3). FireMinipro
fulfils the relevant terms as follows:

- The DMG/AppImage bundles already ship the LGPLv3 text in `Resources/thirdparty/`.
- Release notes and packaging metadata record the exact Qt version used. Users can
  fetch the matching sources from https://download.qt.io/official_releases/. If a
  future release needs patched Qt sources, those patches must be mirrored here as well.
- All builds link Qt dynamically (frameworks on macOS, `.so` on Linux), so users are
  free to replace the Qt libraries with their own builds. No anti-tamper measures are
  present.

If the project ever adds a Qt module distributed solely under GPL terms, or switches to
static linking, the licensing situation will need to be reassessed.

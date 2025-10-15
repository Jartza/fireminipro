## FireMinipro Release Notes

**Packages provided**
- `FireMinipro-macos-arm64.dmg` – Apple Silicon (macOS 12+)
- `FireMinipro-macos-x86_64.dmg` – Intel (macOS 12+)
- `FireMinipro-x86_64.AppImage` – Linux (x86_64)

All builds bundle the open-source **minipro 0.7.4** CLI and **libusb 1.0.29**, along with their data files (`logicic.xml`, `infoic.xml`). Licence notices and the corresponding source archives live under `Contents/Resources/thirdparty/` (and in the repository’s `thirdparty/` folder) to satisfy GPLv3/LGPL requirements.

---

### Linux

Download the AppImage, then:

```bash
chmod +x FireMinipro-x86_64.AppImage
./FireMinipro-x86_64.AppImage
```

The AppImage is fully self-contained; it should run on a clean Ubuntu/Debian install with no additional packages.

---

### macOS (Intel & Apple Silicon)

Because the DMG is ad-hoc signed, macOS will block the first launch. Allow it via:

1. Open the `.dmg` and drag `fireminipro.app` into **Applications**.
2. Launch it once from Applications—macOS shows a warning dialog.
3. Open **System Settings → Privacy & Security** (use the link in the dialog’s `?` help).
4. Scroll down to “fireminipro.app was blocked…” and click **Open Anyway**.
5. Confirm in the new dialog (also **Open Anyway**) and authenticate.
6. Future launches will succeed normally.

No additional Homebrew prerequisites are required: the DMG already contains `minipro`, `libusb`, and the Minipro XML data.

---

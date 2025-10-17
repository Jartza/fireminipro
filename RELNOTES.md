## FireMinipro Release Notes

Find releases from [here](https://github.com/Jartza/fireminipro/releases/)

**Packages provided**
- `fireminipro_<ver>.dmg` – MacOs 12 and later, univeral binary (Apple Silicon and Intel)
- `FireMinipro-<ver>-x86_64.AppImage` – Linux AppImage (x86_64)

All builds bundle the open-source **minipro** CLI and **libusb**, along with minipro's data files (`logicic.xml`, `infoic.xml`). Licence notices and the corresponding source archives live under `Contents/Resources/thirdparty/` (and in the repository’s `thirdparty/` folder) to satisfy GPLv3/LGPL requirements.

---

### Linux

Download the AppImage, then: (replace \<ver\> with real file version number)

```bash
chmod +x FireMinipro-<ver>-x86_64.AppImage
./FireMinipro-<ver>-x86_64.AppImage
```

The AppImage is fully self-contained; it should run on a clean Ubuntu/Debian install with no additional packages.

---

### macOS (Intel & Apple Silicon)

Because the DMG is ad-hoc signed, macOS will block the first launch. Allow it via:

- Open the `.dmg` and drag `fireminipro.app` into **Applications**.

Two options to make it work, either:

1. Launch it once from Applications—macOS shows a warning dialog.
2. Open **System Settings → Privacy & Security** (use the link in the dialog’s `?` help).
3. Scroll down to “fireminipro.app was blocked…” and click **Open Anyway**.
4. Confirm in the new dialog (also **Open Anyway**) and authenticate.
5. Future launches will succeed normally.

Of if you are more at home with Terminal / command-line:

`sudo xattr -dr com.apple.quarantine /Applications/fireminipro.app`

No additional Homebrew prerequisites are required: the DMG already contains `minipro`, `libusb`, and the Minipro XML data.

---

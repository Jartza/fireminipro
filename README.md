# FireMinipro

**FireMinipro** is a modern, cross-platform graphical front-end for the [Minipro](https://gitlab.com/DavidGriffith/minipro) programmer software.  
It is designed to be fast, intuitive, and lightweight — with special emphasis on **buffer management** and **visual clarity**.

Unlike traditional programmer utilities, FireMinipro lets you view, merge, and manipulate binary data buffers easily before writing to or after reading from an EPROM, EEPROM, or logic IC.  
It aims to provide a comfortable workflow for both hobbyists and professionals working with TL866, T48, T56, and compatible programmers.

---

## Screenshots

![Main Window](img/main.png)
![Dialog Example](img/dialog.png)

---

## Features

- **Buffer visualization:** Load multiple binary files at arbitrary offsets, visualize overlaps, and highlight padding.
- **Flexible file operations:** Load, clear, merge, and save buffers of any size.
- **Device awareness:** Automatically detects connected Minipro programmers and supported device lists.
- **Chip info:** Displays memory size, package, protocol, and voltage details directly from Minipro.
- **Device operations:** Blank check, erase, logic test, read, and write functions fully integrated.
- **Progress tracking:** Live progress and operation status while reading or writing chips.
- **Cross-platform support:** Works on both macOS and Linux using Qt6.

---

## Prerequisites

### macOS
Install dependencies using [Homebrew](https://brew.sh/):

```bash
brew install cmake ninja qt6 minipro
```

### Linux (Ubuntu/Debian)
You need Minipro and the development toolchain for Qt6 and CMake.  
Install required packages using:

```bash
sudo apt update
sudo apt install build-essential ninja-build cmake libgl1-mesa-dev qt6-base-dev libxkbcommon-dev
```

> **Note:**  
> The `minipro` package is not available in most distributions by default.  
> You can install it manually by cloning and building from source:
> ```bash
> git clone https://gitlab.com/DavidGriffith/minipro.git
> cd minipro
> make
> sudo make install
> ```

---

## Building FireMinipro

### 1. Clone the repository
```bash
git clone https://github.com/yourusername/fireminipro.git
cd fireminipro
```

### 2. Configure and build
FireMinipro uses **CMake** and **Ninja** for fast, cross-platform builds.

```bash
cmake -B build -G Ninja
cmake --build build
```

### 3. Run the application
On macOS:
```bash
./build/fireminipro.app/Contents/MacOS/fireminipro
```

On Linux:
```bash
./build/fireminipro
```

---

## Using FireMinipro

1. **Connect** your supported programmer (T48, TL866II+, etc.).  
2. FireMinipro automatically detects connected devices at startup.  
3. **Select** your target chip and use the built-in *Device Operations* to:
   - Read chip contents into the buffer.
   - Write buffer data to the chip.
   - Load files into buffer at freely definable offset
   - Perform read, write, erase, blank check, or logic IC test operations.
4. **Edit or combine** ROM/binary file data directly in the buffer before writing.

---

## Notes

- Current version is based on few days of work and might not yet be complete or even usable for all
  purposes. Please report issues!
- FireMinipro uses `minipro` under the hood, so your connected programmer must be supported by Minipro.
- On macOS, the app includes a custom icon and can be bundled as a `.app` package for easier launching.
- On Linux, the icon will appear in most desktop environments after installation (when packaged later).

---

## License

This project is released under the MIT License.  
See [LICENSE](LICENSE) for details.

---

## Acknowledgements

FireMinipro builds upon the open-source **Minipro** utility by [David Griffith](https://gitlab.com/DavidGriffith/minipro)  
and uses **Qt6** for its cross-platform GUI.

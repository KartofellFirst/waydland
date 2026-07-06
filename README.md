![License](https://img.shields.io/badge/license-MIT-blue.svg)
![C](https://img.shields.io/badge/C-99-blue)
![GTK](https://img.shields.io/badge/GTK-4.0-green)
![Wayland](https://img.shields.io/badge/Wayland-native-blue)
![Status](https://img.shields.io/badge/status-alpha-orange)

# Waydland

A Wayland dynamic island component with a mission to replace waybar-like menus, built with the Hyprland philosophy to expand the no-mouse-use concept. Written in pure C with GTK4.

**⚠️ Currently in active development**

---

## Features

- 🏝️ **Dynamic Island** — animated pill-shaped container, doesn't take up your screenspace as waybar 
- 🎯 **No-mouse philosophy** — designed for keyboard-driven workflows (tho optimised for mouse-use too)
- 🎨 **Smooth animations** — fade transitions and expand/collapse effects (still in progress)
- 📦 **Modular architecture** — built-in applets with plugin support (waybar plugins inteprotator in progress)

---

## Requirements

- Linux (Wayland session)
- GTK4 >= 4.0.0
- gtk4-layer-shell >= 1.0.0

### Ubuntu/Debian
```bash
sudo apt install libgtk-4-dev libgtk-4-layer-shell-dev
```

### Arch Linux
```bash
sudo pacman -S gtk4 gtk4-layer-shell
```

### Fedora
```bash
sudo dnf install gtk4-devel gtk4-layer-shell-devel
```

---

## Build & Run

1. Clone the repository:
```bash
git clone https://github.com/KartofellFirst/waydland
cd waydland
```

2. Build with make:
```bash
make
```

3. Run:
```bash
./waydland
```

### Build Options
```bash
make clean      # Remove binary
make install    # Install to /usr/local/bin/
make uninstall  # Remove from /usr/local/bin/
```

---

## Project Structure

```
waydland/
├── src/
│   ├── main.c          # Entry point
│   ├── island.c        # Core island logic
│   └── island.h        # Public API
├── Makefile            # Build system
├── LICENSE             # MIT License
└── README.md           # This file
```

---

## Usage

### Manual Run
```bash
./waydland
```

### Autostart with Hyprland

Add to your `~/.config/hypr/hyprland.conf`:
```ini
exec-once = /path/to/waydland
```

Replace `/path/to/` with the actual absolute path to the binary.

### Navigation
- **Hover** — arrows appear for navigation
- **Left/Right arrows** — switch between applets
- **Click applet** — expand/collapse detailed view
- **Click settings (⚙)** — open settings panel

---

## Configuration

**⚠️ Coming soon** — planned config file support:

- `~/.config/waydland/waydland.conf` — island settings (margins, height, theme)
- `~/.config/waydland/modules/*.conf` — per-module configuration

---


## Contributing

Found a bug or have an idea? [Open an issue](https://github.com/KartofellFirst/waydland/issues)

Pull requests are welcome!

---

## License

MIT License — see [LICENSE](LICENSE) file for details.

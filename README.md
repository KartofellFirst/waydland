![License](https://img.shields.io/badge/license-MIT-blue.svg)
![C](https://img.shields.io/badge/C-99-blue)
![GTK](https://img.shields.io/badge/GTK-4.0-green)
![Wayland](https://img.shields.io/badge/Wayland-native-blue)
![Status](https://img.shields.io/badge/status-alpha-orange)
# Waydland

A Wayland dynamic island component with a mission to replace waybar-like menus, built with the Hyprland philosophy to expand the no-mouse-use concept. Written in pure C with GTK4.

**⚠️ Currently in active development** 

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

2. Build with GCC:
```bash
gcc -o waydland main.c $(pkg-config --cflags --libs gtk4 gtk4-layer-shell-0)
```

3. Run:
```bash
./waydland
```

---

## Autostart with Hyprland

Add to your `~/.config/hypr/hyprland.conf`:

```ini
exec-once = /home/yourusername/path/to/waydland
```

Replace `/home/yourusername/path/to/` with the actual absolute path to the binary.

---

## Configuration

*(Coming soon — planned config file support)*

---

## License

MIT License — see [LICENSE](LICENSE) file for details.

---

Found a bug? [Open an issue](https://github.com/KartofellFirst/waydland/issues)


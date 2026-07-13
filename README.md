![License](https://img.shields.io/badge/license-MIT-blue.svg)
![C](https://img.shields.io/badge/C-99-blue)
![GTK](https://img.shields.io/badge/GTK-4.0-green)
![Wayland](https://img.shields.io/badge/Wayland-native-blue)
![Status](https://img.shields.io/badge/status-alpha-orange)

# Waydland

A native Wayland Dynamic Island component built to replace static, screen-wasting panels like Waybar. Designed with the Hyprland philosophy in mind, Waydland expands the no-mouse workflow while providing a highly modular, interactive environment. Written in pure C with native GTK4 and Cairo.

**⚠️ Currently in active development (Python Alpha)**

---
<h2>
🏝️ Why Waydland?
<img src="https://duiqt.github.io/herta_kuru/static/img/hertaa1.gif" width="30px">
</h2>

You need to monitor **time**, **RAM**, **battery**, or other simple stats? Or just want to display a GIF of Herta spinning whenever the Kuru Kuru song plays? You don't have to rely on Conky or Waybar. Waydland is exactly what you need — a minimalistic monitor that lives in the top-right corner of your screen, displaying the information you need, plus some extra features you'll definitely like!

- **Extremely lightweight** — Applets in Waydland are independent, non-looping, auto-updating apps that can be literally updated by your fastfetch. No complicated structure is needed to create your own based on your favorite bash utility.

- **Keyboard-driven** — Unlike other trays, Waydland is fully keyboard-driven and can be set up blazingly fast without reaching for the window to focus it (just add a keybind via your system config).

- **Dynamic Applet Slot** — Temporary background tasks or attention-valuable monitors will auto-show when it's important (e.g., low battery percentage, alarm).

- **Hide when unnecessary** — Bind a global combination to hide or fade Waydland whenever you need. Set it up to hide when you watch full-screen video or play games.

And even more features! → See [PHILOSOPHY.md](docs/PHILOSOPHY.md).

---

## 📋 Requirements

- Linux (Wayland session)
- Python 3.10 or higher
- **GTK4** and **`gtk4-layer-shell`** libraries

### Install System Dependencies

#### Ubuntu/Debian
```bash
sudo apt install libgtk-4-dev libgtk-4-layer-shell-dev python3-gi python3-gi-cairo gir1.2-gtk-4.0
```

#### Arch Linux
```bash
sudo pacman -S gtk4 gtk4-layer-shell python-gobject
```

#### Fedora
```bash
sudo dnf install gtk4-devel gtk4-layer-shell-devel python3-gobject
```

## 🛠️ Run

1. Clone the repository:

```bash
git clone https://github.com/KartofellFirst/waydland
cd waydland
```

2. (Optional) Create and activate a virtual environment:

```bash
python -m venv venv
source venv/bin/activate
```

3. Install Python dependencies:

```bash
pip install -r requirements.txt
```

4. Run:

```bash
python main.py
```

## ⌨️ Usage & Navigation

### Manual Run

```bash
python main.py
```

### Autostart with Hyprland

Add the following to your `~/.config/hypr/hyprland.conf`:

```ini
exec-once = python /path/to/waydland/main.py
```

### Navigation Rules

* **Hover** — Contextual navigation arrows appear if the dynamic tray has overflow items.
* **Left/Right Arrows** — Cycle through your active dynamic applets.
* **Click/Select Applet** — Expand or collapse the detailed native GTK view.

## 🔧 Configuration

**⚠️ Coming soon** — Planned declarative config file support:

* `~/.config/waydland/waydland.conf` — Island global configurations (margins, default height, physics easing curves).
* `~/.config/waydland/modules/*.json` — Per-module declarative JSON architecture payloads.

---

## 🤝 Contributing

Found a bug or want to help with C part? [Open an issue](https://github.com/KartofellFirst/waydland/issues). Pull requests are always welcome!

---

## 📄 License

Distributed under the MIT License. See the [LICENSE](LICENSE) file for details.

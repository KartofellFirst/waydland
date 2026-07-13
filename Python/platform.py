import sys
import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk

# Gracefully handle the absence of layer-shell (e.g., when testing on macOS)
try:
    gi.require_version('Gtk4LayerShell', '1.0')
    from gi.repository import Gtk4LayerShell
    HAS_LAYER_SHELL = True
except ValueError:
    HAS_LAYER_SHELL = False

class PlatformWindow:
    def setup(self, window: Gtk.Window):
        raise NotImplementedError

class LayerShellPlatform(PlatformWindow):
    def setup(self, window: Gtk.Window):
        if not HAS_LAYER_SHELL:
            return

        Gtk4LayerShell.init_for_window(window)
        Gtk4LayerShell.set_namespace(window, "waydland-island")

        # Changed to OVERLAY per your C code
        Gtk4LayerShell.set_layer(window, Gtk4LayerShell.Layer.OVERLAY)

        # Anchored TOP and RIGHT
        Gtk4LayerShell.set_anchor(window, Gtk4LayerShell.Edge.TOP, True)
        Gtk4LayerShell.set_anchor(window, Gtk4LayerShell.Edge.RIGHT, True)
        Gtk4LayerShell.set_anchor(window, Gtk4LayerShell.Edge.LEFT, False)
        Gtk4LayerShell.set_anchor(window, Gtk4LayerShell.Edge.BOTTOM, False)

        # Set keyboard mode to NONE
        Gtk4LayerShell.set_keyboard_mode(window, Gtk4LayerShell.KeyboardMode.NONE)

class MacPlatform(PlatformWindow):
    def setup(self, window: Gtk.Window):
        window.set_decorated(False)
        # Provide a fallback size so the window manager doesn't collapse it
        window.set_default_size(0, 0)
def get_platform() -> PlatformWindow:
    # If on macOS, or if the layer-shell library isn't installed on Linux, fallback
    if sys.platform == "darwin" or not HAS_LAYER_SHELL:
        return MacPlatform()
    return LayerShellPlatform()

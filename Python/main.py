import sys
import ctypes

# 1. Force the linker to load layer-shell BEFORE Wayland gets initialized
try:
    ctypes.CDLL("libgtk4-layer-shell.so", mode=ctypes.RTLD_GLOBAL)
except OSError:
    print("Warning: libgtk4-layer-shell.so not found via ctypes.")

from app import WaydlandApp

if __name__ == '__main__':
    app = WaydlandApp()
    sys.exit(app.run(sys.argv))

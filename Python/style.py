import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, Gdk
import configparser

CONFIG_PATH = "waydland.conf"

config = configparser.ConfigParser()
config.read(CONFIG_PATH)

# Spacing
PADDING_X = str(config.get("spacing", "PADDING_X", fallback="15"))
PADDING_Y = str(config.get("spacing", "PADDING_Y", fallback="15"))
DEFORM_PADDING = int(config.get("spacing", "DEFORM_PADDING", fallback=10))
DEFORM_FALLOFF = float(config.get("deform properties", "FALLOFF", fallback="2.0"))
DEFORM_WEIGHT = float(config.get("deform properties", "WEIGHT", fallback="1.0"))
DEFORM_QUALITY = float(config.get("deform properties", "QUALITY", fallback="1.0"))
DEFORM_SAMPLES = config.get("deform properties", "SAMPLES", fallback=None)
DEFORM_STRENGTH = config.get("deform properties", "STRENGTH", fallback=None)
DEFORM_INFLUENCE = float(config.get("deform properties", "INFLUENCE", fallback="40.0"))
MARGIN_RIGHT = str(config.get("spacing", "MARGIN_X", fallback="10"))
MARGIN_TOP = str(config.get("spacing", "MARGIN_Y", fallback="10"))
EXPANDED_WIDTH = int(config.get("spacing", "EXPANDED_WIDTH", fallback=400))
EXPANDED_HEIGHT = int(config.get("spacing", "EXPANDED_HEIGHT", fallback=300))
EXPANDED_ROUNDING = int(config.get("spacing", "EXPANDED_ROUNDING", fallback=50))
INPUT_EXTRA = int(config.get("spacing", "INPUT_EXTRA", fallback=5))

BACKGROUND_COLOR = str(config.get("style", "BACKGROUND_COLOR", fallback="#000"))
COLOR = str(config.get("style", "COLOR", fallback="#fff"))
ARROW_COLOR = str(config.get("style", "ARROW_COLOR", fallback="#fff"))
ARROW_HOVER_OPACITY = str(config.get("style", "ARROW_HOVER_OPACITY", fallback="0.3"))
PILL_BORDER_WIDTH = str(config.get("style", "PILL_BORDER_WIDTH", fallback="0"))
PILL_BORDER_COLOR = str(config.get("style", "PILL_BORDER_COLOR", fallback="white"))
APPLETS_BORDER_WIDTH = str(config.get("style", "APPLETS_BORDER_WIDTH", fallback="0"))
APPLETS_BORDER_COLOR = str(config.get("style", "APPLETS_BORDER_COLOR", fallback="white"))
APPLET_BACKGROUND = str(config.get("style", "APPLET_BACKGROUND", fallback="transparent"))
APPLET_BACKGROUND_HOVER = str(config.get("style", "APPLET_BACKGROUND_HOVER", fallback="#222"))


def load_css():
    provider = Gtk.CssProvider()
    
    css = """
    window {
        background: transparent;
        margin-top: %(MARGIN_TOP)spx;
        margin-right: %(MARGIN_RIGHT)spx;
    }
    revealer {
        background: transparent;
    }
    #pill {
        background-color: transparent;
        border-radius: 9999px;
        color: %(COLOR)s;
        transition: all 250ms cubic-bezier(0.25, 1, 0.5, 1);
        border: %(PILL_BORDER_WIDTH)spx solid %(PILL_BORDER_COLOR)s;
    }
    .main-container {
        background-color: transparent;
        border-radius: 99px;
        border: %(APPLETS_BORDER_WIDTH)spx solid %(APPLETS_BORDER_COLOR)s;
        padding: %(PADDING_Y)spx %(PADDING_X)spx;
    }
    #arrow-left {
        border-left: 2px solid %(ARROW_COLOR)s;
        border-radius: 50px;
    }
    #arrow-right {
        border-right: 2px solid %(ARROW_COLOR)s;
        border-radius: 50px;
    }
    .mini-applet, .arrow-revealer {
        background: %(APPLET_BACKGROUND)s;
        border-radius: 9999px;
        transition: all .2s ease;
    }
    .mini-applet:hover {
        background: %(APPLET_BACKGROUND_HOVER)s;
        transform: scale(1.1);
    }
    .arrow-revealer:hover {
        opacity: %(ARROW_HOVER_OPACITY)s;
        transform: scale(1.5);
    }
    .webview {
        background: %(BACKGROUND_COLOR)s;
    }
    """ % {
        'MARGIN_TOP': MARGIN_TOP,
        'MARGIN_RIGHT': MARGIN_RIGHT,
        'PADDING_X': PADDING_X,
        'PADDING_Y': PADDING_Y,
        'BACKGROUND_COLOR': BACKGROUND_COLOR,
        'COLOR': COLOR,
        'ARROW_COLOR': ARROW_COLOR,
        'ARROW_HOVER_OPACITY': ARROW_HOVER_OPACITY,
        'PILL_BORDER_WIDTH': PILL_BORDER_WIDTH,
        'PILL_BORDER_COLOR': PILL_BORDER_COLOR,
        'APPLETS_BORDER_WIDTH': APPLETS_BORDER_WIDTH,
        'APPLETS_BORDER_COLOR': APPLETS_BORDER_COLOR,
        'APPLET_BACKGROUND': APPLET_BACKGROUND,
        'APPLET_BACKGROUND_HOVER': APPLET_BACKGROUND_HOVER,
    }
    
    provider.load_from_data(css.encode('UTF-8'))
    
    Gtk.StyleContext.add_provider_for_display(
        Gdk.Display.get_default(),
        provider,
        Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
    )

def hex_to_rgba(hex_color, alpha=1.0):
    """
    Конвертирует hex-строку в кортеж (r, g, b, a)
    Поддерживает форматы: #RGB, #RRGGBB, #RRGGBBAA, rgba(R,G,B,A)
    """
    hex_color = hex_color.strip()
    
    # Если уже в формате rgba(R,G,B,A)
    if hex_color.startswith("rgba("):
        parts = hex_color[5:-1].split(",")
        if len(parts) == 4:
            return tuple(float(p.strip()) / 255.0 if i < 3 else float(p.strip()) for i, p in enumerate(parts))
    
    # Если это название цвета
    if not hex_color.startswith("#"):
        # Простой маппинг базовых цветов
        color_map = {
            "transparent": (0, 0, 0, 0),
            "black": (0, 0, 0, 1),
            "white": (1, 1, 1, 1),
            "red": (1, 0, 0, 1),
            "green": (0, 1, 0, 1),
            "blue": (0, 0, 1, 1),
        }
        return color_map.get(hex_color.lower(), (0, 0, 0, 1))
    
    # Убираем #
    hex_color = hex_color.lstrip("#")
    
    # Конвертируем в зависимости от длины
    if len(hex_color) == 3:  # #RGB
        r = int(hex_color[0] * 2, 16) / 255.0
        g = int(hex_color[1] * 2, 16) / 255.0
        b = int(hex_color[2] * 2, 16) / 255.0
        return (r, g, b, alpha)
    
    elif len(hex_color) == 6:  # #RRGGBB
        r = int(hex_color[0:2], 16) / 255.0
        g = int(hex_color[2:4], 16) / 255.0
        b = int(hex_color[4:6], 16) / 255.0
        return (r, g, b, alpha)
    
    elif len(hex_color) == 8:  # #RRGGBBAA
        r = int(hex_color[0:2], 16) / 255.0
        g = int(hex_color[2:4], 16) / 255.0
        b = int(hex_color[4:6], 16) / 255.0
        a = int(hex_color[6:8], 16) / 255.0
        return (r, g, b, a)
    
    return (0, 0, 0, alpha)  # fallback


BACKGROUND_COLOR_RGBA = hex_to_rgba(BACKGROUND_COLOR)
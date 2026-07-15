import json
import math
import gi
import cairo
import sys 
import configparser

if sys.platform == 'darwin':  # macOS does not support WebKit
    USE_WEBVIEW = False  
else:  # Linux
    gi.require_version('WebKit', '6.0')
    from gi.repository import WebKit
    USE_WEBVIEW = True

gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib, Gdk
from applet_engine import GenericApplet
from config_watcher import ConfigReloader


# --- CONFIGURATION ENGINE ---
class AppletManager:
    """Handles discovery, configuration parsing, and instantiation of applets."""
    @staticmethod
    def load_from_config(config_path="applets/config.json"):
        try:
            with open(config_path, 'r') as f:
                paths = json.load(f)
            return [GenericApplet(path) for path in paths] if paths else []
        except (FileNotFoundError, json.JSONDecodeError):
            return []


# --- THE MAIN COMPONENT ---
class IslandWidget(Gtk.Box):
    def __init__(self):
        super().__init__(orientation=Gtk.Orientation.HORIZONTAL)
        self.set_name("island-container")
        
        # Pin widget to Top-Right to force expansion leftward and downward
        self.set_halign(Gtk.Align.END)
        self.set_valign(Gtk.Align.START)

        self._config_reloader = ConfigReloader()
        self._config_reloader.add_observer(self)

        self.set_can_target(True)
        self.set_focusable(True)
        
        # Initialize internal structural variables
        self._init_state()
        self._build_architecture()
        self._build_foreground_ui()
        self._setup_controllers()
        
        self._config_reloader.start_watching()

        # Start animation frame callback
        self.add_tick_callback(self.on_animation_frame)


    def _init_state(self):
        """Isolates application state and animation variables."""
        from style import (DEFORM_PADDING, DEFORM_INFLUENCE, DEFORM_FALLOFF, 
        DEFORM_QUALITY, DEFORM_SAMPLES, DEFORM_STRENGTH, DEFORM_WEIGHT, 
        EXPANDED_HEIGHT, EXPANDED_WIDTH, EXPANDED_ROUNDING, INPUT_EXTRA, 
        PADDING_X, PADDING_Y, BACKGROUND_COLOR_RGBA, BACKGROUND_COLOR)

        self.background_color = BACKGROUND_COLOR_RGBA
        self.background_color_raw = BACKGROUND_COLOR

        self.is_expanded = False
        
        # Animation dimension trackers
        self.current_w = 0
        self.current_h = 0
        self.collapsed_h = 0
        self.current_alpha = 0.0
        
        self.expanded_w = EXPANDED_WIDTH
        self.expanded_h = EXPANDED_HEIGHT
        self.actual_padding_x, self.actual_padding_y = int(PADDING_X), int(PADDING_Y)
        self.expanded_rounding = EXPANDED_ROUNDING
        self.input_extra = INPUT_EXTRA
        self.target_alpha = 0.0

        # Cursor tracking for liquid deformation
        self.cursor_x = -1000.0  # Start far offscreen
        self.cursor_y = -1000.0
        self.cursor_active = False
        
        # Deformation tuning 
        self.deform_influence = DEFORM_INFLUENCE # how far cursor affects edge (pixels)
        self.deform_strength = DEFORM_STRENGTH # maximum bulge amount (pixels)
        self.deform_falloff = DEFORM_FALLOFF # 1=linear, 2=quadratic, 3=cubic falloff def = 2.0
        self.deform_padding = DEFORM_PADDING # cut out some space for simulation
        self.deform_samples = DEFORM_SAMPLES # cant be visible below 3. Chose the samples amount or put None to turn on quality
        self.deform_quality = DEFORM_QUALITY # if samples not set, quality autocalculates them
        self.deform_weight = DEFORM_WEIGHT # if strength not set, weight defines it (0 to 1)

    def on_config_reloaded(self):
        """Вызывается при изменении конфигурационного файла."""
        print("Configuration reloaded, updating widget...")
        
        from style import (
            DEFORM_PADDING, DEFORM_INFLUENCE, DEFORM_FALLOFF, 
            DEFORM_QUALITY, DEFORM_SAMPLES, DEFORM_STRENGTH, 
            DEFORM_WEIGHT, EXPANDED_HEIGHT, EXPANDED_WIDTH, 
            EXPANDED_ROUNDING, INPUT_EXTRA, PADDING_X, PADDING_Y,
            BACKGROUND_COLOR_RGBA, BACKGROUND_COLOR
        )

        self.background_color = BACKGROUND_COLOR_RGBA
        self.background_color_raw = BACKGROUND_COLOR
        
        # Обновляем внутреннее состояние
        self.deform_influence = DEFORM_INFLUENCE
        self.deform_strength = DEFORM_STRENGTH
        self.deform_falloff = DEFORM_FALLOFF
        self.deform_padding = DEFORM_PADDING
        self.deform_samples = DEFORM_SAMPLES
        self.deform_quality = DEFORM_QUALITY
        self.deform_weight = DEFORM_WEIGHT
        
        self.expanded_w = EXPANDED_WIDTH
        self.expanded_h = EXPANDED_HEIGHT
        self.actual_padding_x, self.actual_padding_y = int(PADDING_X), int(PADDING_Y)
        self.expanded_rounding = EXPANDED_ROUNDING
        self.input_extra = INPUT_EXTRA
        
        # Обновляем размеры WebView при необходимости
        if hasattr(self, 'webv'):
            self.webv.set_size_request(
                self.expanded_w - self.actual_padding_x * 2, 
                self.expanded_h - self.actual_padding_y * 2
            )
        
        # Принудительно обновляем отрисовку
        self.canvas.queue_draw()
        
        # Обновляем CSS если нужно
        from style import load_css
        load_css()

        # пересчитать после обновления стилей
        self.collapsed_h = self.current_h

    def _build_architecture(self):
        """Creates the background/foreground layering stack."""
        self.pill = Gtk.Overlay()
        self.pill.set_name("pill")

        # 1. Background Layer (Vector Canvas)
        self.canvas = Gtk.DrawingArea()
        self.canvas.set_draw_func(self.draw_vector_pill)
        self.canvas.set_size_request(int(self.current_w), int(self.current_h))
        self.pill.set_child(self.canvas)

        # 2. Stiff Alignment Layer (Prevents the overlay from forcing dynamic size constraints)
        align_wrapper = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        align_wrapper.set_halign(Gtk.Align.END)   # Keep right boundary completely stiff
        align_wrapper.set_valign(Gtk.Align.START) # Keep top boundary completely stiff
        align_wrapper.set_size_request(0, 0)

        # 3. Foreground Layer (Interactive Content)
        self.foreground = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.foreground.set_halign(Gtk.Align.CENTER) 
        self.foreground.set_valign(Gtk.Align.START)
        
        # Assemble decoupled layout tree
        align_wrapper.append(self.foreground)
        self.pill.add_overlay(align_wrapper)
        
        self.pill.set_measure_overlay(align_wrapper, False)
        self.append(self.pill)

    def _build_foreground_ui(self):
        """Assembles the interactive interface elements."""
        # Top Compact Bar Container
        self.main_container = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        self.main_container.set_halign(Gtk.Align.END)  
        self.main_container.set_valign(Gtk.Align.FILL)   
        self.main_container.set_css_classes(["main-container"])
        self.main_container.set_size_request(-1, -1) 
        
        # Assemble Revealing Nav Arrows
        self.left_revealer = self._create_arrow_revealer("arrow-left", Gtk.RevealerTransitionType.CROSSFADE, Gtk.Align.START)
        self.right_revealer = self._create_arrow_revealer("arrow-right", Gtk.RevealerTransitionType.CROSSFADE, Gtk.Align.END)

        # Populate Applet Tray
        self.static_applets = Gtk.Box(spacing=10) 
        applets = AppletManager.load_from_config()
        
        if not applets:
            spacer = Gtk.Box()
            spacer.set_size_request(50, 15)
            self.static_applets.append(spacer)
        else:
            for applet in applets:
                click_gesture = Gtk.GestureClick()
                applet.compact_widget.add_controller(click_gesture)
                click_gesture.connect("pressed", lambda g, n, x, y, app=applet: self.on_applet_click(app))
                applet.compact_widget.set_can_target(True)
                applet.compact_widget.set_css_classes(["mini-applet"])
                self.static_applets.append(applet.compact_widget)

        # Pack Top Bar
        self.main_container.append(self.left_revealer)
        self.main_container.append(self.static_applets)
        self.main_container.append(self.right_revealer)
        self.current_w = self.main_container.get_allocated_width()
        self.current_h = self.main_container.get_allocated_height()
        self.collapsed_h = self.current_h
        self.current_alpha = 0.0

        # Bottom Expanded Panel Details Container
        self.details = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.details.set_opacity(1.0) 
        
        self.webv = WebKit.WebView() if USE_WEBVIEW else Gtk.Box()
        if not USE_WEBVIEW: 
            self.webv.append(Gtk.Label(label="HTML here")) # заглушка под макос
            self.webv.get_first_child().set_wrap(True) 
            self.webv.get_first_child().set_size_request(self.expanded_w - self.actual_padding_x * 2, self.expanded_h - self.actual_padding_y * 2)
        self.webv.set_size_request(self.expanded_w - self.actual_padding_x * 2, self.expanded_h - self.actual_padding_y * 2)
        self.webv.set_vexpand(False)
        self.webv.set_hexpand(False)
        self.webv.set_css_classes(["webview"])
        self._apply_margins(self.webv, end=self.actual_padding_x)
        self.details.append(self.webv) 
            
        # Final layout alignment
        self.foreground.append(self.main_container)
        self.foreground.append(self.details)

    def update_html(self, code="<p>Hello!</p>"):
        if USE_WEBVIEW:
            self.webv.load_html(f"<html><head><style>* {"{" + f"background-color: {self.background_color_raw};color: #333;" + "}" }</style></head><body>{code}</body></html>")
            return
        self.webv.get_first_child().set_label(code[:100])

    def _setup_controllers(self):
        """Attaches gestures, event controllers, and actions."""
        # Global Window Hover Events - no more in use
        # self.motion_ctrl = Gtk.EventControllerMotion()
        # self.motion_ctrl.connect("enter", self.on_hover_enter)
        # self.motion_ctrl.connect("leave", self.on_hover_leave)
        # self.motion_ctrl.set_propagation_phase(Gtk.PropagationPhase.CAPTURE)
        # self.add_controller(self.motion_ctrl)

        # Navigation Arrow Click Handlers
        self.left_arrow_click = Gtk.GestureClick()
        self.left_revealer.add_controller(self.left_arrow_click)
        self.left_revealer.set_can_target(True)
        self.left_arrow_click.connect("pressed", lambda *args: print("left"))

        self.right_arrow_click = Gtk.GestureClick()
        self.right_revealer.add_controller(self.right_arrow_click)
        self.right_arrow_click.connect("pressed", lambda *args: print("right"))

        # Cursor position tracker for liquid deformation
        self.pill_motion = Gtk.EventControllerMotion()
        self.pill_motion.connect("motion", self.on_pill_motion)
        self.pill_motion.connect("enter", self.on_pill_enter)
        self.pill_motion.connect("leave", self.on_pill_leave)
        self.pill.add_controller(self.pill_motion)

        self.key_ctrl = Gtk.EventControllerKey()
        self.key_ctrl.set_propagation_phase(Gtk.PropagationPhase.CAPTURE)
        self.key_ctrl.connect("key-pressed", self.on_key_pressed)
        self.key_ctrl.connect("key-released", self.on_key_released)
        self.add_controller(self.key_ctrl)

    # --- UI HELPER METHOD LAYERS ---
    def _create_arrow_revealer(self, name, transition_type, alignment):
        arrow = Gtk.Box()
        arrow.set_name(name)
        arrow.set_size_request(10, 6)
        arrow.set_vexpand(True)
        arrow.set_hexpand(True)
        arrow.set_valign(Gtk.Align.CENTER)
        arrow.set_halign(alignment)
        
        revealer = Gtk.Revealer()
        revealer.set_transition_type(transition_type)
        revealer.set_transition_duration(250)
        revealer.set_child(arrow)
        revealer.set_css_classes(["arrow-revealer"])
        revealer.set_reveal_child(False)
        return revealer

    def _apply_margins(self, widget, top=0, bottom=0, start=0, end=0):
        widget.set_margin_top(top)
        widget.set_margin_bottom(bottom)
        widget.set_margin_start(start)
        widget.set_margin_end(end)

    def _clear_details_container(self):
        while (child := self.details.get_first_child()):
            self.details.remove(child)

    # --- VECTOR RENDERING ---
    def draw_vector_pill(self, area, cr, width, height, *args):
        cr.save()
        
        cr.set_source_rgba(0, 0, 0, 0)
        cr.set_operator(cairo.Operator.CLEAR)
        cr.paint()
        cr.set_operator(cairo.Operator.OVER)

        points = self._generate_rounded_rectangle_perimeter(self.collapsed_h / 2)

        if points:
            cr.set_source_rgba(*self.background_color)
            cr.move_to(points[0][0], points[0][1])
            for i in range(1, len(points)):
                cr.line_to(points[i][0], points[i][1])
            cr.close_path()
            cr.fill()

        cr.restore()

    def _generate_pill_perimeter(self):
        """Генерирует точки периметра пилюли с деформацией под курсор."""
        points = []

        pill_w = self.current_w
        pill_h = self.current_h
        r = pill_h / 2.0
        
        if r <= 0:
            return []
        
        left_cx = r # Everything starts from the right positioned left circle
        rect_w = pill_w - 2 * r # then the main body calculates
        right_cx = left_cx + rect_w # and the second circle ends the chain
        cy = pill_h / 2.0 # everything is on one Y-level
        r -= self.deform_padding # after the calculations, substract the deform padding
        
        arc_samples = max(8, int(self.deform_samples or (math.pi * r * self.deform_quality)))
        
        # --- ЛЕВЫЙ ПОЛУКРУГ (от π/2 до -π/2 через верх, идём против часовой) ---
        # Это даст нам выпуклость ВЛЕВО
        for i in range(arc_samples + 1):
            angle = -math.pi / 2 - (math.pi * i / arc_samples)
            px = left_cx + r * math.cos(angle)
            py = cy + r * math.sin(angle)
            
            dx, dy = self._deform_point(px, py, pill_w, pill_h)
            points.append((px + dx, py + dy))
        
        # --- ВЕРХНЯЯ ГРАНЬ (слева направо) ---
        top_y = self.deform_padding
        top_samples = max(2, int(rect_w * self.deform_quality))
        
        for i in range(top_samples + 1):
            px = left_cx + (rect_w * i / top_samples)
            py = top_y
            
            dx, dy = self._deform_point(px, py, pill_w, pill_h)
            points.append((px + dx, py + dy))
        
        # --- ПРАВЫЙ ПОЛУКРУГ (от -π/2 до π/2 через низ, идём против часовой) ---
        # Это даст нам выпуклость ВПРАВО
        for i in range(arc_samples + 1):
            angle = -math.pi / 2 + (math.pi * i / arc_samples)
            px = right_cx + r * math.cos(angle)
            py = cy + r * math.sin(angle)
            
            dx, dy = self._deform_point(px, py, pill_w, pill_h)
            points.append((px + dx, py + dy))
        
        # --- НИЖНЯЯ ГРАНЬ (справа налево, замыкаем контур) ---
        bottom_y = pill_h - self.deform_padding
        bottom_samples = max(2, int(rect_w * self.deform_quality))
        
        for i in range(bottom_samples, -1, -1):
            px = left_cx + (rect_w * i / bottom_samples)
            py = bottom_y
            
            dx, dy = self._deform_point(px, py, pill_w, pill_h)
            points.append((px + dx, py + dy))
        
        return points

    def _generate_rounded_rectangle_perimeter(self, r):
        """Генерирует точки периметра прямоугольника со скруглёнными углами и деформацией под курсор."""
        points = []

        rect_w = self.current_w
        rect_h = self.current_h

        if rect_h / 2 == r and not self.is_expanded: return self._generate_pill_perimeter() 
        r = min(self.expanded_rounding, rect_h / 2)
        
        if r <= 0 or rect_w <= 0 or rect_h <= 0:
            return []
        
        # Границы внутреннего прямоугольника (без скруглений)
        left_x = r
        right_x = rect_w - r
        top_y = r
        bottom_y = rect_h - r
        
        r_inner = r - self.deform_padding
        if r_inner < 0:
            r_inner = 0
        
        arc_samples = max(8, int(self.deform_samples or (math.pi * r * self.deform_quality)))
        
        # --- ПРАВЫЙ ВЕРХНИЙ УГОЛ (от 3π/2 до 2π, против часовой) ---
        for i in range(arc_samples + 1):
            angle = 3 * math.pi / 2 + (math.pi / 2 * i / arc_samples)
            px = right_x + r_inner * math.cos(angle)
            py = top_y + r_inner * math.sin(angle)
            
            dx, dy = self._deform_point(px, py, rect_w, rect_h)
            points.append((px + dx, py + dy))
        
        # --- ВЕРХНЯЯ ГРАНЬ (справа налево) ---
        top_samples = max(2, int((right_x - left_x) * self.deform_quality))
        for i in range(top_samples, -1, -1):
            px = left_x + ((right_x - left_x) * i / top_samples) if top_samples > 0 else left_x
            py = self.deform_padding
            
            dx, dy = self._deform_point(px, py, rect_w, rect_h)
            points.append((px + dx, py + dy))
        
        # --- ЛЕВЫЙ ВЕРХНИЙ УГОЛ (ыы, хз че тут сделал, но нейронка не смогла довертеть. ) ---
        for i in range(arc_samples + 1):
            angle = math.pi * 3 / 2 - (math.pi / 2 - math.pi / 2 * i / arc_samples)
            px = left_x + r_inner * math.cos(angle)
            py = top_y + r_inner * math.sin(angle)
            
            dx, dy = self._deform_point(px, py, rect_w, rect_h)
            points.append((px + dx, py + dy))
        
        # --- ЛЕВАЯ ГРАНЬ (сверху вниз) ---
        left_samples = max(2, int((bottom_y - top_y) * self.deform_quality))
        for i in range(left_samples + 1):
            px = self.deform_padding
            py = top_y + ((bottom_y - top_y) * i / left_samples) if left_samples > 0 else top_y
            
            dx, dy = self._deform_point(px, py, rect_w, rect_h)
            points.append((px + dx, py + dy))
        
        # --- ЛЕВЫЙ НИЖНИЙ УГОЛ (от π/2 до π, против часовой) ---
        for i in range(arc_samples + 1):
            angle = math.pi / 2 + (math.pi / 2 * i / arc_samples)
            px = left_x + r_inner * math.cos(angle)
            py = bottom_y + r_inner * math.sin(angle)
            
            dx, dy = self._deform_point(px, py, rect_w, rect_h)
            points.append((px + dx, py + dy))
        
        # --- НИЖНЯЯ ГРАНЬ (слева направо) ---
        bottom_samples = max(2, int((right_x - left_x) * self.deform_quality))
        for i in range(bottom_samples + 1):
            px = left_x + ((right_x - left_x) * i / bottom_samples) if bottom_samples > 0 else left_x
            py = rect_h - self.deform_padding
            
            dx, dy = self._deform_point(px, py, rect_w, rect_h)
            points.append((px + dx, py + dy))
        
        # --- ПРАВЫЙ НИЖНИЙ УГОЛ (от π до 3π/2, против часовой) ---
        for i in range(arc_samples + 1):
            angle = math.pi * 2 + (math.pi / 2 * i / arc_samples)
            px = right_x + r_inner * math.cos(angle)
            py = bottom_y + r_inner * math.sin(angle)
            
            dx, dy = self._deform_point(px, py, rect_w, rect_h)
            points.append((px + dx, py + dy))
        
        # --- ПРАВАЯ ГРАНЬ (снизу вверх, замыкаем контур) ---
        right_samples = max(2, int((bottom_y - top_y) * self.deform_quality))
        for i in range(right_samples, -1, -1):
            px = rect_w - self.deform_padding
            py = top_y + ((bottom_y - top_y) * i / right_samples) if right_samples > 0 else top_y
            
            dx, dy = self._deform_point(px, py, rect_w, rect_h)
            points.append((px + dx, py + dy))
        
        return points

    def _deform_point(self, px, py, w, h):
        """Push point outward along its normal based on cursor proximity."""
        dx = self.cursor_x - px
        dy = self.cursor_y - py
        dist = math.sqrt(dx * dx + dy * dy)
        
        if dist > self.deform_influence or dist < 0.001:
            return 0.0, 0.0
        
        ds = self.deform_strength or self.deform_padding * self.deform_weight
        # Falloff curve: closer = stronger bulge
        strength = (1.0 - dist / self.deform_influence) ** self.deform_falloff * ds
        
        # Calculate outward normal from center of pill
        cx = w / 2.0
        cy = h / 2.0
        nx = px - cx
        ny = py - cy
        n_dist = math.sqrt(nx * nx + ny * ny)
        
        if n_dist < 0.001:
            return 0.0, 0.0
        
        # Normalize and push outward
        return (nx / n_dist) * strength, (ny / n_dist) * strength

    # --- EVENT & INTERACTION HANDLERS ---
    def on_applet_click(self, applet):
        if self.is_expanded:
            self.is_expanded = False
            self.webv.set_visible(False)
            self.target_alpha = 0.0
            return

        self.update_html(applet.expanded)
        self.webv.set_visible(True)
        self.is_expanded = True
        self.arrows_reveal()

        # Не меняем размер окна браузера покачто
        self.target_alpha = 1.0

    def arrows_reveal(self):
        arrows_reveal = (not self.is_expanded and self.cursor_active)
        self.left_revealer.set_reveal_child(arrows_reveal)
        self.right_revealer.set_reveal_child(arrows_reveal)

    def on_pill_enter(self, controller, x, y):
        self.cursor_active = True
        self.cursor_x = x
        self.cursor_y = y

    def on_pill_leave(self, controller):
        self.cursor_active = False
        self.left_revealer.set_reveal_child(False)
        self.right_revealer.set_reveal_child(False)

    def on_pill_motion(self, controller, x, y):
        self.cursor_x = x
        self.cursor_y = y
        self.grab_focus()

    def on_key_pressed(self, controller, keyval, keycode, state):
        if keyval == Gdk.KEY_space:
            print("Space pressed")
            return True
        print("key_pressed")
        return False  

    def on_key_released(self, controller, keyval, keycode, state):
        return False

    # --- ANIMATION ENGINE LOOP & INPUT MASKING ---
    def on_animation_frame(self, widget, clock):
        alpha_easing = 0.23  # Blazing fast fade-out
        size_easing = 0.18   # Smooth, clean window sizing motion

        _, base_w, _, _ = self.main_container.measure(Gtk.Orientation.HORIZONTAL, -1)
        _, base_h, _, _ = self.main_container.measure(Gtk.Orientation.VERTICAL, -1)
        target_a = self.target_alpha
        
        if self.is_expanded:
            target_w = self.expanded_w
            target_h = base_h + self.expanded_h
        else:
            target_w = base_w
            target_h = base_h

        self.current_w += (target_w - self.current_w) * size_easing
        self.current_h += (target_h - self.current_h) * size_easing
        if self.is_expanded and self.expanded_w - self.current_w >= 5: target_a = 0.0
        self.current_alpha += (target_a - self.current_alpha) * alpha_easing

        if abs(self.current_h - self.collapsed_h) <= self.deform_padding * 2 + self.input_extra: 
            self.arrows_reveal()

        if abs(self.current_alpha - self.target_alpha) < 0.02:
            self.current_alpha = self.target_alpha
            if self.current_alpha <= 0.01 and not self.is_expanded:
                self.webv.set_visible(False)
                

        # Animate cursor away when not hovering — spring-back effect
        if not self.cursor_active:
            target_x = -1000.0
            target_y = -1000.0
            spring = 0.15  # Speed of return to rest shape
            self.cursor_x += (target_x - self.cursor_x) * spring
            self.cursor_y += (target_y - self.cursor_y) * spring

        self.canvas.set_size_request(
            int(self.current_w), 
            int(self.current_h)
        )
        self.canvas.queue_draw()
        self.details.set_opacity(self.current_alpha)

        self._update_wayland_input_region()
        return GLib.SOURCE_CONTINUE

    def _update_wayland_input_region(self):
        root_window = self.get_root()
        if not root_window:
            return
            
        surface = root_window.get_surface()
        if not surface:
            return

        window_width = root_window.get_width()
        

        border = self.input_extra # additional free space
        island_x = int(window_width - int(self.current_w + self.deform_padding * 2) - self.deform_padding  - border)
        island_y = 0
        
        rect = cairo.RectangleInt(island_x, island_y, int(self.current_w) + int(self.deform_padding) * 4 + border, int(self.current_h) + int(self.deform_padding) * 4 + border)
        region = cairo.Region(rect)
        surface.set_input_region(region)
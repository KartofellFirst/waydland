import json
import math
import gi
import cairo

gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib
from applet_engine import GenericApplet

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
        
        # Initialize internal structural variables
        self._init_state()
        self._build_architecture()
        self._build_foreground_ui()
        self._setup_controllers()
        
        # Start animation frame callback
        self.add_tick_callback(self.on_animation_frame)

    def _init_state(self):
        """Isolates application state and animation variables."""
        self.is_expanded = False
        self.active_expanded_widget = None
        
        # Animation dimension trackers
        self.current_w = 0.0  
        self.current_h = 0.0
        self.current_alpha = 0.0
        
        self.expanded_w = 0.0
        self.expanded_h = 0.0
        self.target_alpha = 0.0

        self.resting_compact_w = 120.0  # Will be measured once
        self._compact_measured = False

        # Cursor tracking for liquid deformation
        self.cursor_x = -1000.0  # Start far offscreen
        self.cursor_y = -1000.0
        self.cursor_active = False
        
        # Deformation tuning 
        self.deform_influence = 80.0 # how far cursor affects edge (pixels)
        self.deform_strength = 10.0 # maximum bulge amount (pixels)
        self.deform_falloff = 3.0 # 1=linear, 2=quadratic, 3=cubic falloff
        self.deform_padding = 10.0 # padding def = 10
        self.deform_samples = None # cant be visible below 3. Chose the samples amount or put None to turn on quality
        self.deform_quality = 1.0 # if samples not set, quality autocalculates them
        

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
        self.foreground.set_halign(Gtk.Align.END) 
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
        margin = int((self.deform_padding * 1.5 + 5) + max(0, (self.deform_padding - 10) / 4))
        self._apply_margins(self.main_container, end=margin, top=margin) #
        self.main_container.set_size_request(-1, -1) # 30
        
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

        # Bottom Expanded Panel Details Container
        self.details = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.details.set_opacity(0.0) 
        self._apply_margins(self.details, top=10, bottom=10, start=0, end=margin)

        # Final layout alignment
        self.foreground.append(self.main_container)
        self.foreground.append(self.details)

        # if not self._compact_measured: why would we need to keep the width static?
        #     _, self.resting_compact_w, _, _ = self.main_container.measure(Gtk.Orientation.HORIZONTAL, -1)
        #     self.resting_compact_w += 10
        #     self._compact_measured = True``

    def _setup_controllers(self):
        """Attaches gestures, event controllers, and actions."""
        # Global Window Hover Events
        self.motion_ctrl = Gtk.EventControllerMotion()
        self.motion_ctrl.connect("enter", self.on_hover_enter)
        self.motion_ctrl.connect("leave", self.on_hover_leave)
        self.motion_ctrl.set_propagation_phase(Gtk.PropagationPhase.CAPTURE)
        self.add_controller(self.motion_ctrl)

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
        
        # Translate to center the actual pill within the padded canvas
        cr.translate(self.deform_padding, self.deform_padding)
        
        cr.set_source_rgba(0.05, 0.05, 0.05, 1) # why was it transparent, bro?
        
        w = float(width) - self.deform_padding * 2
        h = float(height) - self.deform_padding * 2
        radius = min(min(w, h) / 2.0, 16.0)
        
        points = self._generate_pill_perimeter(w, h, radius)
        
        if points:
            cr.move_to(points[0][0], points[0][1])
            for i in range(1, len(points)):
                cr.line_to(points[i][0], points[i][1])
            cr.close_path()
        
        cr.fill()
        cr.restore()

    def _generate_pill_perimeter(self, w, h, r):
        """Sample the rounded rectangle perimeter and apply cursor deformation."""
        points = []
        samples = int(self.deform_samples if self.deform_samples != None else (self.current_w + self.current_h * self.deform_quality * 2))

        for i in range(samples):
            t = i / samples  # 0.0 to 1.0 around perimeter
            
            # Get base point on rounded rectangle
            px, py = self._sample_rounded_rect(t, w, h, r)
            
            # Apply deformation toward cursor
            dx, dy = self._deform_point(px, py, w, h)
            
            points.append((px + dx, py + dy))
        
        return points

    def _sample_rounded_rect(self, t, w, h, r):
        """Sample a point on the rounded rectangle perimeter at parameter t (0-1)."""
        # Calculate perimeter lengths
        straight_top = w - 2 * r
        straight_bottom = w - 2 * r
        straight_left = h - 2 * r
        straight_right = h - 2 * r
        
        corner_arc = (math.pi / 2) * r  # Length of one corner arc (quarter circle)
        
        total_perimeter = straight_top + straight_bottom + straight_left + straight_right + 4 * corner_arc
        
        # Distance along perimeter at parameter t
        dist = t * total_perimeter
        
        # Top-left corner arc
        if dist < corner_arc:
            angle = math.pi + (dist / corner_arc) * (math.pi / 2)
            return r + r * math.cos(angle), r + r * math.sin(angle)
        dist -= corner_arc
        
        # Top straight edge
        if dist < straight_top:
            return r + dist, 0
        dist -= straight_top
        
        # Top-right corner arc
        if dist < corner_arc:
            angle = -math.pi / 2 + (dist / corner_arc) * (math.pi / 2)
            return w - r + r * math.cos(angle), r + r * math.sin(angle)
        dist -= corner_arc
        
        # Right straight edge
        if dist < straight_right:
            return w, r + dist
        dist -= straight_right
        
        # Bottom-right corner arc
        if dist < corner_arc:
            angle = 0 + (dist / corner_arc) * (math.pi / 2)
            return w - r + r * math.cos(angle), h - r + r * math.sin(angle)
        dist -= corner_arc
        
        # Bottom straight edge
        if dist < straight_bottom:
            return w - r - dist, h
        dist -= straight_bottom
        
        # Bottom-left corner arc
        if dist < corner_arc:
            angle = math.pi / 2 + (dist / corner_arc) * (math.pi / 2)
            return r + r * math.cos(angle), h - r + r * math.sin(angle)
        dist -= corner_arc
        
        # Left straight edge
        return 0, h - r - dist

    def _deform_point(self, px, py, w, h):
        """Push point outward along its normal based on cursor proximity."""
        dx = self.cursor_x - px
        dy = self.cursor_y - py
        dist = math.sqrt(dx * dx + dy * dy)
        
        if dist > self.deform_influence or dist < 0.001:
            return 0.0, 0.0
        
        # Falloff curve: closer = stronger bulge
        strength = (1.0 - dist / self.deform_influence) ** self.deform_falloff * self.deform_strength
        
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
        if self.is_expanded and self.active_expanded_widget == applet.expanded_widget:
            self.is_expanded = False
            self.active_expanded_widget = None
            self.target_alpha = 0.0
            return

        self._clear_details_container()
        self.details.append(applet.expanded_widget)
        self.active_expanded_widget = applet.expanded_widget
        self.is_expanded = True

        _, base_w, _, _ = self.main_container.measure(Gtk.Orientation.HORIZONTAL, -1)
        
        _, det_w, _, _ = self.details.measure(Gtk.Orientation.HORIZONTAL, -1)
        _, det_h, _, _ = self.details.measure(Gtk.Orientation.VERTICAL, -1)
        
        self.expanded_w = max(base_w, det_w)
        self.expanded_h = det_h
        self.target_alpha = 1.0

    def on_hover_enter(self, controller, x, y):
        self.left_revealer.set_reveal_child(True)
        self.right_revealer.set_reveal_child(True)

    def on_hover_leave(self, controller):
        self.left_revealer.set_reveal_child(False)
        self.right_revealer.set_reveal_child(False)

    def on_pill_enter(self, controller, x, y):
        self.cursor_active = True
        self.cursor_x = x
        self.cursor_y = y

    def on_pill_leave(self, controller):
        self.cursor_active = False

    def on_pill_motion(self, controller, x, y):
        self.cursor_x = x
        self.cursor_y = y

    # --- ANIMATION ENGINE LOOP & INPUT MASKING ---
    def on_animation_frame(self, widget, clock):
        alpha_easing = 0.23  # Blazing fast fade-out
        size_easing = 0.18   # Smooth, clean window sizing motion

        _, base_w, _, _ = self.main_container.measure(Gtk.Orientation.HORIZONTAL, -1)
        # base_w = self.resting_compact_w # Never shrinks
        _, base_h, _, _ = self.main_container.measure(Gtk.Orientation.VERTICAL, -1)
        
        base_h = max(30, base_h)

        # --- THE FIX: Maintain width until opacity finishes fading out ---
        if self.is_expanded or self.current_alpha > 0.01:
            target_w = max(base_w, self.expanded_w)
            target_h = base_h + self.expanded_h
        else:
            target_w = base_w
            target_h = base_h

        self.current_w += (target_w - self.current_w) * size_easing
        self.current_h += (target_h - self.current_h) * size_easing
        self.current_alpha += (self.target_alpha - self.current_alpha) * alpha_easing

        if abs(self.current_alpha - self.target_alpha) < 0.02:
            self.current_alpha = self.target_alpha
            if self.current_alpha <= 0.01 and not self.is_expanded:
                self._clear_details_container()
                # Snap values back to resting state once invisible
                self.expanded_w = 0.0
                self.expanded_h = 0.0

        # Animate cursor away when not hovering — spring-back effect
        if not self.cursor_active:
            target_x = -1000.0
            target_y = -1000.0
            spring = 0.15  # Speed of return to rest shape
            self.cursor_x += (target_x - self.cursor_x) * spring
            self.cursor_y += (target_y - self.cursor_y) * spring

        self.canvas.set_size_request(
            int(self.current_w + self.deform_padding * 2), 
            int(self.current_h + self.deform_padding * 2)
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
        
        # The pill itself is offset by deform_padding within the canvas
        island_x = int(window_width - int(self.current_w + self.deform_padding * 2) + int(self.deform_padding) - self.deform_padding * 2 - 10)
        island_y = int(max(0, int(self.deform_padding) - self.deform_padding * 2 - 10))
        
        # rect = cairo.RectangleInt(island_x, island_y, int(self.current_w), int(self.current_h))
        rect = cairo.RectangleInt(island_x, island_y, int(self.current_w) + int(self.deform_padding) * 2 + 10, int(self.current_h) + int(self.deform_padding) * 2 + 10)
        region = cairo.Region(rect)
        surface.set_input_region(region)
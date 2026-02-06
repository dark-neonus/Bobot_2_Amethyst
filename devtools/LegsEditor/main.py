"""
Main Application Entry Point
Spider Robot Gait Editor
"""

import sys
import os
import subprocess
import atexit
from pathlib import Path

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from imgui_bundle import imgui, immapp, hello_imgui
import time

from config_loader import RobotConfig
from state_manager import AppState
from core.kinematics import RobotKinematics
from core.playback import PlaybackEngine
from gui.viewport_gl import Viewport
from gui.properties import PropertiesPanel
from gui.timeline import TimelinePanel


# Global variable to track Xvfb process
_xvfb_process = None


def check_display_available(display=None):
    """Check if an X display is available and accessible"""
    if display is None:
        display = os.environ.get('DISPLAY', '')
    
    if not display:
        return False
    
    try:
        # Try to run xdpyinfo to check if display is accessible
        result = subprocess.run(
            ['xdpyinfo', '-display', display],
            capture_output=True,
            timeout=2
        )
        return result.returncode == 0
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return False


def find_available_display():
    """Find an available display number"""
    for display_num in range(99, 0, -1):
        display = f':{display_num}'
        # Check if display is not in use
        if not check_display_available(display):
            return display
    return ':99'  # Fallback


def start_xvfb():
    """Start Xvfb (X Virtual Framebuffer) for headless rendering"""
    global _xvfb_process
    
    # Check if current DISPLAY is accessible
    current_display = os.environ.get('DISPLAY', '')
    if current_display and check_display_available(current_display):
        print(f"✓ Using existing X display: {current_display}")
        return True
    
    # Find an available display
    display = find_available_display()
    
    try:
        print(f"Starting Xvfb on display {display}...")
        _xvfb_process = subprocess.Popen(
            ['Xvfb', display, '-screen', '0', '1920x1080x24'],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )
        
        # Set the DISPLAY environment variable
        os.environ['DISPLAY'] = display
        
        # Wait a moment for Xvfb to start
        import time
        time.sleep(1)
        
        # Verify it's working
        if check_display_available(display):
            print(f"✓ Xvfb started successfully on display {display}")
            
            # Register cleanup
            atexit.register(stop_xvfb)
            return True
        else:
            print(f"✗ Failed to verify Xvfb on display {display}")
            stop_xvfb()
            return False
            
    except FileNotFoundError:
        print("✗ Error: Xvfb not found. Please install: apt-get install xvfb")
        return False
    except Exception as e:
        print(f"✗ Error starting Xvfb: {e}")
        return False


def stop_xvfb():
    """Stop the Xvfb process"""
    global _xvfb_process
    if _xvfb_process:
        try:
            _xvfb_process.terminate()
            _xvfb_process.wait(timeout=5)
            print("✓ Xvfb stopped")
        except Exception as e:
            print(f"Warning: Error stopping Xvfb: {e}")
            try:
                _xvfb_process.kill()
            except:
                pass
        finally:
            _xvfb_process = None


class GaitEditorApp:
    """Main application class"""
    
    def __init__(self):
        # Load configuration
        self.config = RobotConfig()
        
        # Initialize state
        self.state = AppState()
        self.state.initialize_default_pose(self.config.get_all_servo_names())
        
        # Initialize kinematics
        self.kinematics = RobotKinematics(self.config)
        
        # Initialize playback engine
        self.playback = PlaybackEngine(self.state)
        
        # GUI components (initialized after OpenGL context)
        self.viewport = None
        self.properties_panel = None
        self.timeline_panel = None
        
        # Timing
        self.last_time = time.time()
        
        # File dialog state
        self.show_save_dialog = False
        self.show_load_dialog = False
    
    def setup(self):
        """Setup called once after OpenGL context creation"""
        try:
            # Initialize viewport (uses raw OpenGL, no ModernGL)
            self.viewport = Viewport()
            
            print(f"Viewport initialized: {self.viewport.width}x{self.viewport.height}")
            print(f"Camera initial position: dist={self.viewport.camera.distance}, az={self.viewport.camera.azimuth}, el={self.viewport.camera.elevation}")
        except Exception as e:
            print(f"Error initializing viewport: {e}")
            import traceback
            traceback.print_exc()
        
        # Initialize GUI panels
        self.properties_panel = PropertiesPanel(self.config, self.state)
        self.timeline_panel = TimelinePanel(self.config, self.state, self.playback)
        
        # Add an initial keyframe at time 0
        self.state.add_keyframe(0.0)
        
        # Print initial state for debugging
        print(f"Initial servos: {len(self.state.current_pose)} servos")
        print(f"Keyframes: {len(self.state.keyframes)}")
    
    def render_menu_bar(self):
        """Render the main menu bar (called by Hello ImGui)"""
        if imgui.begin_menu("File"):
            if imgui.menu_item("New", "Ctrl+N")[0]:
                self.new_animation()
                
                if imgui.menu_item("Open", "Ctrl+O")[0]:
                    self.open_animation()
                
                imgui.separator()
                
                if imgui.menu_item("Save", "Ctrl+S", False, self.state.current_file is not None)[0]:
                    self.save_animation()
                
                if imgui.menu_item("Save As...", "Ctrl+Shift+S")[0]:
                    self.save_animation_as()
                
                imgui.separator()
                
                if imgui.menu_item("Exit", "Alt+F4")[0]:
                    hello_imgui.get_runner_params().app_shall_exit = True
                
                imgui.end_menu()
            
            if imgui.begin_menu("View"):
                if imgui.menu_item("Reset Camera")[0]:
                    self.viewport.camera = self.viewport.camera.__class__()
                
                imgui.separator()
                
                if imgui.menu_item("Front View")[0]:
                    self.viewport.camera.snap_to_view("front")
                
                if imgui.menu_item("Top View")[0]:
                    self.viewport.camera.snap_to_view("top")
                
                if imgui.menu_item("Right View")[0]:
                    self.viewport.camera.snap_to_view("right")
                
                imgui.end_menu()
            
            if imgui.begin_menu("Help"):
                if imgui.menu_item("About")[0]:
                    pass  # Could show about dialog
                
                imgui.end_menu()
    
    def render_viewport_window(self):
        """Render the 3D viewport content"""
        avail = imgui.get_content_region_avail()
        
        if avail.x > 0 and avail.y > 0:
            # Get window position for rendering
            win_pos = imgui.get_window_pos()
            cursor_pos = imgui.get_cursor_screen_pos()
            
            # Reserve space in ImGui
            imgui.dummy(imgui.ImVec2(avail.x, avail.y))
            
            # Render text as placeholder for now
            draw_list = imgui.get_window_draw_list()
            draw_list.add_rect_filled(
                imgui.ImVec2(cursor_pos.x, cursor_pos.y),
                imgui.ImVec2(cursor_pos.x + avail.x, cursor_pos.y + avail.y),
                imgui.color_convert_float4_to_u32(imgui.ImVec4(0.2, 0.2, 0.25, 1.0))
            )
            
            # Draw some test graphics
            if self.viewport:
                leg_positions = self.kinematics.calculate_all_legs(self.state.current_pose)
                body_corners = self.kinematics.get_body_corners()
                
                # Draw debug info as text for now
                text_x = cursor_pos.x + 10
                text_y = cursor_pos.y + 10
                draw_list.add_text(
                    imgui.ImVec2(text_x, text_y),
                    imgui.color_convert_float4_to_u32(imgui.ImVec4(1, 1, 1, 1)),
                    f"3D Viewport - PyOpenGL context issue"
                )
                draw_list.add_text(
                    imgui.ImVec2(text_x, text_y + 20),
                    imgui.color_convert_float4_to_u32(imgui.ImVec4(1, 1, 0, 1)),
                    f"Legs: {len(leg_positions)}, Body corners: {len(body_corners)}"
                )
                draw_list.add_text(
                    imgui.ImVec2(text_x, text_y + 40),
                    imgui.color_convert_float4_to_u32(imgui.ImVec4(0, 1, 1, 1)),
                    f"Camera: dist={self.viewport.camera.distance:.0f} az={self.viewport.camera.azimuth:.0f}"
                )
                
                # Draw a simple 2D representation of the robot
                center_x = cursor_pos.x + avail.x / 2
                center_y = cursor_pos.y + avail.y / 2
                scale = 0.5
                
                # Draw body as rectangle (simplified top-down view)
                body_w = 120 * scale
                body_h = 120 * scale
                draw_list.add_rect(
                    imgui.ImVec2(center_x - body_w/2, center_y - body_h/2),
                    imgui.ImVec2(center_x + body_w/2, center_y + body_h/2),
                    imgui.color_convert_float4_to_u32(imgui.ImVec4(0, 1, 1, 1)),
                    0, 0, 2.0
                )
                
                # Draw legs as lines from body corners
                for leg_id, positions in leg_positions.items():
                    if positions and len(positions) >= 2:
                        start = positions[0]
                        end = positions[-1]
                        
                        # Project 3D to 2D (simple top-down)
                        start_x = center_x + start[0] * scale
                        start_y = center_y + start[2] * scale
                        end_x = center_x + end[0] * scale
                        end_y = center_y + end[2] * scale
                        
                        # Draw leg
                        draw_list.add_line(
                            imgui.ImVec2(start_x, start_y),
                            imgui.ImVec2(end_x, end_y),
                            imgui.color_convert_float4_to_u32(imgui.ImVec4(1, 1, 0, 1)),
                            2.0
                        )
                        # Draw foot
                        draw_list.add_circle_filled(
                            imgui.ImVec2(end_x, end_y),
                            4.0,
                            imgui.color_convert_float4_to_u32(imgui.ImVec4(1, 0, 0, 1))
                        )
        else:
            imgui.text("Viewport area too small")
    
    def handle_viewport_input(self):
        """Handle mouse input for viewport camera control"""
        io = imgui.get_io()
        
        # Orbit: Shift + Middle Mouse
        if io.key_shift and imgui.is_mouse_dragging(imgui.MouseButton_.middle):
            delta = imgui.get_mouse_drag_delta(imgui.MouseButton_.middle)
            imgui.reset_mouse_drag_delta(imgui.MouseButton_.middle)
            self.viewport.camera.orbit(delta.x * 0.5, -delta.y * 0.5)
        
        # Pan: Middle Mouse
        elif imgui.is_mouse_dragging(imgui.MouseButton_.middle):
            delta = imgui.get_mouse_drag_delta(imgui.MouseButton_.middle)
            imgui.reset_mouse_drag_delta(imgui.MouseButton_.middle)
            self.viewport.camera.pan(-delta.x, delta.y)
        
        # Zoom: Mouse Wheel
        if io.mouse_wheel != 0:
            self.viewport.camera.zoom(io.mouse_wheel)
    
    def gui_loop(self):
        """Main GUI rendering loop"""
        # Calculate delta time
        current_time = time.time()
        delta_time = current_time - self.last_time
        self.last_time = current_time
        
        # Update playback
        self.playback.update(delta_time)
        
        # Render main windows with begin/end
        # 3D Viewport goes to MainDockSpace (which becomes the center/top-left after splits)
        imgui.begin("3D Viewport")
        self.render_viewport_window()
        imgui.end()
        
        # Properties goes to RightSpace
        imgui.begin("Properties")
        if self.properties_panel:
            self.properties_panel.render()
        imgui.end()
        
        # Timeline goes to BottomSpace
        imgui.begin("Timeline")
        if self.timeline_panel:
            self.timeline_panel.render()
        imgui.end()
        
        # Handle keyboard shortcuts
        self.handle_keyboard_shortcuts()
    
    def handle_keyboard_shortcuts(self):
        """Handle keyboard shortcuts"""
        io = imgui.get_io()
        
        # Ctrl+N: New
        if io.key_ctrl and imgui.is_key_pressed(imgui.Key.n):
            self.new_animation()
        
        # Ctrl+S: Save
        if io.key_ctrl and not io.key_shift and imgui.is_key_pressed(imgui.Key.s):
            if self.state.current_file:
                self.save_animation()
        
        # Ctrl+Shift+S: Save As
        if io.key_ctrl and io.key_shift and imgui.is_key_pressed(imgui.Key.s):
            self.save_animation_as()
        
        # Space: Play/Pause
        if imgui.is_key_pressed(imgui.Key.space):
            self.playback.toggle_play_pause()
    
    def new_animation(self):
        """Create a new animation"""
        if self.state.is_modified:
            # TODO: Show confirmation dialog
            pass
        
        self.state.new_animation()
        self.state.initialize_default_pose(self.config.get_all_servo_names())
        self.state.add_keyframe(0.0)
    
    def open_animation(self):
        """Open an animation file"""
        # Simple file dialog using imgui
        # For production, use a proper file dialog
        import tkinter as tk
        from tkinter import filedialog
        
        root = tk.Tk()
        root.withdraw()
        
        filepath = filedialog.askopenfilename(
            title="Open Animation",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")]
        )
        
        if filepath:
            try:
                self.state.load_from_file(filepath)
                print(f"Loaded animation from {filepath}")
            except Exception as e:
                print(f"Error loading file: {e}")
    
    def save_animation(self):
        """Save animation to current file"""
        if self.state.current_file:
            try:
                self.state.save_to_file(self.state.current_file)
                print(f"Saved animation to {self.state.current_file}")
            except Exception as e:
                print(f"Error saving file: {e}")
        else:
            self.save_animation_as()
    
    def save_animation_as(self):
        """Save animation to a new file"""
        import tkinter as tk
        from tkinter import filedialog
        
        root = tk.Tk()
        root.withdraw()
        
        filepath = filedialog.asksaveasfilename(
            title="Save Animation As",
            defaultextension=".json",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")]
        )
        
        if filepath:
            try:
                self.state.save_to_file(filepath)
                print(f"Saved animation to {filepath}")
            except Exception as e:
                print(f"Error saving file: {e}")
    
    def cleanup(self):
        """Cleanup resources"""
        if self.viewport:
            self.viewport.cleanup()


def create_default_docking_splits():
    """Create the default docking layout"""
    splits = []
    
    # First split: Separate right side for Properties (20% width)
    # MainDockSpace -> RightSpace (right 20%) + LeftSpace (left 80%)
    split_right = hello_imgui.DockingSplit()
    split_right.initial_dock = "MainDockSpace"
    split_right.new_dock = "RightSpace"
    split_right.direction = imgui.Dir_.right
    split_right.ratio = 0.2
    splits.append(split_right)
    
    # Second split: Separate bottom for Timeline (30% height of left area)
    # MainDockSpace (which is now the left 80%) -> BottomSpace (bottom 30%) + CenterSpace (top 70%)
    split_bottom = hello_imgui.DockingSplit()
    split_bottom.initial_dock = "MainDockSpace"
    split_bottom.new_dock = "BottomSpace"
    split_bottom.direction = imgui.Dir_.down
    split_bottom.ratio = 0.3
    splits.append(split_bottom)
    
    return splits


def main():
    """Application entry point"""
    # Ensure X display is available
    if not start_xvfb():
        print("ERROR: Failed to initialize X display")
        print("Please ensure Xvfb is installed or X11 forwarding is configured")
        sys.exit(1)
    
    app = GaitEditorApp()
    
    # Configure Hello ImGui
    runner_params = hello_imgui.RunnerParams()
    runner_params.app_window_params.window_title = "Bobot Legs Editor - Spider Robot Gait Editor"
    runner_params.app_window_params.window_geometry.size = (1600, 900)
    
    # Enable docking with layout
    runner_params.imgui_window_params.default_imgui_window_type = (
        hello_imgui.DefaultImGuiWindowType.provide_full_screen_dock_space
    )
    runner_params.imgui_window_params.enable_viewports = False
    
    # Setup docking layout  
    docking_params = hello_imgui.DockingParams()
    docking_params.docking_splits = create_default_docking_splits()
    docking_params.layout_condition = hello_imgui.DockingLayoutCondition.application_start
    
    # Create dockable windows list to tell Hello ImGui which windows go where
    dockable_windows = []
    
    # 3D Viewport -> MainDockSpace (center/top area)
    viewport_win = hello_imgui.DockableWindow()
    viewport_win.label = "3D Viewport"
    viewport_win.dock_space_name = "MainDockSpace"
    viewport_win.gui_function = lambda: None  # We render it manually in gui_loop
    dockable_windows.append(viewport_win)
    
    # Properties -> RightSpace
    properties_win = hello_imgui.DockableWindow()
    properties_win.label = "Properties"
    properties_win.dock_space_name = "RightSpace"
    properties_win.gui_function = lambda: None
    dockable_windows.append(properties_win)
    
    # Timeline -> BottomSpace
    timeline_win = hello_imgui.DockableWindow()
    timeline_win.label = "Timeline"
    timeline_win.dock_space_name = "BottomSpace"
    timeline_win.gui_function = lambda: None
    dockable_windows.append(timeline_win)
    
    docking_params.dockable_windows = dockable_windows
    runner_params.docking_params = docking_params
    
    # Set callbacks
    runner_params.callbacks.show_gui = app.gui_loop
    runner_params.callbacks.post_init = app.setup
    runner_params.callbacks.before_exit = app.cleanup
    runner_params.callbacks.show_menus = app.render_menu_bar
    
    # Enable menu bar
    runner_params.imgui_window_params.show_menu_bar = True
    
    # Run the application
    immapp.run(runner_params)


if __name__ == "__main__":
    main()

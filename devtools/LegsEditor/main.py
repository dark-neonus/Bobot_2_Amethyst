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
        
        # Selection state
        self.selected_joint = None  # Format: (leg_id, joint_index) or None
        
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
            cursor_pos = imgui.get_cursor_screen_pos()
            
            # Reserve space in ImGui and make it interactive
            imgui.invisible_button("viewport_area", imgui.ImVec2(avail.x, avail.y))
            is_hovered = imgui.is_item_hovered()
            is_clicked = imgui.is_item_clicked(imgui.MouseButton_.left)
            
            # Draw background
            draw_list = imgui.get_window_draw_list()
            draw_list.add_rect_filled(
                imgui.ImVec2(cursor_pos.x, cursor_pos.y),
                imgui.ImVec2(cursor_pos.x + avail.x, cursor_pos.y + avail.y),
                imgui.color_convert_float4_to_u32(imgui.ImVec4(0.15, 0.15, 0.18, 1.0))
            )
            
            if self.viewport:
                leg_positions = self.kinematics.calculate_all_legs(self.state.current_pose)
                body_corners = self.kinematics.get_body_corners()
                
                # Handle joint selection
                if is_clicked:
                    mouse_pos = imgui.get_mouse_pos()
                    self._handle_joint_selection(mouse_pos, cursor_pos, avail, leg_positions)
                
                # Render 3D scene with perspective projection
                self._render_3d_scene(draw_list, cursor_pos, avail, leg_positions, body_corners)
                
                # Draw orientation axes in top-right corner
                self._draw_orientation_cube(draw_list, cursor_pos, avail)
                
            # Handle camera controls
            if is_hovered:
                self.handle_viewport_input()
        else:
            imgui.text("Viewport area too small")
    
    def _project_3d_to_2d(self, point, camera, screen_center, screen_width, screen_height):
        """Project a 3D point to 2D screen coordinates with perspective"""
        import math
        
        # Camera parameters
        distance = camera.distance
        azimuth = math.radians(camera.azimuth)
        elevation = math.radians(camera.elevation)
        
        # Calculate camera position
        cam_x = distance * math.cos(elevation) * math.cos(azimuth)
        cam_y = distance * math.sin(elevation)
        cam_z = distance * math.cos(elevation) * math.sin(azimuth)
        
        # Transform point to camera space
        dx = point[0] - 0
        dy = point[1] - 0
        dz = point[2] - 0
        
        # Rotate around Y axis (azimuth)
        cos_az = math.cos(-azimuth)
        sin_az = math.sin(-azimuth)
        tx = dx * cos_az - dz * sin_az
        tz = dx * sin_az + dz * cos_az
        ty = dy
        
        # Rotate around X axis (elevation)
        cos_el = math.cos(-elevation)
        sin_el = math.sin(-elevation)
        ty2 = ty * cos_el - tz * sin_el
        tz2 = ty * sin_el + tz * cos_el
        tx2 = tx
        
        # Translate by camera distance
        tz2 += distance
        
        # Perspective projection
        if tz2 > 0.1:  # Avoid division by zero
            fov = 45.0  # Field of view in degrees
            scale = (screen_height / 2.0) / math.tan(math.radians(fov / 2.0))
            
            screen_x = screen_center[0] + (tx2 * scale / tz2)
            screen_y = screen_center[1] - (ty2 * scale / tz2)
            return (screen_x, screen_y, tz2)  # Return depth for visibility testing
        
        return None
    
    def _render_3d_scene(self, draw_list, cursor_pos, avail, leg_positions, body_corners):
        """Render 3D scene using 2D primitives with perspective projection"""
        screen_center = (cursor_pos.x + avail.x / 2, cursor_pos.y + avail.y / 2)
        
        # Draw grid
        self._draw_3d_grid(draw_list, screen_center, avail.x, avail.y)
        
        # Draw body
        if body_corners:
            self._draw_3d_body(draw_list, body_corners, screen_center, avail.x, avail.y)
        
        # Draw legs
        if leg_positions:
            self._draw_3d_legs(draw_list, leg_positions, screen_center, avail.x, avail.y)
    
    def _draw_3d_grid(self, draw_list, screen_center, width, height):
        """Draw 3D ground grid"""
        grid_size = 500
        grid_step = 50
        
        lines = []
        for i in range(-grid_size, grid_size + grid_step, grid_step):
            # Lines parallel to X axis
            lines.append((i, 0, -grid_size, i, 0, grid_size))
            # Lines parallel to Z axis
            lines.append((-grid_size, 0, i, grid_size, 0, i))
        
        for x1, y1, z1, x2, y2, z2 in lines:
            p1 = self._project_3d_to_2d((x1, y1, z1), self.viewport.camera, screen_center, width, height)
            p2 = self._project_3d_to_2d((x2, y2, z2), self.viewport.camera, screen_center, width, height)
            
            if p1 and p2:
                draw_list.add_line(
                    imgui.ImVec2(p1[0], p1[1]),
                    imgui.ImVec2(p2[0], p2[1]),
                    imgui.color_convert_float4_to_u32(imgui.ImVec4(0.3, 0.3, 0.35, 1.0)),
                    1.0
                )
    
    def _draw_3d_body(self, draw_list, corners, screen_center, width, height):
        """Draw 3D robot body"""
        # Define body edges (cube)
        edges = [
            (0, 1), (1, 2), (2, 3), (3, 0),  # Bottom face
            (4, 5), (5, 6), (6, 7), (7, 4),  # Top face
            (0, 4), (1, 5), (2, 6), (3, 7),  # Vertical edges
        ]
        
        for i, j in edges:
            p1 = self._project_3d_to_2d(corners[i], self.viewport.camera, screen_center, width, height)
            p2 = self._project_3d_to_2d(corners[j], self.viewport.camera, screen_center, width, height)
            
            if p1 and p2:
                draw_list.add_line(
                    imgui.ImVec2(p1[0], p1[1]),
                    imgui.ImVec2(p2[0], p2[1]),
                    imgui.color_convert_float4_to_u32(imgui.ImVec4(0.0, 1.0, 1.0, 1.0)),
                    2.0
                )
    
    def _draw_3d_legs(self, draw_list, leg_positions, screen_center, width, height):
        """Draw 3D robot legs as solid rectangular tubes"""
        import math
        import numpy as np
        
        leg_width = 18.0   # Width of leg in mm (3D space)
        leg_height = 12.0  # Height/thickness of leg in mm (3D space)
        
        for leg_id, positions in leg_positions.items():
            if not positions or len(positions) < 2:
                continue
            
            # Draw leg segments as 3D rectangular tubes
            for i in range(len(positions) - 1):
                p1_3d = np.array(positions[i])
                p2_3d = np.array(positions[i + 1])
                
                # Calculate leg direction vector
                leg_dir = p2_3d - p1_3d
                leg_length = np.linalg.norm(leg_dir)
                
                if leg_length > 0.1:
                    leg_dir = leg_dir / leg_length
                    
                    # Calculate two perpendicular vectors to create rectangular cross-section
                    # Use world up vector (Y-axis) to keep cross-section consistent
                    world_up = np.array([0.0, 1.0, 0.0])
                    
                    # First perpendicular: perpendicular to leg in horizontal plane
                    # This keeps the leg "width" horizontal
                    perp_width = np.cross(world_up, leg_dir)
                    perp_width_len = np.linalg.norm(perp_width)
                    
                    if perp_width_len > 0.01:
                        perp_width = perp_width / perp_width_len * (leg_width / 2.0)
                    else:
                        # Leg is vertical, use X-axis
                        perp_width = np.array([leg_width / 2.0, 0.0, 0.0])
                    
                    # Second perpendicular: perpendicular to both leg and width
                    # This is the "height" dimension
                    perp_height = np.cross(leg_dir, perp_width)
                    perp_height_len = np.linalg.norm(perp_height)
                    if perp_height_len > 0.01:
                        perp_height = perp_height / perp_height_len * (leg_height / 2.0)
                    else:
                        perp_height = np.array([0.0, leg_height / 2.0, 0.0])
                    
                    # Create 8 corners of rectangular prism in 3D
                    # Bottom face (4 corners)
                    c1_3d = p1_3d + perp_width - perp_height
                    c2_3d = p2_3d + perp_width - perp_height
                    c3_3d = p2_3d - perp_width - perp_height
                    c4_3d = p1_3d - perp_width - perp_height
                    
                    # Top face (4 corners)
                    c5_3d = p1_3d + perp_width + perp_height
                    c6_3d = p2_3d + perp_width + perp_height
                    c7_3d = p2_3d - perp_width + perp_height
                    c8_3d = p1_3d - perp_width + perp_height
                    
                    # Project all 8 corners to 2D
                    corners_3d = [c1_3d, c2_3d, c3_3d, c4_3d, c5_3d, c6_3d, c7_3d, c8_3d]
                    corners_2d = []
                    for c in corners_3d:
                        proj = self._project_3d_to_2d(c, self.viewport.camera, screen_center, width, height)
                        if proj:
                            corners_2d.append(proj)
                        else:
                            corners_2d.append(None)
                    
                    # Check if all corners projected successfully
                    if None not in corners_2d:
                        # Base color gradient from yellow to red
                        t = i / (len(positions) - 1)
                        base_r, base_g, base_b = 1.0, 1.0 - t * 0.5, 0.0
                        
                        # Check if this segment is selected
                        is_selected = self.selected_joint == (leg_id, i)
                        if is_selected:
                            base_r, base_g, base_b = 0.0, 1.0, 0.0
                        
                        # Get camera direction for depth sorting
                        cam_dir = self.viewport.camera.get_view_direction()
                        
                        # Define all 6 faces with proper winding order (counter-clockwise when viewed from outside)
                        faces = [
                            # Each face: [4 corner indices], shade multiplier
                            ([0, 1, 2, 3], 0.7),   # Bottom face
                            ([4, 5, 6, 7], 1.0),   # Top face
                            ([0, 1, 5, 4], 0.85),  # Right face  
                            ([3, 2, 6, 7], 0.85),  # Left face
                            ([1, 2, 6, 5], 0.9),   # Front face
                            ([0, 4, 7, 3], 0.75),  # Back face
                        ]
                        
                        # Calculate depth for each face and sort
                        face_data = []
                        for face_indices, base_shade in faces:
                            # Calculate face center depth
                            center_3d = sum([corners_3d[idx] for idx in face_indices]) / 4.0
                            depth = np.dot(center_3d, cam_dir)
                            face_data.append((depth, face_indices, base_shade))
                        
                        # Sort by depth (draw far faces first - painter's algorithm)
                        face_data.sort(key=lambda x: x[0])
                        
                        # Draw all faces (no backface culling)
                        for depth, face_indices, base_shade in face_data:
                            r, g, b = base_r * base_shade, base_g * base_shade, base_b * base_shade
                            
                            # Get the 4 corners of this face
                            c = [corners_2d[idx] for idx in face_indices]
                            
                            # Draw as two triangles to form a solid quad
                            fill_color = imgui.color_convert_float4_to_u32(imgui.ImVec4(r, g, b, 1.0))
                            draw_list.add_triangle_filled(
                                imgui.ImVec2(c[0][0], c[0][1]),
                                imgui.ImVec2(c[1][0], c[1][1]),
                                imgui.ImVec2(c[2][0], c[2][1]),
                                fill_color
                            )
                            draw_list.add_triangle_filled(
                                imgui.ImVec2(c[0][0], c[0][1]),
                                imgui.ImVec2(c[2][0], c[2][1]),
                                imgui.ImVec2(c[3][0], c[3][1]),
                                fill_color
                            )
                        
                        # Draw all edges for solid look
                        edge_color = imgui.color_convert_float4_to_u32(imgui.ImVec4(0.1, 0.1, 0.1, 0.8))
                        edges = [
                            (0, 1), (1, 2), (2, 3), (3, 0),  # Bottom face
                            (4, 5), (5, 6), (6, 7), (7, 4),  # Top face
                            (0, 4), (1, 5), (2, 6), (3, 7),  # Vertical edges
                        ]
                        for idx1, idx2 in edges:
                            draw_list.add_line(
                                imgui.ImVec2(corners_2d[idx1][0], corners_2d[idx1][1]),
                                imgui.ImVec2(corners_2d[idx2][0], corners_2d[idx2][1]),
                                edge_color, 1.2
                            )
            
            # Draw joints as circles
            for i, pos_3d in enumerate(positions):
                p_joint = self._project_3d_to_2d(pos_3d, self.viewport.camera, screen_center, width, height)
                if p_joint:
                    is_selected = self.selected_joint == (leg_id, i)
                    radius = 6.0 if is_selected else 4.0
                    
                    if i == 0:
                        # Base joint - cyan
                        color = imgui.ImVec4(0.0, 1.0, 1.0, 1.0)
                    elif i == len(positions) - 1:
                        # Foot - red or green if selected
                        color = imgui.ImVec4(0.0, 1.0, 0.0, 1.0) if is_selected else imgui.ImVec4(1.0, 0.0, 0.0, 1.0)
                    else:
                        # Middle joint - yellow or green if selected
                        color = imgui.ImVec4(0.0, 1.0, 0.0, 1.0) if is_selected else imgui.ImVec4(1.0, 1.0, 0.0, 1.0)
                    
                    draw_list.add_circle_filled(
                        imgui.ImVec2(p_joint[0], p_joint[1]),
                        radius,
                        imgui.color_convert_float4_to_u32(color)
                    )
                    
                    # Draw outline for selected joint
                    if is_selected:
                        draw_list.add_circle(
                            imgui.ImVec2(p_joint[0], p_joint[1]),
                            radius + 2.0,
                            imgui.color_convert_float4_to_u32(imgui.ImVec4(1.0, 1.0, 1.0, 1.0)),
                            0,
                            2.0
                        )
    
    def _draw_orientation_cube(self, draw_list, cursor_pos, avail):
        """Draw orientation axes in top-right corner"""
        import math
        
        # Position in top-right corner
        cube_size = 60
        margin = 15
        cube_center_x = cursor_pos.x + avail.x - cube_size - margin
        cube_center_y = cursor_pos.y + margin + cube_size // 2
        
        # Draw background circle
        draw_list.add_circle_filled(
            imgui.ImVec2(cube_center_x, cube_center_y),
            cube_size // 2 + 5,
            imgui.color_convert_float4_to_u32(imgui.ImVec4(0.1, 0.1, 0.12, 0.8))
        )
        
        # Get camera rotation
        azimuth = math.radians(self.viewport.camera.azimuth)
        elevation = math.radians(self.viewport.camera.elevation)
        
        # Axis vectors in world space
        axes = [
            ((1, 0, 0), (1.0, 0.0, 0.0), "X"),  # Red
            ((0, 1, 0), (0.0, 1.0, 0.0), "Y"),  # Green
            ((0, 0, 1), (0.0, 0.0, 1.0), "Z"),  # Blue
        ]
        
        axis_length = 25
        
        # Sort axes by depth (back to front)
        axes_with_depth = []
        for axis_vec, color, label in axes:
            # Rotate axis vector
            x, y, z = axis_vec
            
            # Rotate around Y (azimuth)
            cos_az = math.cos(-azimuth)
            sin_az = math.sin(-azimuth)
            tx = x * cos_az - z * sin_az
            tz = x * sin_az + z * cos_az
            ty = y
            
            # Rotate around X (elevation)
            cos_el = math.cos(-elevation)
            sin_el = math.sin(-elevation)
            ty2 = ty * cos_el - tz * sin_el
            tz2 = ty * sin_el + tz * cos_el
            
            axes_with_depth.append((tz2, tx, ty2, color, label))
        
        # Sort by depth (draw back axes first)
        axes_with_depth.sort()
        
        # Draw axes
        for depth, tx, ty, color, label in axes_with_depth:
            # Project to 2D
            screen_x = cube_center_x + tx * axis_length
            screen_y = cube_center_y - ty * axis_length
            
            # Make back-facing axes dimmer
            alpha = 1.0 if depth > 0 else 0.3
            
            # Draw axis line
            draw_list.add_line(
                imgui.ImVec2(cube_center_x, cube_center_y),
                imgui.ImVec2(screen_x, screen_y),
                imgui.color_convert_float4_to_u32(imgui.ImVec4(color[0], color[1], color[2], alpha)),
                3.0
            )
            
            # Draw axis label
            label_offset = 8
            text_x = screen_x + (screen_x - cube_center_x) / axis_length * label_offset
            text_y = screen_y + (screen_y - cube_center_y) / axis_length * label_offset
            
            draw_list.add_text(
                imgui.ImVec2(text_x - 5, text_y - 8),
                imgui.color_convert_float4_to_u32(imgui.ImVec4(color[0], color[1], color[2], alpha)),
                label
            )
    
    def _handle_joint_selection(self, mouse_pos, cursor_pos, avail, leg_positions):
        """Handle clicking on joints to select them"""
        screen_center = (cursor_pos.x + avail.x / 2, cursor_pos.y + avail.y / 2)
        click_threshold = 10.0  # Pixels
        
        closest_joint = None
        closest_distance = float('inf')
        
        # Check all joints
        for leg_id, positions in leg_positions.items():
            for i, pos_3d in enumerate(positions):
                p_joint = self._project_3d_to_2d(pos_3d, self.viewport.camera, screen_center, avail.x, avail.y)
                if p_joint:
                    import math
                    dx = mouse_pos.x - p_joint[0]
                    dy = mouse_pos.y - p_joint[1]
                    distance = math.sqrt(dx*dx + dy*dy)
                    
                    if distance < click_threshold and distance < closest_distance:
                        closest_distance = distance
                        closest_joint = (leg_id, i)
        
        # Update selection
        if closest_joint:
            self.selected_joint = closest_joint
            print(f"Selected: Leg {closest_joint[0]}, Joint {closest_joint[1]}")
        else:
            self.selected_joint = None
    
    def handle_viewport_input(self):
        """Handle mouse input for viewport camera control"""
        io = imgui.get_io()
        
        # Orbit: Right Mouse Button drag OR Middle Mouse Button drag
        if imgui.is_mouse_dragging(imgui.MouseButton_.right) or (not io.key_shift and imgui.is_mouse_dragging(imgui.MouseButton_.middle)):
            # Use right mouse button if dragging, otherwise middle
            button = imgui.MouseButton_.right if imgui.is_mouse_dragging(imgui.MouseButton_.right) else imgui.MouseButton_.middle
            delta = imgui.get_mouse_drag_delta(button)
            imgui.reset_mouse_drag_delta(button)
            if abs(delta.x) > 0 or abs(delta.y) > 0:
                self.viewport.camera.orbit(-delta.x * 0.5, delta.y * 0.5)
        
        # Pan: Shift + Middle Mouse OR Middle Mouse with Shift
        elif io.key_shift and imgui.is_mouse_dragging(imgui.MouseButton_.middle):
            delta = imgui.get_mouse_drag_delta(imgui.MouseButton_.middle)
            imgui.reset_mouse_drag_delta(imgui.MouseButton_.middle)
            if abs(delta.x) > 0 or abs(delta.y) > 0:
                self.viewport.camera.pan(-delta.x, delta.y)
        
        # Zoom: Mouse Wheel
        if abs(io.mouse_wheel) > 0:
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

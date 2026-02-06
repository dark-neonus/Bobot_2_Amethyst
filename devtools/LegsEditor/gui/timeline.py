"""
Timeline Panel Module
ImGui interface for keyframe timeline
"""

from imgui_bundle import imgui
import math
from typing import Optional
from config_loader import RobotConfig
from state_manager import AppState
from core.playback import PlaybackEngine


class TimelinePanel:
    """Timeline panel with keyframe visualization and editing"""
    
    def __init__(self, config: RobotConfig, state: AppState, playback: PlaybackEngine):
        self.config = config
        self.state = state
        self.playback = playback
        
        # Timeline view settings
        self.pixels_per_second = 100.0  # Zoom level
        self.scroll_x = 0.0
        self.track_height = 30.0
        self.ruler_height = 40.0
        
        # Interaction state
        self.dragging_keyframe = None
        self.dragging_keyframe_servo = None
    
    def render(self):
        """Render the timeline panel (called by DockableWindow)"""
        # Control buttons
        self._render_controls()
        
        imgui.separator()
        
        # Timeline area
        self._render_timeline()
    
    def _render_controls(self):
        """Render timeline control buttons"""
        # Add Keyframe button
        if imgui.button("+ Keyframe"):
            self.state.add_keyframe()
        
        imgui.same_line()
        
        # Play/Pause button
        if self.state.is_playing:
            if imgui.button("⏸ Pause"):
                self.playback.pause()
        else:
            if imgui.button("▶ Play"):
                self.playback.play()
        
        imgui.same_line()
        
        # Stop button
        if imgui.button("⏹ Stop"):
            self.playback.stop()
        
        imgui.same_line()
        imgui.dummy((20, 0))
        imgui.same_line()
        
        # Loop checkbox
        changed, new_loop = imgui.checkbox("Loop", self.state.loop)
        if changed:
            self.state.loop = new_loop
        
        imgui.same_line()
        imgui.dummy((20, 0))
        imgui.same_line()
        
        # Duration input
        imgui.text("Duration:")
        imgui.same_line()
        imgui.set_next_item_width(80)
        changed, new_duration = imgui.input_float("##duration", self.state.duration, 0.1, 1.0, "%.2f")
        if changed and new_duration > 0:
            self.state.duration = new_duration
        
        imgui.same_line()
        imgui.text("seconds")
        
        imgui.same_line()
        imgui.dummy((20, 0))
        imgui.same_line()
        
        # Current time display
        imgui.text(f"Time: {self.state.current_time:.2f}s")
    
    def _render_timeline(self):
        """Render the timeline tracks and keyframes"""
        # Use child window for scrolling
        imgui.begin_child("timeline_scroll", imgui.ImVec2(0, 0), 
                         imgui.ChildFlags_.none, 
                         imgui.WindowFlags_.horizontal_scrollbar | imgui.WindowFlags_.always_vertical_scrollbar)
        
        draw_list = imgui.get_window_draw_list()
        window_pos = imgui.get_cursor_screen_pos()
        window_size = imgui.get_content_region_avail()
        
        if window_size.x < 50 or window_size.y < 50:
            imgui.end_child()
            return
        
        # Calculate total height needed for all tracks
        num_servos = self.config.leg_count * len(self.config.joints)
        total_tracks_height = num_servos * self.track_height
        
        # Calculate dimensions
        timeline_width = max(window_size.x, self.state.duration * self.pixels_per_second)
        timeline_height = self.ruler_height + total_tracks_height
        
        # Make space for content
        imgui.dummy((timeline_width, timeline_height))
        
        # Render ruler
        ruler_end_y = window_pos.y + self.ruler_height
        self._render_ruler(draw_list, window_pos, timeline_width, ruler_end_y)
        
        # Render tracks
        tracks_start_y = ruler_end_y
        self._render_tracks(draw_list, window_pos.x, tracks_start_y, timeline_width, total_tracks_height)
        
        # Handle mouse interaction
        self._handle_timeline_interaction(window_pos, timeline_width, timeline_height)
        
        imgui.end_child()
    
    def _render_ruler(self, draw_list, pos, width: float, end_y: float):
        """Render the time ruler at the top"""
        # Background
        draw_list.add_rect_filled(
            (pos.x, pos.y),
            (pos.x + width, end_y),
            imgui.get_color_u32((0.2, 0.2, 0.2, 1.0))
        )
        
        # Time markers
        duration_pixels = self.state.duration * self.pixels_per_second
        visible_start_time = self.scroll_x / self.pixels_per_second
        visible_end_time = (self.scroll_x + width) / self.pixels_per_second
        
        # Draw second markers
        time_step = 1.0 if self.pixels_per_second > 50 else 2.0
        start_time = math.floor(visible_start_time / time_step) * time_step
        
        for t in range(int(start_time), int(self.state.duration) + 2):
            if t * time_step > self.state.duration:
                break
            
            x = pos.x + (t * time_step * self.pixels_per_second) - self.scroll_x
            
            if x < pos.x or x > pos.x + width:
                continue
            
            # Marker line
            draw_list.add_line(
                (x, end_y - 15),
                (x, end_y),
                imgui.get_color_u32((0.6, 0.6, 0.6, 1.0)),
                1.0
            )
            
            # Time label
            draw_list.add_text(
                (x + 2, pos.y + 5),
                imgui.get_color_u32((0.9, 0.9, 0.9, 1.0)),
                f"{t * time_step:.1f}s"
            )
        
        # Playhead
        playhead_x = pos.x + (self.state.current_time * self.pixels_per_second) - self.scroll_x
        if pos.x <= playhead_x <= pos.x + width:
            draw_list.add_line(
                (playhead_x, pos.y),
                (playhead_x, end_y),
                imgui.get_color_u32((1.0, 0.3, 0.3, 1.0)),
                2.0
            )
    
    def _render_tracks(self, draw_list, start_x: float, start_y: float, width: float, height: float):
        """Render servo tracks with keyframes"""
        all_servos = self.config.get_all_servo_names()
        
        current_y = start_y
        
        for servo_name in all_servos:
            if current_y > start_y + height:
                break
            
            # Track background (alternating colors)
            track_color = (0.15, 0.15, 0.15, 1.0) if all_servos.index(servo_name) % 2 == 0 else (0.18, 0.18, 0.18, 1.0)
            draw_list.add_rect_filled(
                (start_x, current_y),
                (start_x + width, current_y + self.track_height),
                imgui.get_color_u32(track_color)
            )
            
            # Track label
            draw_list.add_text(
                (start_x + 5, current_y + 8),
                imgui.get_color_u32((0.8, 0.8, 0.8, 1.0)),
                servo_name
            )
            
            # Render keyframes for this servo
            self._render_keyframes_for_track(draw_list, servo_name, start_x, current_y, width)
            
            current_y += self.track_height
    
    def _render_keyframes_for_track(self, draw_list, servo_name: str, start_x: float, y: float, width: float):
        """Render keyframe diamonds for a specific servo track"""
        keyframe_size = 8.0
        center_y = y + self.track_height / 2
        
        for i, keyframe in enumerate(self.state.keyframes):
            if servo_name not in keyframe.servos:
                continue
            
            x = start_x + (keyframe.time * self.pixels_per_second) - self.scroll_x
            
            if x < start_x - 20 or x > start_x + width + 20:
                continue
            
            # Draw diamond
            points = [
                (x, center_y - keyframe_size),  # Top
                (x + keyframe_size, center_y),  # Right
                (x, center_y + keyframe_size),  # Bottom
                (x - keyframe_size, center_y),  # Left
            ]
            
            # Check if this keyframe is selected
            is_selected = self.state.selected_keyframe_index == i
            color = (1.0, 1.0, 0.3, 1.0) if is_selected else (0.3, 0.8, 1.0, 1.0)
            
            # Fill
            draw_list.add_quad_filled(
                points[0], points[1], points[2], points[3],
                imgui.get_color_u32(color)
            )
            
            # Outline
            draw_list.add_quad(
                points[0], points[1], points[2], points[3],
                imgui.get_color_u32((1.0, 1.0, 1.0, 1.0)),
                1.0
            )
    
    def _handle_timeline_interaction(self, window_pos, width: float, height: float):
        """Handle mouse interaction with timeline"""
        mouse_pos = imgui.get_mouse_pos()
        relative_x = mouse_pos.x - window_pos.x
        relative_y = mouse_pos.y - window_pos.y
        
        # Check if mouse is in timeline area
        if imgui.is_window_hovered() and 0 <= relative_x <= width and 0 <= relative_y <= height:
            # Handle scrubbing (clicking on ruler)
            if relative_y < self.ruler_height:
                if imgui.is_mouse_clicked(imgui.MouseButton_.left):
                    # Calculate time from click position
                    click_time = (relative_x + self.scroll_x) / self.pixels_per_second
                    self.playback.seek(click_time)
            
            # Handle zoom with Ctrl+Wheel
            io = imgui.get_io()
            if io.key_ctrl and io.mouse_wheel != 0:
                zoom_factor = 1.1 if io.mouse_wheel > 0 else 0.9
                self.pixels_per_second *= zoom_factor
                self.pixels_per_second = max(20.0, min(500.0, self.pixels_per_second))
            
            # Horizontal scroll with mouse wheel
            elif io.mouse_wheel != 0:
                self.scroll_x -= io.mouse_wheel * 50
                self.scroll_x = max(0, self.scroll_x)

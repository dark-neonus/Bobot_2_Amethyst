"""
Properties Panel Module
ImGui interface for robot joint control
"""

from imgui_bundle import imgui
from typing import Callable
from config_loader import RobotConfig
from state_manager import AppState


class PropertiesPanel:
    """Properties panel with servo angle sliders"""
    
    def __init__(self, config: RobotConfig, state: AppState):
        self.config = config
        self.state = state
        self.selected_joint = None  # (leg_id, joint_index) tuple
    
    def set_selected_joint(self, leg_id, joint_index):
        """Set the currently selected joint from viewport"""
        if leg_id is not None and joint_index is not None:
            self.selected_joint = (leg_id, joint_index)
        else:
            self.selected_joint = None
    
    def render(self):
        """Render the properties panel (called by DockableWindow)"""
        # Global section
        if imgui.tree_node("Global Settings"):
            # Duration control
            changed, new_duration = imgui.slider_float(
                "Duration (s)", 
                self.state.duration, 
                0.1, 
                60.0
            )
            if changed:
                self.state.duration = new_duration
                self.state.is_modified = True
            
            # Loop checkbox
            changed, new_loop = imgui.checkbox("Loop", self.state.loop)
            if changed:
                self.state.loop = new_loop
                self.state.is_modified = True
            
            imgui.tree_pop()
        
        imgui.separator()
        
        # Leg controls - expanded by default
        imgui.set_next_item_open(True, imgui.Cond_.once)
        if imgui.tree_node("Leg Controls"):
            for leg_id in range(self.config.leg_count):
                mount = self.config.get_mounting_point(leg_id)
                leg_name = mount["name"]
                
                # Collapsible header for each leg - expanded by default
                imgui.set_next_item_open(True, imgui.Cond_.once)
                if imgui.tree_node(f"Leg {leg_id} ({leg_name})"):
                    self._render_leg_controls(leg_id)
                    imgui.tree_pop()
            
            imgui.tree_pop()
    
    def _render_leg_controls(self, leg_id: int):
        """Render sliders for a single leg's joints"""
        for joint_idx, joint in enumerate(self.config.joints):
            joint_id = joint["id"]
            joint_name = joint["name"]
            servo_name = self.config.get_servo_name(leg_id, joint_id)
            
            # Get current angle
            current_angle = self.state.get_servo_angle(servo_name, 90.0)
            
            # Get joint limits
            min_angle, max_angle = self.config.get_joint_limits(joint_id)
            
            # Check if this joint is selected
            is_selected = self.selected_joint == (leg_id, joint_idx)
            
            # Highlight selected joint
            if is_selected:
                imgui.push_style_color(imgui.Col_.frame_bg, (0.2, 0.6, 0.2, 0.5))
                imgui.push_style_color(imgui.Col_.frame_bg_hovered, (0.3, 0.7, 0.3, 0.7))
                imgui.push_style_color(imgui.Col_.frame_bg_active, (0.4, 0.8, 0.4, 0.9))
            
            # Slider
            imgui.push_id(servo_name)
            changed, new_angle = imgui.slider_float(
                f"J{joint_id} ({joint_name})",
                current_angle,
                min_angle,
                max_angle,
                "%.1f°"
            )
            
            if changed:
                self.state.set_servo_angle(servo_name, new_angle)
                # Set this joint as selected when changed
                self.selected_joint = (leg_id, joint_idx)
            
            # Add input field for direct angle entry
            imgui.same_line()
            imgui.set_next_item_width(70)
            input_changed, input_angle = imgui.input_float(
                f"##input_{servo_name}",
                current_angle,
                0.0,
                0.0,
                "%.1f"
            )
            
            if input_changed:
                # Clamp to valid range
                input_angle = max(min_angle, min(max_angle, input_angle))
                self.state.set_servo_angle(servo_name, input_angle)
                # Set this joint as selected when changed
                self.selected_joint = (leg_id, joint_idx)
            
            imgui.pop_id()
            
            # Pop highlight colors
            if is_selected:
                imgui.pop_style_color(3)
        
        imgui.spacing()
        
        # Quick preset buttons
        if imgui.button("Reset to 90°"):
            for joint in self.config.joints:
                joint_id = joint["id"]
                servo_name = self.config.get_servo_name(leg_id, joint_id)
                self.state.set_servo_angle(servo_name, 90.0)

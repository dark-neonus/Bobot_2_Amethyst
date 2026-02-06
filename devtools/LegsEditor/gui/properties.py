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
        
        # Leg controls
        if imgui.tree_node("Leg Controls"):
            for leg_id in range(self.config.leg_count):
                mount = self.config.get_mounting_point(leg_id)
                leg_name = mount["name"]
                
                # Collapsible header for each leg
                if imgui.tree_node(f"Leg {leg_id} ({leg_name})"):
                    self._render_leg_controls(leg_id)
                    imgui.tree_pop()
            
            imgui.tree_pop()
    
    def _render_leg_controls(self, leg_id: int):
        """Render sliders for a single leg's joints"""
        for joint in self.config.joints:
            joint_id = joint["id"]
            joint_name = joint["name"]
            servo_name = self.config.get_servo_name(leg_id, joint_id)
            
            # Get current angle
            current_angle = self.state.get_servo_angle(servo_name, 90.0)
            
            # Get joint limits
            min_angle, max_angle = self.config.get_joint_limits(joint_id)
            
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
            
            imgui.pop_id()
        
        imgui.spacing()
        
        # Quick preset buttons
        if imgui.button("Reset to 90°"):
            for joint in self.config.joints:
                joint_id = joint["id"]
                servo_name = self.config.get_servo_name(leg_id, joint_id)
                self.state.set_servo_angle(servo_name, 90.0)

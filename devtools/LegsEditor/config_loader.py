"""
Configuration Loader Module
Loads and parses robot_config.json
"""

import json
import os
from typing import Dict, List, Any


class RobotConfig:
    """Holds the robot's physical configuration data"""
    
    def __init__(self, config_path: str = None):
        if config_path is None:
            # Default to resources/robot_config.json relative to this file
            config_path = os.path.join(
                os.path.dirname(__file__), 
                "resources", 
                "robot_config.json"
            )
        
        with open(config_path, 'r') as f:
            self.data = json.load(f)
        
        # Parse body dimensions
        self.body_length = self.data["body"]["length_mm"]
        self.body_width = self.data["body"]["width_mm"]
        self.body_height = self.data["body"]["height_mm"]
        
        # Parse leg configuration
        self.leg_count = self.data["legs"]["count"]
        self.mounting_points = self.data["legs"]["mounting_points"]
        
        # Parse segment lengths
        self.coxa_len = self.data["legs"]["segments"]["coxa_len_mm"]
        self.femur_len = self.data["legs"]["segments"]["femur_len_mm"]
        self.tibia_len = self.data["legs"]["segments"]["tibia_len_mm"]
        
        # Parse joint definitions
        self.joints = self.data["legs"]["joints"]
    
    def get_mounting_point(self, leg_id: int) -> Dict[str, Any]:
        """Get mounting point data for a specific leg"""
        for mp in self.mounting_points:
            if mp["id"] == leg_id:
                return mp
        return None
    
    def get_joint_limits(self, joint_id: int) -> tuple:
        """Get min/max limits for a joint"""
        for joint in self.joints:
            if joint["id"] == joint_id:
                return (joint["min"], joint["max"])
        return (0, 180)
    
    def get_servo_name(self, leg_id: int, joint_id: int) -> str:
        """Generate servo identifier string"""
        return f"leg{leg_id}_j{joint_id}"
    
    def get_all_servo_names(self) -> List[str]:
        """Get list of all servo identifiers"""
        servos = []
        for leg_id in range(self.leg_count):
            for joint in self.joints:
                servos.append(self.get_servo_name(leg_id, joint["id"]))
        return servos

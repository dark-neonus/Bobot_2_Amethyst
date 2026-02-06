"""
Forward Kinematics Module
Calculates 3D positions of robot joints based on servo angles
"""

import numpy as np
from typing import Dict, List, Tuple
import math


class LegKinematics:
    """Forward kinematics for a single leg"""
    
    def __init__(self, coxa_len: float, femur_len: float, tibia_len: float):
        self.coxa_len = coxa_len
        self.femur_len = femur_len
        self.tibia_len = tibia_len
    
    def calculate(self, 
                  mount_pos: Tuple[float, float, float],
                  base_rotation: float,
                  j1_angle: float, 
                  j2_angle: float, 
                  j3_angle: float) -> List[np.ndarray]:
        """
        Calculate the position of all joints and the tip of the leg.
        
        Args:
            mount_pos: (x, y, z) mounting point on body
            base_rotation: Base rotation of the leg in degrees
            j1_angle: Coxa joint angle (0-180, rotates around Y axis)
            j2_angle: Femur joint angle (0-180, rotates around local Z axis)
            j3_angle: Tibia joint angle (0-180, rotates around local Z axis)
        
        Returns:
            List of positions: [mount, j1, j2, j3, tip]
        """
        positions = []
        
        # Starting position (mounting point)
        pos = np.array(mount_pos, dtype=float)
        positions.append(pos.copy())
        
        # Convert angles from degrees to radians
        # Servo angles are 0-180, convert to -90 to 90 for symmetry around 90°
        base_rad = math.radians(base_rotation)
        j1_rad = math.radians(j1_angle - 90)  # Coxa: rotates around Y axis
        j2_rad = math.radians(j2_angle - 90)  # Femur: rotates around local Z 
        j3_rad = math.radians(j3_angle - 90)  # Tibia: rotates around local Z
        
        # Joint 1 (Coxa) - horizontal rotation around Y axis
        # Combined with base rotation gives total horizontal angle
        total_yaw = base_rad + j1_rad
        
        # Coxa extends horizontally
        j1_pos = pos + np.array([
            self.coxa_len * math.cos(total_yaw),
            0,  # No vertical change
            self.coxa_len * math.sin(total_yaw)
        ])
        positions.append(j1_pos.copy())
        
        # Joint 2 (Femur) - rotates around local Z axis (perpendicular to leg direction)
        # This creates vertical motion
        # Direction in XZ plane (horizontal)
        horiz_dir = np.array([math.cos(total_yaw), 0, math.sin(total_yaw)])
        
        # Femur pitch affects both horizontal reach and vertical position
        j2_pos = j1_pos + np.array([
            self.femur_len * math.cos(total_yaw) * math.cos(j2_rad),
            -self.femur_len * math.sin(j2_rad),  # Negative Y is down
            self.femur_len * math.sin(total_yaw) * math.cos(j2_rad)
        ])
        positions.append(j2_pos.copy())
        
        # Joint 3 (Tibia) - also rotates around local Z axis
        # Cumulative pitch with femur
        cumulative_pitch = j2_rad + j3_rad
        
        j3_pos = j2_pos + np.array([
            self.tibia_len * math.cos(total_yaw) * math.cos(cumulative_pitch),
            -self.tibia_len * math.sin(cumulative_pitch),
            self.tibia_len * math.sin(total_yaw) * math.cos(cumulative_pitch)
        ])
        positions.append(j3_pos.copy())
        
        return positions
        
        return positions


class RobotKinematics:
    """Forward kinematics for the entire robot"""
    
    def __init__(self, config):
        """Initialize with robot configuration"""
        self.config = config
        self.leg_fk = LegKinematics(
            config.coxa_len,
            config.femur_len,
            config.tibia_len
        )
    
    def calculate_all_legs(self, servo_angles: Dict[str, float]) -> Dict[int, List[np.ndarray]]:
        """
        Calculate positions for all legs.
        
        Args:
            servo_angles: Dictionary mapping servo names to angles
        
        Returns:
            Dictionary mapping leg_id to list of joint positions
        """
        leg_positions = {}
        
        for leg_id in range(self.config.leg_count):
            mount = self.config.get_mounting_point(leg_id)
            
            # Get servo angles (default to 90 if not specified)
            j1_name = self.config.get_servo_name(leg_id, 1)
            j2_name = self.config.get_servo_name(leg_id, 2)
            j3_name = self.config.get_servo_name(leg_id, 3)
            
            j1_angle = servo_angles.get(j1_name, 90.0)
            j2_angle = servo_angles.get(j2_name, 90.0)
            j3_angle = servo_angles.get(j3_name, 90.0)
            
            # Calculate positions
            mount_pos = (mount["x"], mount["y"], mount["z"])
            positions = self.leg_fk.calculate(
                mount_pos,
                mount["base_rotation"],
                j1_angle,
                j2_angle,
                j3_angle
            )
            
            leg_positions[leg_id] = positions
        
        return leg_positions
    
    def get_body_corners(self) -> List[np.ndarray]:
        """Get the 8 corners of the body box for rendering"""
        hx = self.config.body_length / 2
        hy = self.config.body_height / 2
        hz = self.config.body_width / 2
        
        corners = [
            np.array([hx, hy, hz]),
            np.array([hx, hy, -hz]),
            np.array([hx, -hy, hz]),
            np.array([hx, -hy, -hz]),
            np.array([-hx, hy, hz]),
            np.array([-hx, hy, -hz]),
            np.array([-hx, -hy, hz]),
            np.array([-hx, -hy, -hz]),
        ]
        
        return corners

"""
GUI Utilities Module
Camera class and helper functions for 3D interaction
"""

import numpy as np
import math
from typing import Tuple


class Camera:
    """3D Camera for viewport navigation (Fusion 360 style)"""
    
    def __init__(self):
        # Camera position
        self.distance = 500.0  # Distance from target
        self.azimuth = 45.0    # Horizontal angle (degrees)
        self.elevation = 30.0  # Vertical angle (degrees)
        
        # Target point (orbit center)
        self.target = np.array([0.0, 0.0, 0.0])
        
        # Calculated camera position
        self.position = np.array([0.0, 0.0, 0.0])
        self.update_position()
    
    def update_position(self):
        """Calculate camera position from spherical coordinates"""
        # Convert to radians
        azimuth_rad = math.radians(self.azimuth)
        elevation_rad = math.radians(self.elevation)
        
        # Spherical to Cartesian
        x = self.distance * math.cos(elevation_rad) * math.cos(azimuth_rad)
        y = self.distance * math.sin(elevation_rad)
        z = self.distance * math.cos(elevation_rad) * math.sin(azimuth_rad)
        
        self.position = self.target + np.array([x, y, z])
    
    def orbit(self, delta_azimuth: float, delta_elevation: float):
        """Orbit camera around target (Shift + Middle Mouse)"""
        self.azimuth += delta_azimuth
        self.elevation += delta_elevation
        
        # Clamp elevation to avoid gimbal lock
        self.elevation = max(-89.0, min(89.0, self.elevation))
        
        self.update_position()
    
    def pan(self, delta_x: float, delta_y: float):
        """Pan camera (Middle Mouse)"""
        # Calculate camera right and up vectors
        azimuth_rad = math.radians(self.azimuth)
        
        # Right vector (perpendicular to view direction in XZ plane)
        right = np.array([
            -math.sin(azimuth_rad),
            0,
            math.cos(azimuth_rad)
        ])
        
        # Up vector (world Y)
        up = np.array([0, 1, 0])
        
        # Pan speed proportional to distance
        pan_speed = self.distance * 0.001
        
        # Move target
        self.target += right * delta_x * pan_speed
        self.target += up * delta_y * pan_speed
        
        self.update_position()
    
    def zoom(self, delta: float):
        """Zoom camera (Mouse Wheel)"""
        zoom_speed = self.distance * 0.1
        self.distance -= delta * zoom_speed
        
        # Clamp distance
        self.distance = max(50.0, min(2000.0, self.distance))
        
        self.update_position()
    
    def get_view_matrix(self) -> np.ndarray:
        """Get view matrix for rendering"""
        # Calculate forward, right, up vectors
        forward = self.target - self.position
        forward = forward / np.linalg.norm(forward)
        
        world_up = np.array([0.0, 1.0, 0.0])
        right = np.cross(forward, world_up)
        right = right / np.linalg.norm(right)
        
        up = np.cross(right, forward)
        
        # Build view matrix
        view = np.eye(4, dtype=np.float32)
        
        view[0, 0:3] = right
        view[1, 0:3] = up
        view[2, 0:3] = -forward
        
        view[0, 3] = -np.dot(right, self.position)
        view[1, 3] = -np.dot(up, self.position)
        view[2, 3] = np.dot(forward, self.position)
        
        return view
    
    def snap_to_view(self, view_name: str):
        """Snap to predefined views (Front, Top, Right, etc.)"""
        views = {
            "front": (0.0, 0.0),
            "back": (180.0, 0.0),
            "right": (90.0, 0.0),
            "left": (-90.0, 0.0),
            "top": (0.0, 89.0),
            "bottom": (0.0, -89.0),
        }
        
        if view_name.lower() in views:
            self.azimuth, self.elevation = views[view_name.lower()]
            self.update_position()


def perspective_matrix(fov: float, aspect: float, near: float, far: float) -> np.ndarray:
    """Create perspective projection matrix"""
    f = 1.0 / math.tan(math.radians(fov) / 2.0)
    
    mat = np.zeros((4, 4), dtype=np.float32)
    mat[0, 0] = f / aspect
    mat[1, 1] = f
    mat[2, 2] = (far + near) / (near - far)
    mat[2, 3] = (2.0 * far * near) / (near - far)
    mat[3, 2] = -1.0
    
    return mat


def ray_from_mouse(mouse_x: float, mouse_y: float, 
                   viewport_width: float, viewport_height: float,
                   projection_matrix: np.ndarray, 
                   view_matrix: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
    """
    Convert mouse coordinates to a ray in world space.
    Returns (ray_origin, ray_direction)
    """
    # Normalize mouse coordinates to [-1, 1]
    x = (2.0 * mouse_x) / viewport_width - 1.0
    y = 1.0 - (2.0 * mouse_y) / viewport_height
    
    # Ray in clip space
    ray_clip = np.array([x, y, -1.0, 1.0])
    
    # Ray in view space
    ray_view = np.linalg.inv(projection_matrix) @ ray_clip
    ray_view[2] = -1.0
    ray_view[3] = 0.0
    
    # Ray in world space
    ray_world = np.linalg.inv(view_matrix) @ ray_view
    ray_direction = ray_world[0:3]
    ray_direction = ray_direction / np.linalg.norm(ray_direction)
    
    # Ray origin is camera position
    camera_pos = np.linalg.inv(view_matrix)[0:3, 3]
    
    return camera_pos, ray_direction


def ray_sphere_intersection(ray_origin: np.ndarray, ray_direction: np.ndarray,
                            sphere_center: np.ndarray, sphere_radius: float) -> float:
    """
    Test ray-sphere intersection. Returns distance along ray, or -1 if no hit.
    """
    oc = ray_origin - sphere_center
    a = np.dot(ray_direction, ray_direction)
    b = 2.0 * np.dot(oc, ray_direction)
    c = np.dot(oc, oc) - sphere_radius * sphere_radius
    
    discriminant = b * b - 4 * a * c
    
    if discriminant < 0:
        return -1.0
    
    t = (-b - math.sqrt(discriminant)) / (2.0 * a)
    return t if t > 0 else -1.0

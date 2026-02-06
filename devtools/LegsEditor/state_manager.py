"""
State Manager Module
Holds the application state including animation data, current time, and selection
"""

import json
from typing import Dict, List, Optional, Any
from dataclasses import dataclass, field


@dataclass
class Keyframe:
    """Represents a single keyframe in the animation"""
    time: float
    servos: Dict[str, float] = field(default_factory=dict)
    
    def get_angle(self, servo_name: str, default: float = 90.0) -> float:
        """Get angle for a servo, or return default if not set"""
        return self.servos.get(servo_name, default)
    
    def set_angle(self, servo_name: str, angle: float):
        """Set angle for a servo"""
        self.servos[servo_name] = angle
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for JSON serialization"""
        return {
            "time": self.time,
            "servos": self.servos
        }
    
    @staticmethod
    def from_dict(data: Dict[str, Any]) -> 'Keyframe':
        """Create Keyframe from dictionary"""
        return Keyframe(
            time=data["time"],
            servos=data.get("servos", {})
        )


class AppState:
    """Main application state manager"""
    
    def __init__(self):
        # Animation data
        self.duration = 5.0
        self.loop = True
        self.keyframes: List[Keyframe] = []
        
        # Playback state
        self.current_time = 0.0
        self.is_playing = False
        
        # Current pose (for editing/preview)
        self.current_pose: Dict[str, float] = {}
        
        # UI state
        self.selected_leg = None
        self.selected_joint = None
        self.selected_keyframe_index = None
        
        # File state
        self.current_file = None
        self.is_modified = False
        
    def add_keyframe(self, time: float = None) -> Keyframe:
        """Add a new keyframe at specified time with current pose"""
        if time is None:
            time = self.current_time
        
        # Check if keyframe already exists at this time
        for i, kf in enumerate(self.keyframes):
            if abs(kf.time - time) < 0.01:  # Within 10ms
                # Update existing keyframe
                kf.servos = self.current_pose.copy()
                self.is_modified = True
                return kf
        
        # Create new keyframe
        kf = Keyframe(time=time, servos=self.current_pose.copy())
        self.keyframes.append(kf)
        self.keyframes.sort(key=lambda x: x.time)
        self.is_modified = True
        return kf
    
    def delete_keyframe(self, index: int):
        """Delete keyframe at specified index"""
        if 0 <= index < len(self.keyframes):
            self.keyframes.pop(index)
            self.is_modified = True
    
    def get_keyframe_at_time(self, time: float, tolerance: float = 0.01) -> Optional[Keyframe]:
        """Get keyframe at specified time (within tolerance)"""
        for kf in self.keyframes:
            if abs(kf.time - time) < tolerance:
                return kf
        return None
    
    def get_surrounding_keyframes(self, time: float) -> tuple:
        """Get keyframes before and after specified time for interpolation"""
        if not self.keyframes:
            return (None, None)
        
        # Find keyframes
        before = None
        after = None
        
        for kf in self.keyframes:
            if kf.time <= time:
                before = kf
            if kf.time >= time and after is None:
                after = kf
                break
        
        return (before, after)
    
    def set_servo_angle(self, servo_name: str, angle: float):
        """Set angle for a servo in current pose"""
        self.current_pose[servo_name] = angle
        
        # If we're at a keyframe, update it
        kf = self.get_keyframe_at_time(self.current_time)
        if kf:
            kf.set_angle(servo_name, angle)
            self.is_modified = True
    
    def get_servo_angle(self, servo_name: str, default: float = 90.0) -> float:
        """Get current angle for a servo"""
        return self.current_pose.get(servo_name, default)
    
    def initialize_default_pose(self, servo_names: List[str]):
        """Initialize all servos to neutral position (90 degrees)"""
        for name in servo_names:
            if name not in self.current_pose:
                self.current_pose[name] = 90.0
    
    def save_to_file(self, filepath: str):
        """Save animation to JSON file"""
        data = {
            "meta": {
                "duration_sec": self.duration,
                "loop": self.loop
            },
            "keyframes": [kf.to_dict() for kf in self.keyframes]
        }
        
        with open(filepath, 'w') as f:
            json.dump(data, f, indent=2)
        
        self.current_file = filepath
        self.is_modified = False
    
    def load_from_file(self, filepath: str):
        """Load animation from JSON file"""
        with open(filepath, 'r') as f:
            data = json.load(f)
        
        # Load metadata
        meta = data.get("meta", {})
        self.duration = meta.get("duration_sec", 5.0)
        self.loop = meta.get("loop", True)
        
        # Load keyframes
        self.keyframes = [
            Keyframe.from_dict(kf_data) 
            for kf_data in data.get("keyframes", [])
        ]
        
        # Sort keyframes by time
        self.keyframes.sort(key=lambda x: x.time)
        
        # Reset playback
        self.current_time = 0.0
        self.is_playing = False
        
        self.current_file = filepath
        self.is_modified = False
    
    def new_animation(self):
        """Create a new animation, clearing all data"""
        self.duration = 5.0
        self.loop = True
        self.keyframes = []
        self.current_time = 0.0
        self.is_playing = False
        self.current_file = None
        self.is_modified = False

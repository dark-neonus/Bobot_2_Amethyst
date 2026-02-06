"""
Playback Engine Module
Handles timeline playback, interpolation, and time management
"""

from typing import Dict, Optional
from state_manager import AppState


class PlaybackEngine:
    """Manages animation playback and interpolation"""
    
    def __init__(self, state: AppState):
        self.state = state
    
    def update(self, delta_time: float):
        """Update playback state and interpolate servo angles"""
        if not self.state.is_playing:
            return
        
        # Increment time
        self.state.current_time += delta_time
        
        # Handle looping
        if self.state.current_time > self.state.duration:
            if self.state.loop:
                self.state.current_time = 0.0
            else:
                self.state.current_time = self.state.duration
                self.state.is_playing = False
        
        # Interpolate servo angles at current time
        self.interpolate_pose()
    
    def interpolate_pose(self):
        """Calculate interpolated servo angles at current time"""
        if not self.state.keyframes:
            return
        
        # Get surrounding keyframes
        before, after = self.state.get_surrounding_keyframes(self.state.current_time)
        
        if before is None and after is None:
            # No keyframes
            return
        
        if before is None:
            # Before first keyframe - use first keyframe
            self.state.current_pose = after.servos.copy()
            return
        
        if after is None:
            # After last keyframe - use last keyframe
            self.state.current_pose = before.servos.copy()
            return
        
        if before == after:
            # Exactly on a keyframe
            self.state.current_pose = before.servos.copy()
            return
        
        # Interpolate between keyframes
        time_diff = after.time - before.time
        if time_diff <= 0:
            self.state.current_pose = before.servos.copy()
            return
        
        t = (self.state.current_time - before.time) / time_diff
        t = max(0.0, min(1.0, t))  # Clamp to [0, 1]
        
        # Linear interpolation for each servo
        all_servo_names = set(before.servos.keys()) | set(after.servos.keys())
        
        for servo_name in all_servo_names:
            angle_before = before.get_angle(servo_name, 90.0)
            angle_after = after.get_angle(servo_name, 90.0)
            
            # Linear interpolation
            interpolated = angle_before + (angle_after - angle_before) * t
            self.state.current_pose[servo_name] = interpolated
    
    def play(self):
        """Start playback"""
        self.state.is_playing = True
        # If at the end, restart from beginning
        if self.state.current_time >= self.state.duration:
            self.state.current_time = 0.0
    
    def pause(self):
        """Pause playback"""
        self.state.is_playing = False
    
    def stop(self):
        """Stop playback and return to beginning"""
        self.state.is_playing = False
        self.state.current_time = 0.0
        self.interpolate_pose()
    
    def toggle_play_pause(self):
        """Toggle between play and pause"""
        if self.state.is_playing:
            self.pause()
        else:
            self.play()
    
    def seek(self, time: float):
        """Seek to a specific time"""
        self.state.current_time = max(0.0, min(time, self.state.duration))
        self.interpolate_pose()

# Running the Bobot Legs Editor

## Quick Start

1. **Install dependencies:**
   ```bash
   pip install -r requirements.txt
   ```

2. **Run the application:**
   ```bash
   python main.py
   ```

## Application Layout

The application window is divided into three main panels:

### 3D Viewport (Center-Left)
- Displays the robot in 3D with real-time forward kinematics
- **Grid**: XZ plane grid for reference
- **Robot Body**: White wireframe box
- **Legs**: Color-coded (Red, Green, Blue, Yellow)
- **Joints**: Small white crosses at joint locations

**Camera Controls:**
- **Orbit**: Hold `Shift` + `Middle Mouse Button` and drag
- **Pan**: Hold `Middle Mouse Button` and drag
- **Zoom**: Scroll `Mouse Wheel`

### Properties Panel (Right)
- **Global Settings**: Duration and Loop checkbox
- **Leg Controls**: Expandable sections for each leg (0-3)
  - Sliders for each joint (J1-Coxa, J2-Femur, J3-Tibia)
  - Range: 0-180 degrees
  - "Reset to 90°" button per leg

### Timeline Panel (Bottom)
- **Controls**: Add Keyframe, Play/Pause, Stop, Loop, Duration
- **Time Ruler**: Click to scrub to a time position
- **Tracks**: One row per servo (leg0_j1, leg0_j2, etc.)
- **Keyframes**: Diamond shapes on tracks
  - Click to select
  - Right-click to delete
- **Zoom**: `Ctrl` + `Mouse Wheel`
- **Scroll**: `Mouse Wheel` (vertical)

## Keyboard Shortcuts

- `Ctrl+N`: New animation
- `Ctrl+O`: Open animation file
- `Ctrl+S`: Save animation
- `Ctrl+Shift+S`: Save As...
- `Space`: Play/Pause toggle

## Workflow

1. **Start**: Application opens with all servos at 90° and one keyframe at time 0
2. **Pose**: Adjust sliders to create a desired robot pose
3. **Keyframe**: Click "+ Keyframe" to save the current pose at the current time
4. **Repeat**: Scrub to a different time, adjust pose, add another keyframe
5. **Play**: Click Play to see interpolated animation
6. **Export**: Save to JSON file

## File Format

Animations are saved as JSON with this structure:
```json
{
  "meta": {
    "duration_sec": 5.0,
    "loop": true
  },
  "keyframes": [
    {
      "time": 0.0,
      "servos": {
        "leg0_j1": 90,
        "leg0_j2": 90,
        ...
      }
    }
  ]
}
```

## Troubleshooting

### "Viewport not initialized"
- The ModernGL context failed to create
- Check that your system supports OpenGL 3.3+
- Try running with `LIBGL_ALWAYS_SOFTWARE=1 python main.py` for software rendering

### No 3D display
- Ensure the 3D Viewport window is visible
- Try resizing the viewport window
- Check terminal output for errors

### Window layout issues
- Close and reopen the application
- The docking layout is saved and restored automatically
- Delete `imgui.ini` if layout is corrupted

## Development Mode

To test core functionality without GUI:
```bash
python -c "
from config_loader import RobotConfig
from state_manager import AppState
from core.kinematics import RobotKinematics

config = RobotConfig()
state = AppState()
state.initialize_default_pose(config.get_all_servo_names())

kinematics = RobotKinematics(config)
positions = kinematics.calculate_all_legs(state.current_pose)

print(f'Calculated positions for {len(positions)} legs')
"
```

## Performance

- Target: 60 FPS
- Typical: 30-60 FPS depending on hardware
- The 3D viewport updates every frame
- Timeline and properties update on interaction only

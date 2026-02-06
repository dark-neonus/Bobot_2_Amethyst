# Bobot Legs Editor

A lightweight, fast, and modular Python-based visual editor for designing gait patterns (animations) for a 4-legged spider robot.

## Features

- **3D Visualization**: Real-time 3D rendering of the robot with forward kinematics
- **Interactive Timeline**: Visual keyframe editor with drag-and-drop support
- **Properties Panel**: Intuitive sliders for controlling servo angles (0-180°)
- **Playback Engine**: Play, pause, loop animations with smooth interpolation
- **Save/Load**: Export and import animations as JSON files
- **Camera Controls**: Fusion 360-style camera (orbit, pan, zoom)

## Installation

1. Install Python 3.10 or higher
2. Install system dependencies (for dev containers/headless environments):

```bash
# Debian/Ubuntu
apt-get update && apt-get install -y xvfb

# The application will automatically use Xvfb for headless rendering
```

3. Install Python dependencies:

```bash
pip install -r requirements.txt
```

## Usage

Run the application:

```bash
python main.py
```

The application automatically handles X11 display setup:
- ✅ Detects if X display is available
- ✅ Auto-starts Xvfb virtual framebuffer if needed
- ✅ Works in dev containers, SSH sessions, and headless environments
- ✅ Cleans up resources on exit

**Viewing the GUI (from dev container/Windows):**

Xvfb is virtual (headless) by default. To see the actual window:

```bash
# Start VNC server
./start_vnc_tiger.sh

# Then open in browser: http://localhost:6080/vnc.html
# Click "Connect" and run in another terminal:
export DISPLAY=:99
python main.py
```

**Testing the display fix:**
```bash
python test_display_fix.py
```

### Controls

#### 3D Viewport
- **Orbit**: `Shift` + `Middle Mouse Drag`
- **Pan**: `Middle Mouse Drag`
- **Zoom**: `Mouse Wheel`

#### Timeline
- **Scrub**: Click on time ruler to jump to that time
- **Add Keyframe**: Click "+ Keyframe" button
- **Play/Pause**: Click play button or press `Space`
- **Zoom Timeline**: `Ctrl` + `Mouse Wheel`
- **Scroll Timeline**: `Mouse Wheel`

#### Keyboard Shortcuts
- `Ctrl+N`: New animation
- `Ctrl+O`: Open animation
- `Ctrl+S`: Save animation
- `Ctrl+Shift+S`: Save As
- `Space`: Play/Pause

## File Format

Animations are saved as JSON files with the following structure:

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
        "leg0_j3": 90,
        ...
      }
    }
  ]
}
```

## Robot Configuration

The robot's physical dimensions and joint constraints are defined in `resources/robot_config.json`:

- **Body**: 120mm x 120mm x 40mm
- **Legs**: 4 legs with 3 joints each
  - Coxa: 40mm (horizontal rotation)
  - Femur: 80mm (vertical rotation)
  - Tibia: 120mm (vertical rotation)

## Project Structure

```
LegsEditor/
├── main.py                 # Application entry point
├── config_loader.py        # Robot configuration loader
├── state_manager.py        # Application state management
├── core/
│   ├── kinematics.py       # Forward kinematics calculations
│   └── playback.py         # Animation playback engine
├── gui/
│   ├── viewport.py         # 3D rendering (ModernGL)
│   ├── timeline.py         # Timeline panel (ImGui)
│   ├── properties.py       # Properties panel (ImGui)
│   └── utils.py            # Camera and helper functions
└── resources/
    └── robot_config.json   # Robot physical configuration
```

## Technology Stack

- **GUI**: imgui-bundle (Dear ImGui + Hello ImGui)
- **3D Rendering**: ModernGL
- **Math**: NumPy, PyGLM
- **Language**: Python 3.10+

## Development

This project follows a "vibecoding" approach - prioritizing rapid iteration, minimal boilerplate, and high interactivity.

## Troubleshooting

### GLFW Error: Failed to open display

**Error:**
```
Glfw Error 65544: X11: Failed to open display :4
RuntimeError: IM_ASSERT( glfwInitSuccess )
```

**Solution:**
This error occurs when no X server is available. The application now automatically handles this by starting Xvfb. If you still encounter issues:

1. Ensure Xvfb is installed:
   ```bash
   apt-get install xvfb
   ```

2. Check the logs in `logs.md` for detailed troubleshooting steps

3. Verify the fix is working:
   ```bash
   python test_display_fix.py
   ```

### OpenGL Rendering Errors

If you see `GL_INVALID_VALUE` errors, these are separate from the GLFW initialization and indicate issues in the viewport rendering code (`gui/viewport.py`). The application should still run despite these warnings.

## License

See LICENSE file in the project root.

## Credits

Created for the Bobot 2 Amethyst spider robot project.

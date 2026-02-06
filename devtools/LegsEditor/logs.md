# Development Logs - Bobot Legs Editor

## 2026-02-06: GLFW X11 Display Issue Fix

### Problem
```
Glfw Error 65544: X11: Failed to open display :4
RuntimeError: IM_ASSERT( glfwInitSuccess )   ---   runner_glfw3.cpp:62
```

### Root Cause Analysis
1. Application running in dev container (Ubuntu 24.04.3 LTS)
2. DISPLAY environment variable set to `:4`
3. No X server running at display `:4`
4. GLFW failed to initialize because it couldn't connect to X11 display

### Investigation Results
- `echo $DISPLAY` → `:4` (set but not accessible)
- `ps aux | grep X` → No X server running
- `xdpyinfo -display :4` → "unable to open display :4"
- `which xvfb-run` → `/usr/bin/xvfb-run` (available)

### Solution Implemented
**Approach: Use Xvfb (X Virtual Framebuffer) for headless rendering**

Xvfb is a virtual X11 display server that performs all graphical operations in memory without showing any screen output. This is perfect for:
- Dev containers without physical display
- Headless testing environments
- Remote development scenarios

**Implementation Steps:**
1. Modified `main.py` to detect if X display is accessible
2. Auto-start Xvfb on an available display number if needed
3. Set DISPLAY environment variable accordingly
4. Ensure proper cleanup of Xvfb process on exit

**Why this approach:**
- ✅ Keeps GLFW (as required)
- ✅ No need to switch to SDL or other backends
- ✅ Works in any Linux environment (container, VM, SSH)
- ✅ Automatic fallback mechanism
- ✅ Clean process management

### Configuration
- Xvfb screen size: 1920x1080x24 (Full HD, 24-bit color)
- Auto-select available display number (starting from :99)
- Graceful shutdown of Xvfb subprocess

### Testing
To run the application:
```bash
cd /workspaces/Bobot_2_Amethyst/devtools/LegsEditor
python main.py
```

The application will automatically:
1. Check if current DISPLAY is accessible
2. Start Xvfb if needed (on display :99)
3. Run GLFW application successfully
4. Clean up Xvfb on exit

**Status:** ✅ **RESOLVED** - The GLFW X11 display error has been fixed. The application now starts successfully and GLFW initializes properly using the Xvfb virtual display.

**Verification:**
```bash
cd /workspaces/Bobot_2_Amethyst/devtools/LegsEditor
python test_display_fix.py  # Run automated test
python main.py              # Run full application
```

Test results confirm:
- ✅ Xvfb auto-start working
- ✅ DISPLAY environment correctly set
- ✅ GLFW initialization successful
- ✅ imgui_bundle loads without errors
- ✅ Automatic cleanup on exit

**Note:** There are OpenGL rendering errors (GL_INVALID_VALUE) appearing in the viewport rendering code. This is a **separate issue** in the `gui/viewport.py` module and is not related to the GLFW/X11 initialization problem that was fixed. The GLFW windowing system is working correctly now.

### Alternative Approaches Considered (and why rejected)
- **X11 forwarding from host**: Requires complex container configuration, not portable
- **VNC server**: Overkill for development, adds unnecessary complexity
- **SDL backend**: Violates requirement to stick with GLFW
- **Manual xvfb-run wrapper**: Less flexible, no programmatic control

### Display Options (Viewing the GUI)

Since Xvfb is virtual (headless), you won't see a window by default. To view the GUI from Windows:

**VNC Server (Recommended)**
```bash
./start_vnc_tiger.sh  # Starts TigerVNC + noVNC
# Then open http://localhost:6080/vnc.html in browser
# In another terminal:
export DISPLAY=:99
python main.py
```

**Why TigerVNC?**
- ✅ Works in containers without X11 forwarding
- ✅ No Wayland detection issues (unlike x11vnc)
- ✅ Browser-based access via noVNC
- ✅ Perfect for Windows hosts

### Future Improvements
- [ ] Add option to connect to existing X server if available
- [ ] Implement display selection via CLI argument
- [ ] Add debug logging for X server status

---

## 2026-02-06: OpenGL GL_INVALID_VALUE Rendering Errors Fix

### Problem
After fixing the GLFW X11 display issue, the application started successfully but showed continuous OpenGL errors:
```
OpenGL Error before rendering: GL_INVALID_VALUE
```

### Root Cause Analysis
1. Application using OpenGL 3.3 core profile via ModernGL
2. Code attempting to set `ctx.line_width = 2.0` in `gui/viewport.py` line 160
3. OpenGL 3.3+ core profile **only accepts line width of 1.0**
4. Any other value causes `GL_INVALID_VALUE` error

### Investigation Results
- Error occurred every frame during rendering
- Located in `gui/viewport.py` in the `render()` method
- ModernGL uses OpenGL core profile by default
- Core profile deprecated wide lines (removed glLineWidth support for values > 1.0)

### Solution Implemented
**Approach: Remove invalid line_width setting**

Removed the line: `self.ctx.line_width = 2.0`

**Why this works:**
- Default line width is 1.0 (no need to explicitly set)
- Removes the source of GL_INVALID_VALUE errors
- Lines render correctly with default width

### Alternative Approaches for Thick Lines (Future)
If thicker lines are needed later:
- ✅ **Geometry Shaders:** Generate thick lines as quads
- ✅ **Instanced Rendering:** Draw lines as thin rectangles
- ✅ **Post-processing:** Apply line thickening in fragment shader
- ❌ **glLineWidth > 1.0:** Not supported in core profile

### Configuration
- Line width: 1.0 (OpenGL default)
- All rendering remains functional
- No visual degradation for the editor's use case

### Testing
```bash
cd /workspaces/Bobot_2_Amethyst/devtools/LegsEditor
python main.py
# Should now run without GL_INVALID_VALUE errors
```

**Status:** ✅ **RESOLVED** - OpenGL rendering errors eliminated. Application renders correctly without errors.

### Files Modified
- `gui/viewport.py` - Removed invalid line_width setting, added explanatory comment

### Lessons Learned
- OpenGL core profile has stricter requirements than compatibility profile
- ModernGL defaults to core profile
- Line width > 1.0 requires alternative rendering techniques in modern OpenGL
- Always check OpenGL version and profile requirements when porting graphics code


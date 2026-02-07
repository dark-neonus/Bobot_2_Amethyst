#!/bin/bash
# Setup Display for Bobot Legs Editor
# Provides options to view the GUI from a dev container

set -e

echo "============================================================"
echo "Bobot Legs Editor - Display Setup"
echo "============================================================"
echo ""

# Check if we're in a container
if [ -f /.dockerenv ]; then
    echo "✓ Running in container"
else
    echo "⚠ Not in container - this script is for container environments"
fi

echo ""
echo "Choose display option:"
echo "  1) X11 Forwarding (Linux/Mac hosts only)"
echo "  2) VNC Server (RECOMMENDED for Windows - view via browser)"
echo "  3) Keep Xvfb only (headless, no GUI)"
echo "  4) Exit"
echo ""
echo "💡 Windows users: Choose option 2 (VNC)"
echo ""

read -p "Select option [1-4]: " choice

case $choice in
    1)
        echo ""
        echo "=== X11 Forwarding Setup ==="
        echo ""
        
        # Check if host display is accessible
        if xdpyinfo -display :0 >/dev/null 2>&1; then
            export DISPLAY=:0
            echo "✓ Connected to host display :0"
            echo ""
            echo "To make this permanent, add to your ~/.bashrc:"
            echo "  export DISPLAY=:0"
            echo ""
            read -p "Test with xclock? [y/N]: " test
            if [[ "$test" =~ ^[Yy]$ ]]; then
                xclock &
                sleep 2
                pkill xclock
                echo "✓ X11 forwarding working!"
            fi
        else
            echo "✗ Cannot connect to :0"
            echo ""
            if grep -qi microsoft /proc/version 2>/dev/null; then
                echo "🪟 Detected WSL/Windows host"
                echo ""
                echo "X11 forwarding on Windows requires:"
                echo "  1. Install VcXsrv or Xming on Windows"
                echo "  2. Start X server with 'Disable access control'"
                echo "  3. Set DISPLAY in container"
                echo ""
                echo "💡 EASIER: Use option 2 (VNC) instead!"
            else
                echo "On your HOST machine, run:"
                echo "  xhost +local:docker"
                echo ""
                echo "Then restart this container and try again."
            fi
        fi
        ;;
        
    2)
        echo ""
        echo "=== Installing VNC Server ==="
        echo ""
        
        # Install VNC server and dependencies
        apt-get update
        apt-get install -y \
            x11vnc \
            fluxbox \
            net-tools \
            novnc \
            websockify
        
        echo ""
        echo "✓ VNC installed"
        echo ""
        echo "📝 Manual start commands (if needed):"
        echo "  1. Start Xvfb: Xvfb :99 -screen 0 1920x1080x24 &"
        echo "  2. Start x11vnc: x11vnc -display :99 -forever -nopw &"
        echo "  3. Start noVNC: websockify --web=/usr/share/novnc/ 6080 localhost:5900 &"
        echo ""
        echo "🌐 Then open in browser: http://localhost:6080/vnc.html"
        echo ""
        echo "🪟 Windows users: Make sure port 6080 is forwarded in Docker/DevContainer"
        echo ""
        read -p "Start VNC now? [y/N]: " start_vnc
        
        if [[ "$start_vnc" =~ ^[Yy]$ ]]; then
            # Kill any existing Xvfb/VNC
            pkill -9 Xvfb 2>/dev/null || true
            pkill -9 x11vnc 2>/dev/null || true
            pkill -9 websockify 2>/dev/null || true
            
            sleep 1
            
            # Start services
            Xvfb :99 -screen 0 1920x1080x24 &
            sleep 2
            export DISPLAY=:99
            fluxbox &
            sleep 1
            x11vnc -display :99 -forever -nopw -bg -o /tmp/x11vnc.log
            websockify --web=/usr/share/novnc/ 6080 localhost:5900 &
            
            sleep 2
            echo ""
            echo "✓ VNC Server running!"
            echo ""
            echo "🌐 Open in browser: http://localhost:6080/vnc.html"
            echo ""
            echo "Then in another terminal run:"
            echo "  export DISPLAY=:99"
            echo "  cd /workspaces/Bobot_2_Amethyst/devtools/LegsEditor"
            echo "  python main.py"
            echo ""
            echo "📋 Logs: /tmp/x11vnc.log"
            echo ""
            echo "⚠️  If browser doesn't connect, check port forwarding:"
            echo "   VS Code → Ports tab → Forward port 6080"
        fi
        ;;
        
    3)
        echo ""
        echo "=== Xvfb (Headless) Mode ==="
        echo ""
        echo "The application will run without a visible window."
        echo "This is useful for:"
        echo "  - Automated testing"
        echo "  - CI/CD pipelines"
        echo "  - Generating output files without GUI"
        echo ""
        echo "The app already auto-starts Xvfb when needed."
        echo "Just run: python main.py"
        ;;
        
    4)
        echo "Exiting..."
        exit 0
        ;;
        
    *)
        echo "Invalid option"
        exit 1
        ;;
esac

echo ""
echo "============================================================"
echo "Setup complete!"
echo "============================================================"

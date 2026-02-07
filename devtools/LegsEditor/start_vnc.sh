#!/bin/bash
# Simple VNC starter for Windows users
# Run this, then open http://localhost:6080/vnc.html

set -e

echo "🚀 Starting VNC Server for Bobot Legs Editor..."
echo ""

# Kill any existing servers
echo "Cleaning up old processes..."
pkill -9 Xvfb 2>/dev/null || true
pkill -9 x11vnc 2>/dev/null || true
pkill -9 websockify 2>/dev/null || true
pkill -9 fluxbox 2>/dev/null || true
sleep 1

# Start Xvfb
echo "Starting Xvfb virtual display..."
Xvfb :99 -screen 0 1920x1080x24 > /dev/null 2>&1 &
XVFB_PID=$!
sleep 2

# Verify Xvfb is running
if ! kill -0 $XVFB_PID 2>/dev/null; then
    echo "❌ Failed to start Xvfb!"
    exit 1
fi
echo "✓ Xvfb running (PID: $XVFB_PID)"

# Start x11vnc
echo "Starting x11vnc VNC server..."
x11vnc -display :99 -forever -nopw -listen 0.0.0.0 -rfbport 5900 -bg -o /dev/null

# Wait for VNC to start
sleep 2

# Check if x11vnc is running
if ! pgrep -x x11vnc > /dev/null; then
    echo "❌ x11vnc failed to start!"
    echo "Check if there's a Wayland conflict..."
    pkill Xvfb
    exit 1
fi
echo "✓ x11vnc running"

# Start websockify/noVNC
echo "Starting noVNC web interface..."
websockify --web=/usr/share/novnc/ 6080 localhost:5900 > /dev/null 2>&1 &
WEBSOCK_PID=$!
sleep 2

# Verify websockify
if ! kill -0 $WEBSOCK_PID 2>/dev/null; then
    echo "❌ Failed to start websockify!"
    pkill Xvfb x11vnc
    exit 1
fi
echo "✓ noVNC running (PID: $WEBSOCK_PID)"

echo ""
echo "============================================================"
echo "✅ VNC Server is ready!"
echo "============================================================"
echo ""
echo "🌐 Open in your Windows browser:"
echo "   http://localhost:6080/vnc.html"
echo ""
echo "📝 Then in another terminal, run:"
echo "   export DISPLAY=:99"
echo "   cd /workspaces/Bobot_2_Amethyst/devtools/LegsEditor"
echo "   python main.py"
echo ""
echo "🛑 To stop VNC server:"
echo "   pkill -9 Xvfb x11vnc websockify"
echo ""
echo "💡 Make sure port 6080 is forwarded in VS Code (Ports tab)"
echo "============================================================"

#!/bin/bash
# VNC Server using TigerVNC (works better in containers)
# Run this, then open http://localhost:6080/vnc.html

set -e

echo "🚀 Starting VNC Server (TigerVNC) for Bobot Legs Editor..."
echo ""

# Kill existing
echo "Cleaning up old processes..."
pkill -9 Xvfb Xtigervnc x11vnc websockify fluxbox 2>/dev/null || true
sleep 1

# Start TigerVNC with built-in Xvfb
echo "Starting TigerVNC server..."
Xtigervnc :99 \
  -geometry 1920x1080 \
  -depth 24 \
  -SecurityTypes None \
  -rfbport 5900 \
  -localhost no \
  > /dev/null 2>&1 &

VNC_PID=$!
sleep 3

# Check if TigerVNC started
if ! kill -0 $VNC_PID 2>/dev/null; then
    echo "❌ TigerVNC failed to start!"
    exit 1
fi
echo "✓ TigerVNC running (PID: $VNC_PID)"

# Start websockify
echo "Starting noVNC web interface..."
websockify --web=/usr/share/novnc/ 6080 localhost:5900 > /dev/null 2>&1 &
WEBSOCK_PID=$!
sleep 2

if ! kill -0 $WEBSOCK_PID 2>/dev/null; then
    echo "❌ websockify failed!"
    kill $VNC_PID
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
echo "   Click 'Connect' (no password needed)"
echo ""
echo "📝 Then in another terminal, run:"
echo "   export DISPLAY=:99"
echo "   cd /workspaces/Bobot_2_Amethyst/devtools/LegsEditor"
echo "   python main.py"
echo ""
echo "🛑 To stop VNC:"
echo "   pkill -9 Xtigervnc websockify"
echo ""
echo "💡 VS Code: Check 'Ports' tab - port 6080 should be forwarded"
echo "============================================================"

# Keep script running to show status
echo ""
echo "Press Ctrl+C to stop VNC server..."
wait

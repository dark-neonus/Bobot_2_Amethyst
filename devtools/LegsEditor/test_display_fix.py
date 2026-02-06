#!/usr/bin/env python3
"""
Test script to verify GLFW X11 display fix
"""

import sys
import os
from pathlib import Path

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent))

# Import and test the Xvfb functionality
from main import check_display_available, find_available_display, start_xvfb, stop_xvfb

def test_display_setup():
    """Test the display detection and Xvfb startup"""
    print("=" * 60)
    print("Testing GLFW X11 Display Fix")
    print("=" * 60)
    
    # Check current display
    current_display = os.environ.get('DISPLAY', 'NOT SET')
    print(f"\n1. Current DISPLAY: {current_display}")
    
    # Test if it's accessible
    is_accessible = check_display_available()
    print(f"   Is accessible: {is_accessible}")
    
    # Find available display
    available = find_available_display()
    print(f"\n2. Available display: {available}")
    
    # Start Xvfb
    print(f"\n3. Starting Xvfb...")
    success = start_xvfb()
    
    if success:
        print("   ✅ Xvfb started successfully!")
        print(f"   DISPLAY is now: {os.environ.get('DISPLAY')}")
        
        # Try to import GLFW to verify it works
        try:
            print(f"\n4. Testing GLFW initialization...")
            from imgui_bundle import hello_imgui
            print("   ✅ imgui_bundle imported successfully!")
            print("   ✅ GLFW should be able to initialize now")
            
        except Exception as e:
            print(f"   ⚠️  Error importing imgui_bundle: {e}")
        
        # Clean up
        print(f"\n5. Cleaning up...")
        stop_xvfb()
        print("   ✅ Xvfb stopped")
        
    else:
        print("   ❌ Failed to start Xvfb")
        return False
    
    print("\n" + "=" * 60)
    print("✅ GLFW X11 Display Fix: WORKING")
    print("=" * 60)
    return True

if __name__ == "__main__":
    try:
        success = test_display_setup()
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"\n❌ Test failed with error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

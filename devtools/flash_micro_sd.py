#!/usr/bin/env python3
"""
Bobot SD Card Asset Uploader

This script uploads assets to the Bobot's SD card via WiFi.
It discovers the device using mDNS or connects to a manually specified IP address.
"""

import argparse
import os
import sys
import time
import json
import socket
import requests
from pathlib import Path
from typing import Optional, List, Tuple
from urllib.parse import urlencode

# Try to import zeroconf for mDNS discovery
try:
    from zeroconf import Zeroconf, ServiceBrowser, ServiceListener
    ZEROCONF_AVAILABLE = True
    
    class BobotDiscoveryListener(ServiceListener):
        """Service listener for mDNS discovery"""
        
        def __init__(self):
            self.found_ip: Optional[str] = None
            self.found = False
        
        def add_service(self, zc: Zeroconf, type_: str, name: str) -> None:
            """Called when a service is discovered"""
            info = zc.get_service_info(type_, name)
            if info and "bobot" in name.lower():
                addresses = [socket.inet_ntoa(addr) for addr in info.addresses]
                if addresses:
                    self.found_ip = addresses[0]
                    self.found = True
                    print(f"✓ Found Bobot at {self.found_ip}:{info.port}")
        
        def remove_service(self, zc: Zeroconf, type_: str, name: str) -> None:
            pass
        
        def update_service(self, zc: Zeroconf, type_: str, name: str) -> None:
            pass
            
except ImportError:
    ZEROCONF_AVAILABLE = False
    print("Warning: zeroconf not available. Install with: pip install zeroconf")
    print("You can still use --ip to specify IP address manually.\n")


def discover_bobot(timeout: int = 10) -> Optional[str]:
    """
    Discover Bobot device using mDNS
    
    Args:
        timeout: Discovery timeout in seconds
        
    Returns:
        IP address if found, None otherwise
    """
    if not ZEROCONF_AVAILABLE:
        print("✗ mDNS discovery not available")
        print("Using default AP IP: 192.168.4.1")
        return "192.168.4.1"
    
    print(f"Searching for Bobot on network (timeout: {timeout}s)...")
    
    try:
        zeroconf = Zeroconf()
        listener = BobotDiscoveryListener()
        browser = ServiceBrowser(zeroconf, "_http._tcp.local.", listener)
        
        # Wait for discovery
        start_time = time.time()
        while not listener.found and (time.time() - start_time) < timeout:
            time.sleep(0.5)
        
        zeroconf.close()
        
        # Fall back to default AP IP if not found
        if not listener.found_ip:
            print("Not found via mDNS, trying default AP IP: 192.168.4.1")
            return "192.168.4.1"
        
        return listener.found_ip
    except Exception as e:
        print(f"✗ Discovery error: {e}")
        print("Using default AP IP: 192.168.4.1")
        return "192.168.4.1"


def get_file_list(assets_dir: Path, exclude_patterns: List[str] = None) -> List[Tuple[Path, str]]:
    """
    Get list of files to upload
    
    Args:
        assets_dir: Assets directory path
        exclude_patterns: List of file patterns to exclude
        
    Returns:
        List of (file_path, relative_path) tuples
    """
    if exclude_patterns is None:
        exclude_patterns = ['.aseprite', '.ase', '.git', '__pycache__', '.DS_Store']
    
    files = []
    
    for root, dirs, filenames in os.walk(assets_dir):
        # Filter out excluded directories
        dirs[:] = [d for d in dirs if not any(pattern in d for pattern in exclude_patterns)]
        
        for filename in filenames:
            # Skip excluded files
            if any(pattern in filename for pattern in exclude_patterns):
                continue
            
            file_path = Path(root) / filename
            relative_path = file_path.relative_to(assets_dir)
            files.append((file_path, str(relative_path).replace('\\', '/')))
    
    return files


def upload_file(base_url: str, file_path: Path, relative_path: str) -> bool:
    """
    Upload a single file
    
    Args:
        base_url: Base URL of the server
        file_path: Path to the file to upload
        relative_path: Relative path on the SD card
        
    Returns:
        True if successful, False otherwise
    """
    try:
        with open(file_path, 'rb') as f:
            data = f.read()
        
        params = {'path': relative_path}
        url = f"{base_url}/file?{urlencode(params)}"
        
        response = requests.post(url, data=data, timeout=30)
        
        if response.status_code == 200:
            result = response.json()
            if result.get('status') == 'ok':
                return True
            else:
                print(f"✗ Server error: {result.get('message', 'Unknown error')}")
                return False
        else:
            print(f"✗ HTTP error {response.status_code}")
            return False
            
    except Exception as e:
        print(f"✗ Upload failed: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Upload assets to Bobot SD card via WiFi",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                          # Auto-discover and upload
  %(prog)s --ip 192.168.1.100       # Upload to specific IP
  %(prog)s --assets custom/path     # Upload from custom directory
        """
    )
    
    parser.add_argument(
        '--ip',
        type=str,
        help='IP address of Bobot (skips mDNS discovery)'
    )
    
    parser.add_argument(
        '--port',
        type=int,
        default=8080,
        help='HTTP server port (default: 8080)'
    )
    
    parser.add_argument(
        '--assets',
        type=str,
        default='../assets',
        help='Path to assets directory (default: ../assets)'
    )
    
    parser.add_argument(
        '--timeout',
        type=int,
        default=10,
        help='Discovery timeout in seconds (default: 10)'
    )
    
    args = parser.parse_args()
    
    # Get script directory
    script_dir = Path(__file__).parent
    
    # Resolve assets directory
    assets_dir = Path(args.assets)
    if not assets_dir.is_absolute():
        assets_dir = (script_dir / assets_dir).resolve()
    
    if not assets_dir.exists():
        print(f"✗ Assets directory not found: {assets_dir}")
        return 1
    
    print("=" * 60)
    print("Bobot SD Card Asset Uploader")
    print("=" * 60)
    print(f"Assets directory: {assets_dir}\n")
    
    # Get IP address
    if args.ip:
        ip = args.ip
        print(f"Using specified IP: {ip}")
    else:
        ip = discover_bobot(args.timeout)
        if not ip:
            print("\n✗ Could not find Bobot on network")
            print("Please ensure:")
            print("  1. Bobot is powered on")
            print("  2. Upload mode is activated (hold Back+Settings+Debug for 2s)")
            print("  3. You're connected to 'Bobot_Upload' WiFi network")
            print("  4. Default IP should be 192.168.4.1")
            print("\nSpecify IP manually with --ip 192.168.4.1")
            return 1
    
    base_url = f"http://{ip}:{args.port}"
    
    # Test connection
    print(f"\nConnecting to {base_url}...")
    try:
        response = requests.get(f"{base_url}/", timeout=5)
    except Exception as e:
        print(f"✗ Connection failed: {e}")
        print("Please ensure upload mode is active on Bobot")
        return 1
    
    # Get file list
    print("\nScanning assets directory...")
    files = get_file_list(assets_dir)
    
    if not files:
        print("✗ No files found to upload")
        return 1
    
    print(f"✓ Found {len(files)} files to upload")
    
    # Calculate total size
    total_size = sum(f[0].stat().st_size for f in files)
    print(f"  Total size: {total_size / 1024:.1f} KB")
    
    # Confirm upload
    print("\nReady to upload. Press Enter to continue or Ctrl+C to cancel...")
    try:
        input()
    except KeyboardInterrupt:
        print("\n\nUpload cancelled")
        return 0
    
    # Start upload
    print("\n" + "=" * 60)
    print("Starting upload...")
    print("=" * 60)
    
    try:
        # Send start command
        print("\n[1/3] Initializing upload...")
        response = requests.post(f"{base_url}/start", timeout=10)
        result = response.json()
        
        if result.get('status') != 'ok':
            print(f"✗ Failed to start upload: {result.get('message')}")
            return 1
        
        print("✓ Ready to receive files")
        
        # Upload files
        print(f"\n[2/3] Uploading {len(files)} files...")
        uploaded_size = 0
        failed_files = []
        
        for idx, (file_path, relative_path) in enumerate(files, 1):
            file_size = file_path.stat().st_size
            progress = (uploaded_size / total_size) * 100
            
            print(f"\n[{idx}/{len(files)}] {relative_path}")
            print(f"  Progress: {progress:.1f}% | Size: {file_size / 1024:.1f} KB")
            
            if upload_file(base_url, file_path, relative_path):
                print(f"  ✓ Uploaded successfully")
                uploaded_size += file_size
            else:
                print(f"  ✗ Upload failed")
                failed_files.append(relative_path)
        
        # Complete upload
        print("\n[3/3] Finalizing upload...")
        response = requests.post(f"{base_url}/complete", timeout=10)
        result = response.json()
        
        if result.get('status') == 'ok':
            print("✓ Upload complete")
        else:
            print(f"⚠ Warning: {result.get('message')}")
        
        # Summary
        print("\n" + "=" * 60)
        print("Upload Summary")
        print("=" * 60)
        print(f"Total files: {len(files)}")
        print(f"Uploaded: {len(files) - len(failed_files)}")
        print(f"Failed: {len(failed_files)}")
        print(f"Total size: {uploaded_size / 1024:.1f} KB")
        
        if failed_files:
            print("\nFailed files:")
            for f in failed_files:
                print(f"  - {f}")
            return 1
        else:
            print("\n✓ All files uploaded successfully!")
            print("\nYou can now exit upload mode on Bobot")
            return 0
            
    except KeyboardInterrupt:
        print("\n\n✗ Upload interrupted by user")
        return 1
    except Exception as e:
        print(f"\n✗ Upload failed: {e}")
        return 1


if __name__ == '__main__':
    sys.exit(main())

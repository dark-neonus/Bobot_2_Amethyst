#!/usr/bin/env python3
"""
Graphics Structure Generation Script for Bobot 2 Amethyst

This script processes the graphics asset structure by:
1. Reading the Description.ini to determine which libraries are enabled
2. Creating library directories if they don't exist
3. Generating Description.ini files for expressions
4. Exporting Aseprite animations to PNG frame sequences
5. Converting PNG frames to u8g2-compatible binary format
6. Cleaning up intermediate files
"""

import os
import sys
import configparser
import subprocess
import struct
import shutil
from pathlib import Path
from typing import List, Dict, Tuple

try:
    from PIL import Image
except ImportError:
    print("Warning: Pillow (PIL) not found. Installing...")
    subprocess.check_call([sys.executable, "-m", "pip", "install", "Pillow"])
    from PIL import Image


class GraphicsStructureGenerator:
    """Main class for generating graphics structure."""
    
    def __init__(self, graphics_dir: Path):
        """
        Initialize the generator.
        
        Args:
            graphics_dir: Path to the assets/graphics directory
        """
        self.graphics_dir = graphics_dir
        self.description_file = graphics_dir / "Description.ini"
        self.enabled_libraries: List[str] = []
        
    def read_libraries_config(self) -> None:
        """Read Description.ini and populate enabled libraries list."""
        if not self.description_file.exists():
            print(f"Error: {self.description_file} not found!")
            sys.exit(1)
            
        # Preserve case sensitivity for library names
        config = configparser.RawConfigParser()
        config.optionxform = str  # Preserve case
        config.read(self.description_file)
        
        if 'Libraries' not in config:
            print("Error: [Libraries] section not found in Description.ini!")
            sys.exit(1)
            
        for library_name, enabled in config['Libraries'].items():
            if enabled.lower() == 'true':
                self.enabled_libraries.append(library_name)
                print(f"✓ Library '{library_name}' is enabled")
            else:
                print(f"✗ Library '{library_name}' is disabled")
    
    def create_library_directories(self) -> None:
        """Create directories for enabled libraries if they don't exist."""
        # Ensure libraries directory exists
        libraries_root = self.graphics_dir / "libraries"
        libraries_root.mkdir(exist_ok=True)
        
        for library_name in self.enabled_libraries:
            library_path = libraries_root / library_name
            if not library_path.exists():
                library_path.mkdir(parents=True, exist_ok=True)
                print(f"Created library directory: {library_path}")
            else:
                print(f"Library directory already exists: {library_path}")
    
    def generate_expression_description(self, expression_dir: Path, expression_name: str) -> None:
        """
        Generate Description.ini for an expression if it doesn't exist.
        
        Args:
            expression_dir: Path to the expression directory
            expression_name: Name of the expression
        """
        desc_file = expression_dir / "Description.ini"
        
        if desc_file.exists():
            print(f"  Description.ini already exists for '{expression_name}'")
            return
            
        # Generate default Description.ini content
        content = f"""; Description for {expression_name} expression

[Loop]
; Supported Loop types:
;
; 1. IdleBlink(default) - Display first frame for random time in range
;                           [IdleTimeMinMS, IdleTimeMaxMS] ms and after that
;                           run animation with AnimationFPS fps.
;                           After that switch frame to first and repeat.
;
; 2. Loop - Repeat animation from first frame to last with AnimationFPS fps.
;           IdleTimeMinMS and IdleTimeMaxMS are unnecessary and will be ignored
;           if presented
;
; 3. Image - Display only first frame. All fields except type are unnecessary 
;             and will be ignored if presented

; Type field must be first
Type = IdleBlink
AnimationFPS = 20
IdleTimeMinMS = 1000
IdleTimeMaxMS = 3000
"""
        
        with open(desc_file, 'w') as f:
            f.write(content)
        print(f"  Generated Description.ini for '{expression_name}'")
    
    def find_aseprite_executable(self) -> str:
        """
        Find the aseprite executable.
        
        Returns:
            Path to aseprite executable
        """
        # Try common locations
        possible_paths = [
            'aseprite',  # In PATH
            '/usr/bin/aseprite',
            '/usr/local/bin/aseprite',
            'C:\\Program Files\\Aseprite\\Aseprite.exe',
            'C:\\Program Files (x86)\\Aseprite\\Aseprite.exe',
        ]
        
        for path in possible_paths:
            try:
                result = subprocess.run(
                    [path, '--version'],
                    capture_output=True,
                    timeout=5
                )
                if result.returncode == 0:
                    return path
            except (subprocess.SubprocessError, FileNotFoundError):
                continue
        
        print("Warning: Aseprite executable not found. Frame export will be skipped.")
        return None
    
    def export_aseprite_frames(self, aseprite_file: Path, frames_dir: Path, aseprite_cmd: str) -> List[Path]:
        """
        Export Aseprite animation to PNG frame sequence.
        
        Args:
            aseprite_file: Path to the .aseprite file
            frames_dir: Directory to save frames
            aseprite_cmd: Path to aseprite executable
            
        Returns:
            List of exported PNG file paths
        """
        if aseprite_cmd is None:
            return []
            
        # Create temporary directory for PNG export
        temp_png_dir = frames_dir / "_temp_png"
        temp_png_dir.mkdir(exist_ok=True)
        
        # Export frames as PNG sequence
        # Frame naming: Frame_00.png, Frame_01.png, etc.
        output_pattern = temp_png_dir / "Frame_{frame2}.png"
        
        try:
            cmd = [
                aseprite_cmd,
                '-b',  # Batch mode
                str(aseprite_file),
                '--save-as',
                str(output_pattern)
            ]
            
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=30
            )
            
            if result.returncode == 0:
                # Get list of exported PNG files and rename them to start from 00
                png_files = sorted(temp_png_dir.glob("Frame_*.png"))
                
                # Rename files to ensure sequential numbering starting from 00
                renamed_files = []
                for idx, old_file in enumerate(png_files):
                    new_name = temp_png_dir / f"Frame_{idx:02d}.png"
                    if old_file != new_name:
                        old_file.rename(new_name)
                    renamed_files.append(new_name)
                
                print(f"    Exported {len(renamed_files)} PNG frames from {aseprite_file.name}")
                return renamed_files
            else:
                print(f"    Error exporting frames: {result.stderr}")
                return []
                
        except subprocess.TimeoutExpired:
            print(f"    Timeout while exporting {aseprite_file.name}")
            return []
        except Exception as e:
            print(f"    Error: {e}")
            return []
    
    def convert_png_to_u8g2_format(self, png_file: Path, output_file: Path) -> bool:
        """
        Convert PNG to u8g2-compatible monochrome binary format.
        
        Format:
        - 2 bytes: width (little-endian uint16)
        - 2 bytes: height (little-endian uint16)
        - N bytes: monochrome bitmap data (1 bit per pixel, packed)
        
        Args:
            png_file: Input PNG file path
            output_file: Output binary file path
            
        Returns:
            True if successful, False otherwise
        """
        try:
            # Open and convert to monochrome
            img = Image.open(png_file)
            
            # Convert to grayscale first, then to 1-bit monochrome
            img = img.convert('L')  # Grayscale
            img = img.convert('1')  # 1-bit monochrome (black & white)
            
            width, height = img.size
            
            # Create binary data
            # u8g2 expects data in column-major order with vertical bytes
            # Each byte represents 8 vertical pixels
            bitmap_data = bytearray()
            
            # Process in vertical strips (u8g2 format)
            for x in range(width):
                for y_byte in range((height + 7) // 8):  # Round up to nearest byte
                    byte_val = 0
                    for bit in range(8):
                        y = y_byte * 8 + bit
                        if y < height:
                            pixel = img.getpixel((x, y))
                            # In PIL '1' mode: 0 = black, 255 = white
                            # For u8g2: 1 = pixel on, 0 = pixel off
                            if pixel == 0:  # Black pixel
                                byte_val |= (1 << bit)
                    bitmap_data.append(byte_val)
            
            # Write header + bitmap data
            with open(output_file, 'wb') as f:
                # Write width and height as little-endian uint16
                f.write(struct.pack('<HH', width, height))
                # Write bitmap data
                f.write(bitmap_data)
            
            return True
            
        except Exception as e:
            print(f"      Error converting {png_file.name}: {e}")
            return False
    
    def process_and_convert_frames(self, png_files: List[Path], frames_dir: Path) -> int:
        """
        Convert PNG frames to u8g2 binary format and clean up.
        
        Args:
            png_files: List of PNG file paths to convert
            frames_dir: Directory to save final binary frames
            
        Returns:
            Number of successfully converted frames
        """
        frames_dir.mkdir(exist_ok=True)
        converted_count = 0
        
        for png_file in png_files:
            # Determine output filename (replace .png with .bin)
            frame_name = png_file.stem  # e.g., "Frame_00"
            output_file = frames_dir / f"{frame_name}.bin"
            
            if self.convert_png_to_u8g2_format(png_file, output_file):
                converted_count += 1
        
        # Clean up temporary PNG files
        if png_files:
            temp_dir = png_files[0].parent
            for png_file in png_files:
                try:
                    png_file.unlink()
                except Exception as e:
                    print(f"      Warning: Could not delete {png_file.name}: {e}")
            
            # Remove temporary directory
            try:
                temp_dir.rmdir()
            except Exception:
                pass  # Directory might not be empty or already removed
        
        return converted_count
    
    def process_library(self, library_name: str, aseprite_cmd: str) -> None:
        """
        Process a single library: generate descriptions and export frames.
        
        Args:
            library_name: Name of the library to process
            aseprite_cmd: Path to aseprite executable (or None)
        """
        library_path = self.graphics_dir / "libraries" / library_name
        
        if not library_path.exists():
            print(f"  Library directory not found: {library_name}")
            return
        
        # Iterate over all directories in the library (each is an expression)
        for item in library_path.iterdir():
            if not item.is_dir():
                continue
                
            expression_name = item.name
            print(f"  Processing expression: {expression_name}")
            
            # Generate Description.ini if needed
            self.generate_expression_description(item, expression_name)
            
            # Look for .aseprite files
            aseprite_files = list(item.glob("*.aseprite")) + list(item.glob("*.ase"))
            
            if not aseprite_files:
                print(f"    No .aseprite file found, skipping frame export")
                continue
            
            # Clear existing Frames directory to remove old/stale frames
            frames_dir = item / "Frames"
            if frames_dir.exists():
                print(f"    Clearing old frames...")
                shutil.rmtree(frames_dir)
            frames_dir.mkdir(exist_ok=True)
            
            # Process each .aseprite file
            for aseprite_file in aseprite_files:
                print(f"    Exporting frames from {aseprite_file.name}...")
                
                # Export to PNG
                png_files = self.export_aseprite_frames(aseprite_file, frames_dir, aseprite_cmd)
                
                if png_files:
                    # Convert PNG to u8g2 binary format
                    print(f"    Converting {len(png_files)} frames to u8g2 binary format...")
                    converted_count = self.process_and_convert_frames(png_files, frames_dir)
                    print(f"    ✓ Converted {converted_count} frames to binary format")
                else:
                    print(f"    No frames exported from {aseprite_file.name}")
    
    def run(self) -> None:
        """Main execution method."""
        print("=" * 60)
        print("Graphics Structure Generation Script")
        print("=" * 60)
        print()
        
        # Read configuration
        print("Reading library configuration...")
        self.read_libraries_config()
        print()
        
        # Create library directories
        print("Creating library directories...")
        self.create_library_directories()
        print()
        
        # Find Aseprite
        print("Looking for Aseprite executable...")
        aseprite_cmd = self.find_aseprite_executable()
        if aseprite_cmd:
            print(f"Found Aseprite at: {aseprite_cmd}")
        print()
        
        # Process each enabled library
        print("Processing libraries...")
        for library_name in self.enabled_libraries:
            print(f"\nProcessing library: {library_name}")
            print("-" * 60)
            self.process_library(library_name, aseprite_cmd)
        
        print()
        print("=" * 60)
        print("Graphics structure generation complete!")
        print("=" * 60)


def main():
    """Main entry point."""
    # Determine graphics directory
    # Script can be run from anywhere, but we need to find assets/graphics
    
    # Try to find project root (look for CMakeLists.txt)
    current_dir = Path.cwd()
    project_root = None
    
    # Search upwards for project root
    for parent in [current_dir] + list(current_dir.parents):
        if (parent / "CMakeLists.txt").exists():
            project_root = parent
            break
    
    if project_root is None:
        # Try current directory
        project_root = current_dir
    
    graphics_dir = project_root / "assets" / "graphics"
    
    if not graphics_dir.exists():
        print(f"Error: Graphics directory not found at {graphics_dir}")
        print("Please run this script from the project root or ensure assets/graphics exists.")
        sys.exit(1)
    
    print(f"Using graphics directory: {graphics_dir}")
    print()
    
    # Run the generator
    generator = GraphicsStructureGenerator(graphics_dir)
    generator.run()


if __name__ == "__main__":
    main()

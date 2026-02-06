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

[Dimensions]
; Frame dimensions in pixels (filled automatically during export)
Width = 0
Height = 0
"""
        
        with open(desc_file, 'w') as f:
            f.write(content)
        print(f"  Generated Description.ini for '{expression_name}'")
    
    def update_description_dimensions(self, expression_dir: Path, width: int, height: int, fps: float = None) -> None:
        """
        Update Description.ini with frame dimensions and FPS.
        
        Args:
            expression_dir: Path to the expression directory
            width: Frame width in pixels
            height: Frame height in pixels
            fps: Animation FPS (optional, will not update if None)
        """
        desc_file = expression_dir / "Description.ini"
        
        if not desc_file.exists():
            print(f"    Warning: Description.ini not found, cannot update dimensions")
            return
        
        # Read existing content
        config = configparser.ConfigParser()
        config.optionxform = str  # Preserve case
        config.read(desc_file)
        
        # Ensure [Dimensions] section exists
        if 'Dimensions' not in config:
            config.add_section('Dimensions')
        
        # Update dimensions
        config['Dimensions']['Width'] = str(width)
        config['Dimensions']['Height'] = str(height)
        
        # Update FPS if provided
        if fps is not None:
            if 'Loop' in config:
                config['Loop']['AnimationFPS'] = str(int(fps)) if fps == int(fps) else f"{fps:.1f}"
        
        # Write back to file
        with open(desc_file, 'w') as f:
            config.write(f, space_around_delimiters=True)
        
        fps_info = f", FPS={fps:.1f}" if fps is not None else ""
        print(f"    Updated Description.ini with dimensions: {width}x{height}{fps_info}")
    
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
    
    def get_aseprite_fps(self, aseprite_file: Path, aseprite_cmd: str) -> float:
        """
        Extract FPS from Aseprite file by reading first frame duration.
        
        Args:
            aseprite_file: Path to the .aseprite file
            aseprite_cmd: Path to aseprite executable
            
        Returns:
            FPS value (default 20.0 if cannot extract)
        """
        if aseprite_cmd is None:
            return 20.0
        
        try:
            # Create temporary JSON file for metadata
            import tempfile
            with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as tmp:
                json_path = tmp.name
            
            # Export metadata to JSON
            cmd = [
                aseprite_cmd,
                '-b',
                str(aseprite_file),
                '--list-tags',
                '--data', json_path,
                '--format', 'json-array'
            ]
            
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=10
            )
            
            if result.returncode == 0:
                # Read JSON metadata
                import json
                with open(json_path, 'r') as f:
                    data = json.load(f)
                
                # Extract duration from first frame (in milliseconds)
                if 'frames' in data and len(data['frames']) > 0:
                    first_frame = data['frames'][0]
                    duration_ms = first_frame.get('duration', 50)  # Default 50ms if not found
                    
                    # Convert duration to FPS
                    fps = 1000.0 / duration_ms
                    
                    # Clean up temp file
                    Path(json_path).unlink()
                    
                    print(f"    Detected FPS: {fps:.1f} (frame duration: {duration_ms}ms)")
                    return fps
            
            # Clean up temp file if it exists
            if Path(json_path).exists():
                Path(json_path).unlink()
                
        except Exception as e:
            print(f"    Warning: Could not extract FPS from Aseprite file: {e}")
        
        # Return default FPS
        return 20.0
    
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
        # Frame naming: Frame_0.png, Frame_1.png, etc.
        # Note: Use {frame} (0-indexed) not {frame2} to get correct frame order
        output_pattern = temp_png_dir / "Frame_{frame}.png"
        
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
                # Get list of exported PNG files and sort them numerically by frame number
                png_files = list(temp_png_dir.glob("Frame_*.png"))
                
                # Sort numerically by extracting the frame number from filename
                # Frame_0.png -> 0, Frame_10.png -> 10, etc.
                def extract_frame_number(filepath):
                    # Extract number from "Frame_N.png"
                    stem = filepath.stem  # "Frame_N"
                    num_str = stem.split('_')[1]  # "N"
                    return int(num_str)
                
                png_files.sort(key=extract_frame_number)
                
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
    
    def convert_png_to_u8g2_format(self, png_file: Path, output_file: Path) -> Tuple[bool, int, int]:
        """
        Convert PNG to u8g2-compatible monochrome binary format.
        
        Format (optimized):
        - N bytes: monochrome bitmap data (1 bit per pixel, packed)
        
        Dimensions are stored in Description.ini instead of each frame file.
        This reduces file size and speeds up loading since dimensions
        are read once and reused for all frames.
        
        Args:
            png_file: Input PNG file path
            output_file: Output binary file path
            
        Returns:
            Tuple of (success, width, height)
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
                            # Invert: white pixels in source become pixels on display
                            if pixel != 0:  # White pixel (inverted)
                                byte_val |= (1 << bit)
                    bitmap_data.append(byte_val)
            
            # Write bitmap data only (no dimensions)
            with open(output_file, 'wb') as f:
                f.write(bitmap_data)
            
            return True, width, height
            
        except Exception as e:
            print(f"      Error converting {png_file.name}: {e}")
            return False, 0, 0
    
    def process_and_convert_frames(self, png_files: List[Path], frames_dir: Path) -> Tuple[int, int, int]:
        """
        Convert PNG frames to u8g2 binary format and clean up.
        
        Args:
            png_files: List of PNG file paths to convert
            frames_dir: Directory to save final binary frames
            
        Returns:
            Tuple of (converted_count, width, height)
        """
        frames_dir.mkdir(exist_ok=True)
        converted_count = 0
        frame_width = 0
        frame_height = 0
        
        for png_file in png_files:
            # Determine output filename (replace .png with .bin)
            frame_name = png_file.stem  # e.g., "Frame_00"
            output_file = frames_dir / f"{frame_name}.bin"
            
            success, width, height = self.convert_png_to_u8g2_format(png_file, output_file)
            if success:
                converted_count += 1
                # Store dimensions from first frame (all frames should be same size)
                if converted_count == 1:
                    frame_width = width
                    frame_height = height
        
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
        
        return converted_count, frame_width, frame_height
    
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
                
                # Extract FPS from Aseprite file
                fps = self.get_aseprite_fps(aseprite_file, aseprite_cmd)
                
                # Export to PNG
                png_files = self.export_aseprite_frames(aseprite_file, frames_dir, aseprite_cmd)
                
                if png_files:
                    # Convert PNG to u8g2 binary format
                    print(f"    Converting {len(png_files)} frames to u8g2 binary format...")
                    converted_count, width, height = self.process_and_convert_frames(png_files, frames_dir)
                    print(f"    ✓ Converted {converted_count} frames ({width}x{height} pixels)")
                    
                    # Update Description.ini with dimensions and FPS
                    if converted_count > 0:
                        self.update_description_dimensions(item, width, height, fps)
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

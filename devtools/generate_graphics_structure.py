#!/usr/bin/env python3
"""
Graphics Structure Generation Script for Bobot 2 Amethyst

This script processes the graphics asset structure by:
1. Reading the Description.ini to determine which libraries are enabled
2. Creating library directories if they don't exist
3. Generating Description.ini files for expressions
4. Exporting Aseprite animations to frame sequences
"""

import os
import sys
import configparser
import subprocess
from pathlib import Path
from typing import List, Dict


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
            
        config = configparser.ConfigParser()
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
        for library_name in self.enabled_libraries:
            library_path = self.graphics_dir / library_name
            if not library_path.exists():
                library_path.mkdir(parents=True, exist_ok=True)
                print(f"Created library directory: {library_path}")
                
                # Create Expressions subdirectory
                expressions_dir = library_path / "Expressions"
                expressions_dir.mkdir(exist_ok=True)
                print(f"Created expressions directory: {expressions_dir}")
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
    
    def export_aseprite_frames(self, aseprite_file: Path, frames_dir: Path, aseprite_cmd: str) -> None:
        """
        Export Aseprite animation to frame sequence.
        
        Args:
            aseprite_file: Path to the .aseprite file
            frames_dir: Directory to save frames
            aseprite_cmd: Path to aseprite executable
        """
        if aseprite_cmd is None:
            return
            
        # Create frames directory if it doesn't exist
        frames_dir.mkdir(exist_ok=True)
        
        # Export frames as PNG sequence
        # Frame naming: Frame_00.png, Frame_01.png, etc.
        output_pattern = frames_dir / "Frame_{frame2}.png"
        
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
                # Count exported frames
                frame_files = list(frames_dir.glob("Frame_*.png"))
                print(f"    Exported {len(frame_files)} frames from {aseprite_file.name}")
            else:
                print(f"    Error exporting frames: {result.stderr}")
                
        except subprocess.TimeoutExpired:
            print(f"    Timeout while exporting {aseprite_file.name}")
        except Exception as e:
            print(f"    Error: {e}")
    
    def process_library(self, library_name: str, aseprite_cmd: str) -> None:
        """
        Process a single library: generate descriptions and export frames.
        
        Args:
            library_name: Name of the library to process
            aseprite_cmd: Path to aseprite executable (or None)
        """
        library_path = self.graphics_dir / library_name
        expressions_path = library_path / "Expressions"
        
        if not expressions_path.exists():
            print(f"  No Expressions directory found in {library_name}")
            return
        
        # Iterate over all directories in Expressions
        for item in expressions_path.iterdir():
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
            
            # Process each .aseprite file
            for aseprite_file in aseprite_files:
                frames_dir = item / "Frames"
                print(f"    Exporting frames from {aseprite_file.name}...")
                self.export_aseprite_frames(aseprite_file, frames_dir, aseprite_cmd)
    
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

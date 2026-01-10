#!/usr/bin/env bash
################################################################################
# Graphics Structure Generation Wrapper Script
# 
# This script provides a convenient wrapper for the Python graphics generation
# tool. It handles:
# - Python environment setup
# - Navigation to the correct directory
# - Execution of the main Python script
################################################################################

set -e  # Exit on error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
GRAPHICS_DIR="$PROJECT_ROOT/assets/graphics"
PYTHON_SCRIPT="$SCRIPT_DIR/generate_graphics_structure.py"

echo -e "${GREEN}================================${NC}"
echo -e "${GREEN}Bobot Graphics Generation Tool${NC}"
echo -e "${GREEN}================================${NC}"
echo ""

# Check if Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}Error: python3 is not installed or not in PATH${NC}"
    echo "Please install Python 3 to use this tool."
    exit 1
fi

PYTHON_VERSION=$(python3 --version)
echo -e "${GREEN}✓${NC} Found $PYTHON_VERSION"

# Check if the Python script exists
if [ ! -f "$PYTHON_SCRIPT" ]; then
    echo -e "${RED}Error: Python script not found at $PYTHON_SCRIPT${NC}"
    exit 1
fi

echo -e "${GREEN}✓${NC} Found Python script"

# Check if graphics directory exists
if [ ! -d "$GRAPHICS_DIR" ]; then
    echo -e "${YELLOW}Warning: Graphics directory not found at $GRAPHICS_DIR${NC}"
    echo "Creating directory..."
    mkdir -p "$GRAPHICS_DIR"
fi

echo -e "${GREEN}✓${NC} Graphics directory: $GRAPHICS_DIR"
echo ""

# Navigate to graphics directory for execution
cd "$GRAPHICS_DIR" || {
    echo -e "${RED}Error: Could not change to graphics directory${NC}"
    exit 1
}

# Execute the Python script
echo "Executing graphics generation script..."
echo ""

python3 "$PYTHON_SCRIPT" "$@"

# Return to original directory
cd - > /dev/null

echo ""
echo -e "${GREEN}Done!${NC}"

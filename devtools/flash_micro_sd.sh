#!/bin/bash
# Bobot SD Card Asset Uploader - Shell Wrapper
# This script wraps the Python uploader with convenient interface

set -e

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "======================================"
echo "Bobot SD Card Asset Uploader"
echo "======================================"
echo ""

# Check if Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}Error: python3 not found${NC}"
    echo "Please install Python 3 to use this tool"
    exit 1
fi

# Check if required Python packages are installed
echo "Checking dependencies..."

# Check for requests
if ! python3 -c "import requests" 2>/dev/null; then
    echo -e "${YELLOW}Warning: 'requests' package not installed${NC}"
    echo "Installing..."
    pip3 install requests || {
        echo -e "${RED}Failed to install requests${NC}"
        exit 1
    }
fi

# Check for zeroconf (optional)
if ! python3 -c "import zeroconf" 2>/dev/null; then
    echo -e "${YELLOW}Note: 'zeroconf' package not installed (optional for mDNS discovery)${NC}"
    echo "You can install it with: pip3 install zeroconf"
    echo "Or use --ip to specify IP address manually"
    echo ""
fi

echo -e "${GREEN}✓ Dependencies OK${NC}"
echo ""

# Run Python script with all arguments
python3 "$SCRIPT_DIR/flash_micro_sd.py" "$@"

#!/bin/bash
#
# Golden RNG Enterprise - Installation Script
# Version: 5.0.0
#

set -e

# Default installation paths
PREFIX="${PREFIX:-/usr/local}"
BINDIR="${PREFIX}/bin"
DATADIR="${PREFIX}/share/doc/goldenrng"
MANDIR="${PREFIX}/man/man1"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "============================================"
echo "  Golden RNG Enterprise v5.0 - Installer"
echo "============================================"
echo ""

# Check if running as root for system-wide installation
if [ "$EUID" -ne 0 ] && [ "$PREFIX" = "/usr" ]; then
    echo -e "${RED}Error: System-wide installation requires root privileges${NC}"
    echo "Please run: sudo $0"
    echo "Or use PREFIX=/usr/local for local installation"
    exit 1
fi

# Check for required files
if [ ! -f "./src/goldenrng" ]; then
    echo -e "${YELLOW}Warning: Binary not found, building...${NC}"
    cd src
    make clean
    make BUILD_TYPE=release
    cd ..
fi

if [ ! -f "./src/goldenrng" ]; then
    echo -e "${RED}Error: Failed to build goldenrng${NC}"
    exit 1
fi

# Create directories
echo "Creating directories..."
mkdir -p "$BINDIR"
mkdir -p "$DATADIR"
mkdir -p "$MANDIR"

# Install binary
echo "Installing binary..."
install -m 755 src/goldenrng "$BINDIR/"

# Install documentation
echo "Installing documentation..."
if [ -f "README.md" ]; then
    install -m 644 README.md "$DATADIR/"
fi

if [ -f "LICENSE" ]; then
    install -m 644 LICENSE "$DATADIR/"
fi

# Install man page
if [ -f "docs/goldenrng.1" ]; then
    install -m 644 docs/goldenrng.1 "$MANDIR/"
fi

# Create configuration directory
mkdir -p /etc/goldenrng

echo ""
echo -e "${GREEN}Installation complete!${NC}"
echo ""
echo "Installed files:"
echo "  Binary:    $BINDIR/goldenrng"
echo "  Docs:      $DATADIR/"
echo "  Config:    /etc/goldenrng/"
echo ""
echo "To verify installation, run:"
echo "  $BINDIR/goldenrng -m test"
echo ""
echo "To uninstall, run:"
echo "  $ uninstall.sh"


#!/bin/bash
#
# Golden RNG Enterprise - Uninstallation Script
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

echo "================================================"
echo "  Golden RNG Enterprise v5.0 - Uninstaller"
echo "================================================"
echo ""

# Check if running as root for system-wide uninstallation
if [ "$EUID" -ne 0 ] && [ "$PREFIX" = "/usr" ]; then
    echo -e "${RED}Error: System-wide uninstallation requires root privileges${NC}"
    echo "Please run: sudo $0"
    exit 1
fi

# Remove binary
echo "Removing binary..."
if [ -f "$BINDIR/goldenrng" ]; then
    rm -f "$BINDIR/goldenrng"
    echo -e "${GREEN}Removed: $BINDIR/goldenrng${NC}"
else
    echo -e "${YELLOW}Warning: $BINDIR/goldenrng not found${NC}"
fi

# Remove documentation
echo "Removing documentation..."
if [ -d "$DATADIR" ]; then
    rm -rf "$DATADIR"
    echo -e "${GREEN}Removed: $DATADIR${NC}"
fi

# Remove man page
echo "Removing man page..."
if [ -f "$MANDIR/goldenrng.1" ]; then
    rm -f "$MANDIR/goldenrng.1"
    echo -e "${GREEN}Removed: $MANDIR/goldenrng.1${NC}"
fi

# Remove configuration directory (optional - ask user)
if [ -d "/etc/goldenrng" ]; then
    echo ""
    read -p "Remove configuration directory /etc/goldenrng? [y/N] " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        rm -rf /etc/goldenrng
        echo -e "${GREEN}Removed: /etc/goldenrng${NC}"
    fi
fi

echo ""
echo -e "${GREEN}Uninstallation complete!${NC}"


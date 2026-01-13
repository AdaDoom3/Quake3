#!/bin/bash
# Script to remove original JPEG files after conversion to TGA

echo "=========================================="
echo "Cleanup Script - Remove Original JPEGs"
echo "=========================================="
echo ""

# Count JPEG files
JPEG_COUNT=$(find assets -type f \( -iname "*.jpg" -o -iname "*.jpeg" \) | wc -l)

echo "Found $JPEG_COUNT JPEG files to remove"
echo ""

if [ "$JPEG_COUNT" -eq 0 ]; then
    echo "No JPEG files found. Already cleaned up?"
    exit 0
fi

# Confirm before deletion
read -p "Are you sure you want to delete all JPEG files? (yes/no): " CONFIRM

if [ "$CONFIRM" != "yes" ]; then
    echo "Cleanup cancelled."
    exit 0
fi

# Delete all JPEG files
echo "Deleting JPEG files..."
find assets -type f \( -iname "*.jpg" -o -iname "*.jpeg" \) -delete

# Verify deletion
REMAINING=$(find assets -type f \( -iname "*.jpg" -o -iname "*.jpeg" \) | wc -l)

echo ""
echo "Cleanup complete!"
echo "Remaining JPEG files: $REMAINING"
echo ""

if [ "$REMAINING" -eq 0 ]; then
    echo "✓ All JPEG files successfully removed"
else
    echo "⚠ Warning: $REMAINING JPEG files still remain"
fi

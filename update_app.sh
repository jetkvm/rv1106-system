#!/bin/bash
set -eE 
set -x

# Download latest app binary
curl -L https://api.jetkvm.com/releases/app/latest -o /tmp/jetkvm_app

# Verify download completed successfully
if [ ! -f /tmp/jetkvm_app ]; then
    echo "Error: Failed to download latest app binary"
    exit 1
fi

# Make executable
chmod +x /tmp/jetkvm_app

# Replace existing binary
mv /tmp/jetkvm_app project/app/jetkvm/jetkvm/bin/jetkvm_app

echo "Successfully updated jetkvm_app to latest version"

rm -rf project/app/jetkvm/out
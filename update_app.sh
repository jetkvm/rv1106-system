#!/bin/bash
set -eE

SKU="${1:?Error: SKU is required (usage: $0 <sku>)}"

curl -fL "https://api.jetkvm.com/releases/app/latest?sku=${SKU}" -o /tmp/jetkvm_app

if [ ! -s /tmp/jetkvm_app ]; then
    echo "Error: Failed to download latest app binary for SKU ${SKU}"
    exit 1
fi

chmod +x /tmp/jetkvm_app
mv /tmp/jetkvm_app project/app/jetkvm/jetkvm/bin/jetkvm_app

echo "Successfully updated jetkvm_app to latest version for SKU ${SKU}"

rm -rf project/app/jetkvm/out

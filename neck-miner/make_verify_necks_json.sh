#!/bin/bash

if [ -z "$1" ]; then
        echo "Must provide targets directory path"
        exit
fi

if [ -z "$2" ]; then
        echo "Must provide json output path"
        exit
fi

cat <<EOT > $2
{
    "module-defaults": {
        "taint-config": "config/cmd-tool-config.json",
        "function-local-points-to-info-wo-globals": true,
        "use-simplified-dfa": true,
        "timeout-duration": "10m",
        "timeout-kill-after": "5m"
    },
    "modules": [
EOT

echo "Found modules:"
for file in  ${1%/}/*; do
    echo "   - $file"
    cat <<EOT >> $2
        {
            "path": "$file"
        },
EOT
done

# remove last comma
sed -i '$ s/,$//' $2

cat <<EOT >> $2
    ]
}
EOT
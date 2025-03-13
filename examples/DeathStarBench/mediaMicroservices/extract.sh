#!/bin/bash

# Check if a prefix is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <prefix>"
    exit 1
fi

prefix="$1"

# Find the Kubernetes pod with 'loadgenerator' in its name
pod_name=$(kubectl get pods --no-headers -o custom-columns=":metadata.name" | grep loadgenerator | head -n 1)

if [ -z "$pod_name" ]; then
    echo "No pod with 'loadgenerator' in its name found."
    exit 1
fi

echo "Found pod: $pod_name"

# Execute kubectl commands to get the logs
kubectl exec "$pod_name" -- cat requests_log.txt > "${prefix}.txt"
echo "Exported requests log to ${prefix}.txt"

kubectl exec "$pod_name" -- cat locust.out > "locust_stats_${prefix}.txt"
echo "Exported locust stats to locust_stats_${prefix}.txt"

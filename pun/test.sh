#!/bin/bash

# Read test parameters and run client-server tests
SERVER_PORT=8080

# Function to cleanup processes
cleanup_processes() {
    if [ ! -z "$SERVER_PID" ] && kill -0 $SERVER_PID 2>/dev/null; then
        kill $SERVER_PID
        wait $SERVER_PID 2>/dev/null
    fi
    if [ ! -z "$CLIENT_PID" ] && kill -0 $CLIENT_PID 2>/dev/null; then
        kill $CLIENT_PID
        wait $CLIENT_PID 2>/dev/null
    fi
}

# Trap to cleanup on script exit
trap cleanup_processes EXIT

# Get file names from command line arguments
if [ $# -gt 0 ]; then
    FILES="$@"
else
    exit 1
fi

if [ ! -f "TEST_PARAMS.txt" ]; then
    echo "TEST_PARAMS.txt not found!"
    exit 1
fi

# Read each line from TEST_PARAMS.txt
while IFS=' ' read -r client_drop client_corrupt server_drop server_corrupt; do
    # Skip empty lines
    [ -z "$client_drop" ] && continue
    
    echo "Running test: client_drop=$client_drop client_corrupt=$client_corrupt server_drop=$server_drop server_corrupt=$server_corrupt"
    
    # Start server in background with log redirection
    ./server.out $SERVER_PORT $server_drop $server_corrupt > server.log 2>&1 &
    SERVER_PID=$!
    
    # Wait a moment for server to start
    sleep 1
    
    # Run client with log redirection in background
    ./client.out 127.0.0.1 $SERVER_PORT $client_drop $client_corrupt $FILES > client.log 2>&1 &
    CLIENT_PID=$!
    
    # Wait for client to finish
    wait $CLIENT_PID
    CLIENT_PID=""
    
    # Kill server after client finishes
    cleanup_processes
    
    # Create test log directory
    TEST_DIR="test-logs/${client_drop}_${client_corrupt}_${server_drop}_${server_corrupt}"
    mkdir -p "$TEST_DIR"
    
    # Copy logs and client files
    cp server.log "$TEST_DIR/" 2>/dev/null || echo "server.log not found"
    cp client.log "$TEST_DIR/" 2>/dev/null || echo "client.log not found"
    cp -r clientFiles "$TEST_DIR/" 2>/dev/null || echo "clientFiles not found"
    
    echo "Test completed. Logs saved to $TEST_DIR"
    echo "---"

    rm -rf clientFiles/*
    
done < TEST_PARAMS.txt

echo "All tests completed!"
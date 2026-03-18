#!/bin/bash

cd /home/kavunvp/cpp-backend/sprint2/problems/time_control/solution

# Запускаем сервер
./build-release/bin/game_server data/config.json static/ &
SERVER_PID=$!
sleep 2

echo "=== Testing map1, move Down ==="

# Join
echo "Joining..."
curl -s -X POST http://127.0.0.1:8080/api/v1/game/join \
    -H "Content-Type: application/json" \
    -d '{"userName":"test","mapId":"map1"}' > /tmp/join_response.json
cat /tmp/join_response.json
echo ""

TOKEN=$(grep -o '"authToken":"[^"]*"' /tmp/join_response.json | cut -d'"' -f4)
echo "Token: $TOKEN"

# Move down
echo "Moving Down..."
curl -s -X POST http://127.0.0.1:8080/api/v1/game/player/action \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $TOKEN" \
    -d '{"move":"D"}'
echo ""

# Get state before tick
echo "State before tick:"
curl -s http://127.0.0.1:8080/api/v1/game/state \
    -H "Authorization: Bearer $TOKEN"
echo ""

# Tick 10000ms (10 seconds) - should move from (0,0) to (0, 30.4) along vertical road
echo "Ticking 10000ms..."
curl -s -X POST http://127.0.0.1:8080/api/v1/game/tick \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $TOKEN" \
    -d '{"timeDelta":10000}'
echo ""

# Get state after tick
echo "State after tick 10000ms (expected: pos [0.0, 30.4]):"
curl -s http://127.0.0.1:8080/api/v1/game/state \
    -H "Authorization: Bearer $TOKEN"
echo ""

# Kill server
kill $SERVER_PID 2>/dev/null

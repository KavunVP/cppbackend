#!/bin/bash

cd /home/kavunvp/cpp-backend/sprint2/problems/time_control/solution

# Запускаем сервер
./build-release/bin/game_server data/config.json static/ &
SERVER_PID=$!
sleep 2

echo "=== Test 1: map1, move Right ==="
# Join
curl -s -X POST http://127.0.0.1:8080/api/v1/game/join \
    -H "Content-Type: application/json" \
    -d '{"userName":"test","mapId":"map1"}' > /tmp/join_response.json
TOKEN=$(grep -o '"authToken":"[^"]*"' /tmp/join_response.json | cut -d'"' -f4)

# Move right
curl -s -X POST http://127.0.0.1:8080/api/v1/game/player/action \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $TOKEN" \
    -d '{"move":"R"}' > /dev/null

# Tick 15000ms - should move from (0,0) to (40.4, 0.4) along horizontal road
curl -s -X POST http://127.0.0.1:8080/api/v1/game/tick \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $TOKEN" \
    -d '{"timeDelta":15000}' > /dev/null

echo "State after tick 15000ms (expected: pos [40.4, 0.4]):"
curl -s http://127.0.0.1:8080/api/v1/game/state \
    -H "Authorization: Bearer $TOKEN"
echo ""

# Kill server
kill $SERVER_PID 2>/dev/null
sleep 1

# Запускаем сервер снова
./build-release/bin/game_server data/config.json static/ &
SERVER_PID=$!
sleep 2

echo "=== Test 2: town, move Right ==="
# Join
curl -s -X POST http://127.0.0.1:8080/api/v1/game/join \
    -H "Content-Type: application/json" \
    -d '{"userName":"test","mapId":"town"}' > /tmp/join_response.json
TOKEN=$(grep -o '"authToken":"[^"]*"' /tmp/join_response.json | cut -d'"' -f4)

# Move right
curl -s -X POST http://127.0.0.1:8080/api/v1/game/player/action \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $TOKEN" \
    -d '{"move":"R"}' > /dev/null

# Tick 15000ms
curl -s -X POST http://127.0.0.1:8080/api/v1/game/tick \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $TOKEN" \
    -d '{"timeDelta":15000}' > /dev/null

echo "State after tick 15000ms (expected: pos [40.4, 0.4]):"
curl -s http://127.0.0.1:8080/api/v1/game/state \
    -H "Authorization: Bearer $TOKEN"
echo ""

# Kill server
kill $SERVER_PID 2>/dev/null

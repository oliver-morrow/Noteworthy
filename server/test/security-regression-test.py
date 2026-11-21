"""Security regression checks. Run against a local server: docker compose up -d, then python3 security-regression-test.py"""
import asyncio, json
import requests, websockets

HTTP, WS = "http://127.0.0.1:8080", "ws://127.0.0.1:8081"
ROOM, PAGE, USER, STROKE, TEXT = 0, 1, 2, 3, 6
results = []


def check(name, ok, detail=""):
    results.append(ok)
    print(f"{'PASS' if ok else 'FAIL'}  {name}" + (f"  ({detail})" if detail else ""))


def alive():
    try:
        return requests.get(f"{HTTP}/v1/rooms/NOPE0", timeout=3).status_code == 404
    except requests.RequestException:
        return False


def room(owner):
    return requests.post(f"{HTTP}/v1/rooms", params={"username": owner}).text


def join(r, user):
    return requests.post(f"{HTTP}/v1/rooms/{r}/users", params={"username": user})


def ws(r, user):
    return websockets.connect(f"{WS}?username={user}&room_id={r}", subprotocols=["echo-protocol"])


async def recv_all(sock, timeout=0.7):
    out = []
    try:
        while True:
            msg = await asyncio.wait_for(sock.recv(), timeout)
            out.append(json.loads(msg.rstrip("\x00")))
    except (asyncio.TimeoutError, websockets.ConnectionClosed):
        pass
    return out


async def main():
    # --- the showcase flow still works ---
    r = room("alice")
    check("join second user", join(r, "bob").status_code == 201)
    async with ws(r, "alice") as a, ws(r, "bob") as b:
        await recv_all(a); await recv_all(b)
        await a.send(json.dumps({"object_type": PAGE, "event_type": 2, "room_id": r,
                                 "page_id": 111, "previous_page_id": 0, "base64_image": ""}))
        got = await recv_all(b)
        check("page insert reaches other user", any(e.get("object_type") == PAGE for e in got))
        await a.send(json.dumps({"object_type": STROKE, "event_type": 0, "room_id": r, "page_id": 111,
                                 "object_id": 5, "owner_id": "alice", "points": [[1.0, 2.0]]}))
        got = await recv_all(b)
        check("stroke reaches other user", any(e.get("object_type") == STROKE for e in got))
        await a.send(json.dumps({"object_type": STROKE, "event_type": 5, "room_id": r, "page_id": 111,
                                 "object_id": 5, "owner_id": "alice", "new_points": [[3.0, 4.0]]}))
        got = await recv_all(b)
        check("stroke append reaches other user", any(e.get("event_type") == 5 for e in got))

        # HTTP-thread broadcast now goes through the service-thread queue
        join(r, "carol")
        got = await recv_all(a)
        check("HTTP join broadcast reaches WS users",
              any(e.get("object_type") == USER and e.get("username") == "carol" for e in got))

        state = requests.get(f"{HTTP}/v1/rooms/{r}").json()
        check("room state has page and stroke",
              any(e.get("object_type") == STROKE for e in state) and any(e.get("object_type") == PAGE for e in state))

        # --- previously fatal messages ---
        await a.send(json.dumps({"object_type": USER, "event_type": 2, "room_id": r, "username": "carol"}))
        await a.send(json.dumps({"object_type": TEXT, "event_type": 0, "room_id": r, "page_id": 111,
                                 "object_id": 9, "owner_id": "alice"}))
        await a.send(json.dumps({"object_type": 99, "event_type": 0}))
        await a.send("not json")
        await asyncio.sleep(0.5)
        check("server survives former crash messages", alive())

        # --- room isolation and owner checks ---
        victim = room("victim")
        await a.send(json.dumps({"object_type": ROOM, "event_type": 2, "room_id": victim, "new_password": "x"}))
        await asyncio.sleep(0.3)
        check("cross-room password change blocked", requests.get(f"{HTTP}/v1/rooms/{victim}").status_code == 200)
        check("cross-room event applied to sender's own room instead",
              requests.get(f"{HTTP}/v1/rooms/{r}").status_code == 423)  # owner set own room's pw: allowed
        await a.send(json.dumps({"object_type": ROOM, "event_type": 2, "room_id": r, "new_password": ""}))
        await asyncio.sleep(0.3)

        await b.send(json.dumps({"object_type": ROOM, "event_type": 2, "room_id": r, "new_password": "bob"}))
        await asyncio.sleep(0.3)
        check("non-owner cannot set password", requests.get(f"{HTTP}/v1/rooms/{r}").status_code == 200)

        await b.send(json.dumps({"object_type": USER, "event_type": 4, "room_id": r, "username": "alice"}))
        await asyncio.sleep(0.3)
        check("non-owner cannot kick", a.state.name == "OPEN")

        # --- kick works and sticks ---
        await a.send(json.dumps({"object_type": USER, "event_type": 4, "room_id": r, "username": "bob"}))
        await recv_all(b, timeout=2)
        check("kicked user is disconnected", b.state.name != "OPEN")
    try:
        async with ws(r, "bob") as b2:
            await recv_all(b2, timeout=1)
            check("kicked user cannot reconnect", b2.state.name != "OPEN")
    except websockets.InvalidStatus:
        check("kicked user cannot reconnect", True)
    check("kicked user cannot rejoin over HTTP", join(r, "bob").status_code == 403)

    # --- rate limiting ---
    r2 = room("spam")
    join(r2, "watch")
    async with ws(r2, "spam") as s, ws(r2, "watch") as w:
        await recv_all(s); await recv_all(w)
        await s.send(json.dumps({"object_type": PAGE, "event_type": 2, "room_id": r2,
                                 "page_id": 1, "previous_page_id": 0, "base64_image": ""}))
        await s.send(json.dumps({"object_type": STROKE, "event_type": 0, "room_id": r2, "page_id": 1,
                                 "object_id": 1, "owner_id": "spam", "points": [[0.0, 0.0]]}))
        msg = json.dumps({"object_type": STROKE, "event_type": 2, "room_id": r2, "page_id": 1,
                          "object_id": 1, "owner_id": "spam", "distance_x": 1.0, "distance_y": 1.0})
        for _ in range(2000):
            await s.send(msg)
        got = await recv_all(w, timeout=1.5)
        check("message flood is throttled", len(got) < 2000, f"{len(got)} of 2002 forwarded")

    # --- HTTP input checks ---
    check("PDF import disabled", requests.post(f"{HTTP}/v1/rooms/{r2}/pdf_insert",
                                               params={"previous_page_id": 0}, data=b"%PDF").status_code == 404)
    check("overlong username rejected", requests.post(f"{HTTP}/v1/rooms", params={"username": "x" * 40}).status_code == 400)
    check("seeded demo rooms removed", requests.get(f"{HTTP}/v1/rooms/12345").status_code == 404)
    check("server alive at end", alive())
    print(f"\n{sum(results)}/{len(results)} passed")


asyncio.run(main())

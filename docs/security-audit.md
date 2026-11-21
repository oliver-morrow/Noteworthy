# Noteworthy security audit (public demo readiness)

Audit date: 2026-11-20. Scope: `server/` (C++ HTTP API on 8080, WebSocket on 8081), `server/compose.yaml`, `server/Dockerfile`, `qt/dockerfile`, `qt/nginx.conf`. Target deployment: a VM on a capstone PC, public traffic through Cloudflare Tunnel, admin over Tailscale.

## Status (2026-11-20)

Fixed in the working tree and verified against a local build (`server/test/security-regression-test.py`, 19/19 passing):

- **C1:** every lws callback is wrapped; string-literal throws are gone; unknown and `TEXT` object types are rejected.
- **C2:** every event is applied to the sender's own room.
- **C3:** ROOM events and kicks are owner-only; clients can't send CONNECT/DISCONNECT; kicks now stick (WebSocket and HTTP).
- **C4:** PDF import is off unless `NW_ENABLE_PDF=1`, and the Qt upload button is hidden unless the client is built with `NW_ENABLE_PDF`. When enabled: PDFium mutex, in-memory parsing, 20-page and 4 MP caps, null checks, documents closed, no temp files.
- **C5:** broadcasts from HTTP threads go through a queue drained on the lws service thread; `hasRoom` locks.
- **L1–L4:** room, user, page, object, and stroke caps; idle rooms are evicted after 120 minutes; per-user message token bucket; per-message logging only with `NW_DEBUG_LOG=1`; Docker log rotation.
- **D1–D4:** loopback-only ports; memory, CPU, and PID limits; read-only, non-root, no capabilities; multi-stage image on `ubuntu:24.04`; Pistache and cloudflared pinned; seeded rooms removed.

Still open:

- **D5:** WebSocket identity is still just the username. Fixing it needs a session token from the HTTP API and a client change.
- **D6:** room IDs stay at 5 characters because the client's room-code UI is fixed at 5. Rely on Cloudflare rate limits.
- **D7:** room password is still in the room JSON. The client's `RoomState::fromJson` requires the key.
- Not done yet: the Qt client change is untested (no Qt toolchain here), and the Cloudflare rules and VM setup.

## What Cloudflare Tunnel covers, and what it doesn't

| Covered by the tunnel | Not covered |
|---|---|
| No inbound ports on the PC or router; origin IP hidden | WebSocket frames after the upgrade. Cloudflare doesn't inspect or rate-limit them |
| TLS for HTTPS and WSS | A single well-formed request or message that crashes the server (see C1) |
| Volumetric / L3–L4 DDoS | Authorization between rooms (C2, C3) |
| Optional HTTP rate-limit and WAF rules on the REST endpoints | Slow memory growth that stays under any rate limit (L1) |
| Upload size cap (100 MB on free plans, so the app's 10 MB is the real limit) | What's inside an uploaded PDF (C4) |

The tunnel plus the VM handles exposure of the PC. The application itself is where the work is.

## Critical: fix before going public

**C1. One WebSocket message can kill the whole server.** `handleEvent` throws raw string literals (`throw "..."`), which the `catch (const std::exception&)` / `catch (std::string)` handlers in `LWS_CALLBACK_RECEIVE` don't catch, so the process calls `std::terminate`.
- `WebSocketHandler.cpp:414`: forwarding to a user marked connected with a null socket. Any member can trigger this by sending a USER `CONNECT` event naming an offline user.
- `WebSocketHandler.cpp:270`: `createCanvasObject` for `TEXT` or an unknown `object_type`.
- `handleEvent` calls from `LWS_CALLBACK_ESTABLISHED` (`:187`) and `LWS_CALLBACK_CLOSED` (`:222`) have no try/catch at all.
- Impact: every room is lost (state is in memory). `restart: unless-stopped` restarts it empty, and the attacker can repeat.
- **Confirmed locally:** one USER `CONNECT` message naming an offline member logged `terminate called after throwing an instance of 'char const*'`, after which neither port answered.

**C2. No room isolation on the WebSocket.** Every handler uses `event["room_id"]` from the message body, not the room the connection joined (`WebSocketHandler.cpp:293, 309, 315, 325, 345, 361, 372, 384, 403`). A user who joins *any* room can draw in, delete pages from, rename the password of, or take ownership of any other room whose 5-character ID they know or guess. The event is also forwarded to that room's users. **Confirmed locally:** a member of room A sent `CHANGE_PASSWORD` with room B's ID, and `GET` on room B went from 200 to 423 (password required).

**C3. No owner checks.** `RoomState::applyEvent` lets any member `CHANGE_PASSWORD` or `PROMOTE_USER` (make themselves owner). USER events can flip another user's `is_connected`, which feeds C1. `KICK` sets `is_kicked = false` (`User.hpp`), so kicking does nothing.

**C4. PDF import path.**
- PDFium is not thread-safe, but Pistache runs 5 handler threads (`main-api-server.cpp:396`). Two concurrent uploads can corrupt memory.
- Untrusted PDFs are parsed by a prebuilt `libpdfium.so` (chromium/6858, about two years old, x86-64 only, provenance not recorded).
- No cap on page count or page size. A huge MediaBox makes `FPDFBitmap_Create` return null, which isn't checked (`:350`) → null dereference, crash.
- `FPDF_CloseDocument` is never called (leaks per upload). Temp files `uploaded_file<thread>.pdf` are never deleted.
- A bad `previous_page_id` calls `response.send` twice (`:299` then `:324`) and leaves pages half-inserted. `std::stoull` overflow (`std::out_of_range`) isn't caught.

**C5. libwebsockets is called from the wrong threads.** `createUser` and `insertPDFPages` run on Pistache threads and call `lws_write` directly through `User::sendEvent`, while the lws service thread is also writing. libwebsockets only supports writes from its own service thread. Expect sporadic crashes under concurrent use. `ServerState::hasRoom` and the room-ID collision loop also read `room_map` without the lock.

## Load protection

**L1. Unbounded in-memory state.** No limits on rooms (`POST /v1/rooms`), users per room, pages (WS `INSERT`), objects per page, or points per stroke (append events). Nothing is ever evicted: room deletion is commented out (`WebSocketHandler.cpp:223–236`). Memory only grows.

**L2. PDF amplification.** A 10 MB upload becomes one PNG per page, base64-encoded, held forever, and resent in full on every `GET /v1/rooms/:id`. Rendering is CPU-heavy and has no queue.

**L3. Logs fill the disk.** Every WebSocket message is printed (`WebSocketHandler.cpp:276`), and Docker's default json-file log driver has no rotation.

**L4. No per-connection message rate.** Each message is fanned out to every user in the room while the global `room_map_mutex` is held, so one noisy client slows every room.

## Deployment and configuration

- **D1.** `compose.yaml` publishes 8080/8081 on all interfaces, and Docker's iptables rules bypass ufw. With `cloudflared` on the same Compose network, publish no ports.
- **D2.** No memory, CPU, or PID limits. The container runs as root with a writable filesystem.
- **D3.** Builds aren't reproducible: `ubuntu:latest` (now 26.04), `cloudflare/cloudflared:latest`, and Pistache from git `HEAD`. Confirmed: `server/Dockerfile` no longer builds as committed, because CMake 4 on 26.04 rejects Pistache's CMake config. The PDFium binary also forces `linux/amd64`.
- **D4.** `main.cpp` seeds room `ABCDE` (password `ggggghh`, public in this repo) and an open room `12345`.
- **D5.** Identity on the WebSocket is just the `username` query parameter. The handshake doesn't check the room password, so anyone who knows a disconnected member's name and the room ID can reconnect as them.
- **D6.** Room IDs are 5 characters from a 34-character alphabet (about 45M) and generated with `mt19937_64`. Combined with C2, enumeration matters. Lengthen IDs and rate-limit `GET /v1/rooms/:id`.
- **D7.** `GET /v1/rooms/:id` returns the room password in the room JSON (`RoomState::toJson`). Only authenticated members can call it, but it isn't needed.

## Hardening plan

1. **Code fixes (C1–C5).**
   - Wrap every lws callback body in `catch (...)` and replace string-literal throws with exceptions.
   - Overwrite `event["room_id"]` with the connection's room. Reject USER events not about the sender. Make ROOM events owner-only.
   - Move HTTP-thread broadcasts onto a queue drained in the lws service thread (`lws_cancel_service` + `LWS_CALLBACK_EVENT_WAIT_CANCELLED`).
   - Lock `hasRoom`.
2. **PDF import.** Either turn it off for the public demo with a build/env flag, or harden it: global PDFium mutex, `FPDF_LoadMemDocument` instead of temp files, max pages and pixel area, null checks, `FPDF_CloseDocument`, a single-slot render queue.
3. **Limits.** Max rooms, idle-room eviction (e.g. 2 h), max users per room, pages per room, objects per page, points per stroke, messages per second per connection. Drop per-message logging.
4. **Container.** No published ports; `mem_limit`, `cpus`, `pids_limit`; `read_only` with a small tmpfs; non-root user; `cap_drop: [ALL]`; `no-new-privileges`; log rotation; pinned base images and Pistache tag.
5. **Cloudflare.** Rate-limit rule on `POST /v1/rooms*` and `GET /v1/rooms/*`; managed WAF rules on. Serve the client and API from the same hostname so CORS stays unnecessary.
6. **VM.** No capstone mounts or shared folders. Guest firewall denies inbound. Tunnel credentials live only in the guest.

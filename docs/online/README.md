# Online (experimental)

This branch lets the game play online against a community server: the online menus, leaderboards, challenges and
profiles, friends, invitations, **private lobbies and races between players** (peer to peer, over a LAN or a VPN
such as Tailscale, Hamachi or Radmin). It is a work in progress. Nothing here is in a release yet.

Deutsche Kurzanleitung: [ANLEITUNG.de.md](ANLEITUNG.de.md).

## What works

- The game signs in with a local account, with no PSN and no shadNet server.
- Login to the game's own services, profiles, the friends list, leaderboards (read and score posting), event
  history, challenge lists, challenges sent to friends.
- Every other player on the server is a friend and shows as online.
- Private lobbies: invite a friend, they join, both see each other in the lobby, the host starts an event, both
  race together, and after the race both are back in the lobby.
- Players on different PCs: on one LAN or through a VPN (tested with Tailscale and Hamachi).

## What does not work yet

- Public matchmaking (the "Multiplayer" events), clubs, tours with others and ghosts.
- Each game runs its own simulation, and its speed follows the frame rate. With different frame rate settings
  (or one PC that cannot hold its target), race clocks and weather drift apart: in one test 4.3 s over 3:24. All
  players should use the same fixed frame rate in the launcher (e.g. 60 FPS, "Unlocked" off); then the times
  match closely.
- The game reads its friend list only when it starts: someone who comes online later is not listed until you
  restart the game. Let the others in first, then start the host's game.
- Presence (what a friend is doing) is accepted and not shown.
- The add-ons store answers with an empty container.

## Play with friends

You need: your own copy of the game (PS4, CUSA00093, version 1.28), a build of this branch, the community server
(only the host), and for players outside your home network a VPN that all of you join (Tailscale is the quickest:
direct connections, free for 3 users).

1. Build the server from [Judaswick/driveclub-server](https://github.com/Judaswick/driveclub-server) branch
   `coop` (see "The server" below), or take it from a release. Only the host needs it.
2. Copy the files from `play/` next to `DriveClub.exe`, and `override-0.tss` too. The host puts the server in a
   folder `server` next to them (or sets `server=` in `online.ini`), with `server-config.json` renamed to
   `config.json`.
3. Every player runs `Firewall-Freigabe.bat` once (adds inbound rules for the game, and for the server on the host).
4. Every player starts `DriveClub.exe` once to pick the game folders and sets the **same fixed frame rate** as
   the others (e.g. 60 FPS, "Unlocked" off), then closes it.
5. The host runs `Online-Spielen.bat` with `host=self`. It starts the server and shows the host's IP addresses.
6. The others run `Online-Spielen.bat` with the host's IP (the VPN address, e.g. `100.x.x.x` for Tailscale).
7. When they are in the main menu, the host presses Enter to start the game, opens a private lobby and invites them.

`online.ini` keeps the name and the host; delete it to change them.

## How it fits together

1. `BB_ONLINE=1` sends the `sceNet`, `sceNp`, `sceHttp`, `sceSsl` and `sceRudp` calls to the network libraries in
   `gpu/shadps4` instead of the offline stubs.
2. `BB_NP_LOCAL=<name>` makes the game see a signed-in account called `<name>` that lives on this PC. The account ID
   is derived from the name. `BB_COOP_SERVER=<host:port>` is the community server for the runtime's own requests
   (default `127.0.0.1:8080`).
3. `data/user/host_overrides.json` sends the game's old hostnames to the community server over plain http
   (see `host_overrides.json` here; `Online-Spielen.ps1` writes it).
4. The game reads its server settings from a "TSS" file. `mods/override-0.tss` replaces it (see `override-0.tss`
   here). The `OTG_*` keys switch on the leaderboard service, `fa_egsConnection` the server connection, and
   `egs_timeout 3600` stops the game from dropping its session every 30 seconds. Slots 1 to 6 are files with the
   single line `<< Config >>`, so the game does not wait for a storage server that no longer exists.
5. Invitations go through the server as PSN "game custom data": the runtime polls `dccoop/pending/<name>` and raises
   the system event the game waits for. The server then pushes the inviter's lobby to both games (see
   `lobby-notes.md`).
6. Races are peer to peer. The game looks up the other player's profile (`/v1/profiles`, which must carry `npId`),
   opens a connection with `sceNpSignalingActivateConnection` and then talks over `sceRudp`:
   - Without a matchmaking server, each runtime registers the UDP port of its P2P transport on the community server
     (`dccoop/p2p/register/<name>?port=N`) and resolves peers there (`dccoop/p2p/<name>`). The server answers with
     the address it sees, or its own address towards the asker for a game on the server's PC.
   - `gpu/shadps4/core/libraries/network/rudp.cpp` is a small `libSceRudp`: both ends call `sceRudpInitiate` (a
     simultaneous open), reliable contexts get acks, retransmission and in-order delivery, the others are plain
     datagrams. It runs on its own virtual port of the P2P transport. Both players need this runtime; it does not
     talk to a console.
   - The game owns two signaling contexts. An incoming connection goes to the newest one, also when the game creates
     it after the peer connected, and `PEER_ACTIVATED` reaches it; the game answers with its own activation.

## The server

[shad-net/driveclub-server](https://github.com/shad-net/driveclub-server) (GPL-2.0) with the online changes on
[Judaswick/driveclub-server](https://github.com/Judaswick/driveclub-server), branch `coop` (lobbies, invitations,
messaging push, matching stubs, profiles with `npId`, the P2P directory). The patches in `server-patches` are an
older state of the same work.

Build (Qt 6.10 or newer and protobuf; on Windows with MSYS2 CLANG64 the packages are
`mingw-w64-clang-x86_64-{qt6-base,protobuf}`):

```
git clone -b coop https://github.com/Judaswick/driveclub-server
cd driveclub-server
cmake -S . -B out -G Ninja -DCMAKE_BUILD_TYPE=Release -DGAME_SERVER_DEPLOY_QT=OFF
ninja -C out
```

Run `driveclubsrv.exe` from a folder that holds `server-config.json` renamed to `config.json`, with the Qt DLLs and
`sqldrivers/qsqlite.dll` next to the exe (or run `windeployqt`). With `shadnet.url` empty and `allow_unverified` true
it accepts any login. It listens on all interfaces (port 8080): keep it behind your router or VPN, since anyone who
reaches it can sign in under any name.

## Tools for continuing

Run with `BB_GPU_LOG=info` and `BB_GPU_LOG_ONLY=NpHandler,Lib.NpSignaling,Lib.Net,Lib.Np` for the network side.
These are for reverse engineering and do nothing unless set:

- `BB_GUEST_TRACE="0xADDR,..."` logs the registers whenever the game reaches one of those guest addresses
  (`BB_GUEST_TRACE_MAX` hits each, default 20); `BB_GUEST_TRACE_DUMP="reg:len"` also dumps memory at a register.
- `BB_GUEST_PATCH="0xADDR=HEXBYTES,..."` writes bytes into guest code before the game starts.
- `BB_GUEST_SAMPLE=1` samples where the game's threads are, 20 times a second.

`lobby-notes.md` has what is known about the lobby and messaging code, with guest addresses
(0x800000000 + ELF vaddr).

## Notes

- `host_overrides.json` ends with a catch-all (`*`) so nothing from the game goes to the original hosts. Remove it
  if you want other traffic to leave the PC.
- The game files are not part of anything here; every player needs their own copy.

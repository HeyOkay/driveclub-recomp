# Online (experimental)

This branch lets the game talk to a community server: the online menus, leaderboards, challenges screens and
profiles load, scores are posted and stored. It is a work in progress. Nothing here is in a release.

## What works

- The game signs in with a local account, with no PSN and no shadNet server.
- Login to the game's own services, profiles, the friends list, leaderboards (read and score posting), event
  history, challenge lists.
- Two players on one server see each other as friends and on the leaderboards.

## What does not work yet

- **The game flashes "Disconnected" about every 30 seconds** and can drop back to a menu if you are on a screen that
  needs the server at that moment. See "The 30 second disconnect" below. This is the main open problem.
- Friends are "everyone else on the server". Presence is accepted and not stored.
- The add-ons store answers with an empty container.
- Matchmaking, clubs, live races and ghosts are not done. Races need `sceRudp`, which no library in the tree has.
- Challenge creation has not been tried against the server yet. Expect unhandled requests in the server log.

## How it fits together

1. `BB_ONLINE=1` sends the `sceNet`, `sceNp`, `sceHttp` and `sceSsl` calls to the network libraries in `gpu/shadps4`
   instead of the offline stubs.
2. `BB_NP_LOCAL=<name>` makes the game see a signed-in account called `<name>` that lives on this PC. The account ID
   is derived from the name.
3. `data/user/host_overrides.json` sends the game's old hostnames to the community server over plain http
   (see `host_overrides.json` here). The runtime already supports this file.
4. The game reads its server settings from a "TSS" file. `mods/override-0.tss` replaces it (see `override-0.tss`
   here). The `OTG_*` keys switch on the leaderboard service and `fa_egsConnection` switches on the server
   connection. Slots 1 to 6 can be empty files with the single line `<< Config >>`, so the game does not wait for
   a storage server that no longer exists.
5. The server is [shad-net/driveclub-server](https://github.com/shad-net/driveclub-server) (GPL-2.0). The patches in
   `server-patches` were made against its commit `01f4ffa` and apply together.

## Try it

Build the server (Qt 6.10 or newer and protobuf) after applying the patches:

```
git clone https://github.com/shad-net/driveclub-server
cd driveclub-server
git apply ../driveclub-recomp/docs/online/server-patches/*.patch
cmake -S . -B out -G Ninja -DCMAKE_BUILD_TYPE=Release -DGAME_SERVER_DEPLOY_QT=OFF
ninja -C out
```

On Windows with MSYS2 CLANG64 the packages are `mingw-w64-clang-x86_64-{qt6-base,protobuf}`. Run
`driveclubsrv.exe` from a folder that holds `server-config.json` renamed to `config.json`. With `shadnet.url` empty
and `allow_unverified` true the server accepts any login, and it listens on 127.0.0.1 only. Copy the Qt DLLs and
`sqldrivers/qsqlite.dll` next to the exe (or run `windeployqt`).

Then, next to `DriveClub.exe`:

- copy `host_overrides.json` to `data/user/`
- copy `override-0.tss` to `mods/`, and make `override-1.tss` to `override-6.tss` containing `<< Config >>`
- start the game with `online_test.bat`, or set the two variables yourself

For a second player on the same PC, copy the whole game folder and change `BB_NP_LOCAL` (and `name=` in
`launcher.ini`) to another name.

## The 30 second disconnect

What we know, from logs of two players on one server:

- The game logs in to its server again every 30.1 seconds, always with `oauthLogin` (never `oauthRefresh`, which
  the server lists and accepts). Each login succeeds and returns in a few milliseconds.
- About 20 ms before each login starts, the answer of the game's "server unreachable" check goes to 1, and it goes
  back to 0 about 130 ms later. The "network down" and "signed out of PSN" answers never change. That short
  "server unreachable" is the flash. It also resets the leaderboard session, which is why a screen can drop.
- The game's server session state, the user's active flag and the user table stay steady through all of this.
- The login code arms a timer after every successful login. The session objects are created with interval arrays
  whose first value is 30000 ms (constants `0x7530` in the constructors at guest 0x631790, 0x6332a0 and 0x630cf0).
- It is not caused by the server: answering `GET /egs/messaging` as a held request (long poll) changes the polling
  rate but not the 30 second cycle (patch 02 keeps that, it is harmless and cuts the polling from 4 a second to 1 every
  10 seconds).
- Not found yet: what makes the game choose `oauthLogin` over `oauthRefresh`, and what sets the "server
  unreachable" answer when the timer fires. An attempt to raise the timer intervals in memory did nothing, so the
  layout of that timer object is not what was assumed.

Tools for continuing: build with `BB_DIAGNOSTICS=ON` and run with `BB_GPU_LOG=info` and
`BB_GPU_LOG_ONLY=Lib.Np,Lib.User`. The log then has wall clock times, `ONLINE-PROBE` lines (the game's session state
from guest memory) and `ONLINE-STATUS` lines (each change of the status bar's three answers). Compare those times with
the server log. The function that raises the status bar message is at guest 0x3daf90, the OTG leaderboard session
reset at guest 0x4b4210, and the server session state machine at guest 0x632410.

## Notes

- Keep the server on 127.0.0.1 unless you mean to host it: it has no login check in this setup.
- `host_overrides.json` ends with a catch-all (`*`) so nothing from the game goes to the original hosts. Remove it
  if you want other traffic to leave the PC.

# NickelKoSync

KOReader progress sync for Kobo's own software, for books read in Nickel's reader and in
[Kobalt](https://kobalt.nicoverbruggen.be). A [NickelHook](https://github.com/pgaskin/NickelHook)
mod that keeps the reading position of your sideloaded books in step with
[KOReader's sync server](https://github.com/koreader/koreader-sync-server) (the official
`sync.koreader.rocks`, or self-hosted servers such as
[crosspoint-sync](https://github.com/crosspoint-reader/crosspoint-sync)), so a book you
continue on another device, KOReader, CrossPoint or anything else that speaks the same
protocol, opens where you left it.

It works for books read in [Kobalt](https://github.com/nicoverbruggen/kobalt) and in
Nickel's built-in reader, without changing either of them.

## How it works

Kobalt stores each book's position as a CREngine XPointer in its own saved state, and
writes Nickel's library database like Nickel's reader does. The mod lives in the same
process as both readers and works at that storage layer:

- **Push.** When a book is closed, the mod reads the position (Kobalt's XPointer, or
  Nickel's chapter position translated into an XPointer through the chapter's XHTML)
  and sends it to the server with the percentage read. Offline, it asks Nickel to turn
  Wi-Fi on and sends as soon as the device is online.
- **Pull.** Whenever the device is online it checks books against the server: the recently
  read books at start, after Nickel's own Sync, on request and every 15 minutes while
  online; a book when it is opened; and the whole library from *Check whole library* on
  the KOSync screen. A newer position from another device is written into Kobalt's
  saved state and into Nickel's library, so the next opening starts there, also for books
  never opened on the Kobo. If Kobalt has that book open at the time, Kobalt itself offers
  to jump there, as it does for any position changed outside it.
- **Conflicts.** The newest position wins (configurable to forward-only or off). Before
  pushing, the mod always fetches the server's copy, so a stale local position never
  overwrites a newer one from another device, and a book that has only been opened here
  (cover, 0%) never overrides what another device stored.

The exchanged positions are the same XPointers KOReader uses
(`/body/DocFragment[3]/body/div[2]/p[5]/text().17`), so KOReader and CrossPoint land on
the exact paragraph. For books read in Nickel's reader the position is exact to the
kepub span (about a sentence) for kepub files and approximate for plain EPUB files.

Documents are matched the way KOReader does it: by a checksum of the file's content
(the default) or by file name. The same files must be on all devices for the default.

## Compatibility

| Firmware | Status |
|---|---|
| 4.x (Qt 5) | Built and used on 4.45 (Kobo Clara BW) |
| 5.x / 6.x (Qt 6) | Not built yet |

**Kobalt.** The mod is built for [Kobalt](https://kobalt.nicoverbruggen.be), Nico
Verbruggen's CREngine-based reader for Kobo, and needs no change to it: it reads and
writes the position files Kobalt keeps in `.adds/kobalt` and the library row Kobalt
maintains. Positions read in Kobalt are exchanged as exact XPointers. While Kobalt has a
book open, a newer position from another device reaches it through Nickel's library, which
Kobalt re-reads about once a minute before offering to jump. Nothing here requires Kobalt;
without it the mod syncs Nickel's reader only.

**Other devices.** Tested against [crosspoint-sync](https://github.com/crosspoint-reader/crosspoint-sync)
with CrossPoint on an Xteink; the protocol is KOReader's, so KOReader on any device and
the official `sync.koreader.rocks` work the same way.

Only sideloaded books (`file:///mnt/onboard/...`) are synced. Kobo store books and Kobo's
own cloud sync are left alone.

## Install

1. Download `KoboRoot.tgz` from the latest release.
2. Connect the Kobo over USB and copy it to the hidden `.kobo` folder on the device.
3. Eject. The Kobo reboots and installs the mod.
4. Open the main menu, tap **KOSync**, then tap *Server*, *Username* and *Password* and type
   the account you use in KOReader's *Progress sync* on the on-screen keyboard. The official
   server address is filled in already. That's all; nothing needs editing over USB.
5. Tap *Sync now*. A notice reports the result, and `.adds/kosync/log.txt` says
   `signed in to <server> as <user>`.

The KOSync menu entry comes from [NickelMenu](https://pgaskin.net/NickelMenu/), which the
mod adds to by itself. Without NickelMenu, set the account in `.adds/kosync/config.ini`
instead (see below) and trigger syncs with the `sync-now` file.

## The KOSync screen

Everything day-to-day lives on the KOSync screen, laid out like Kobo's own settings:

- **Sync now** checks the recently read books and **Check whole library** every EPUB on the
  device. Both always report what happened in a notice.
- **Account**: server, username, password, each opening a keyboard page.
- **Sync**: how positions from other devices are taken (newest wins, forward only, ignore),
  whether this device sends its progress, whether a book is checked when opened, Wi-Fi
  (connect when needed or leave alone), and how books are matched (file content or file
  name). Multi-value settings open a pop-up that explains each choice.
- **Options**: notifications, a *KOSync log* menu entry, detailed logging.

Changes take effect at once. After each sync a notice at the bottom of the screen lists
what moved or was sent, three lines at most ("and N more"); tap it to dismiss it, or it
goes away by itself.

## config.ini

The screen writes its settings to `.adds/kosync/config.ini`, so the file is also the way to
configure the mod over USB, and the only way for the few settings that are not on the
screen. `config.sample.ini` next to it documents every key; anything left out takes its
default.

| Setting | Default | On screen | Meaning |
|---|---|---|---|
| `[server] url` | `https://sync.koreader.rocks` | yes | Any KOSync-compatible server |
| `[server] username`, `password` | | yes | The account; `password_md5` can replace `password` |
| `[server] device` | `Kobo` | | Device name shown on other readers |
| `[server] insecure`, `ca_file` | `false`, | | TLS: accept any certificate, or add your own CA |
| `[sync] pull` | `newer` | yes | `newer` (newest wins), `forward` (only ahead), `off` |
| `[sync] push` | `true` | yes | Send this device's progress |
| `[sync] on_open` | `true` | yes | Check a book against the server when it is opened |
| `[sync] document_id` | `binary` | yes | `binary` (file content) or `filename`, as in KOReader |
| `[sync] recent_books` | `3` | | Books covered by "recent" checks |
| `[sync] pull_interval_minutes` | `15` | | Background check interval while online |
| `[sync] send_metadata` | `false` | | Send file name, title and author with each push |
| `[wifi] mode` | `connect` | yes | `connect` asks Nickel for Wi-Fi when needed; `keep` never does |
| `[wifi] cooldown_minutes` | `10` | | Minimum gap between Wi-Fi requests |
| `[options] notify` | `true` | yes | Notice after a sync |
| `[options] menu`, `menu_log` | `true`, `false` | `menu_log` | KOSync menu entry, log menu entry |
| `[options] max_books` | `300` | | Cap for "Check whole library" |
| `[options] verbose` | `false` | yes | Log every decision |

## Files

Everything lives in `.adds/kosync/`:

- `config.ini` — your settings.
- `state.json` — the device id and what was pushed or applied per book.
- `log.txt` — the log (rotated at 256 KB to `log.txt.1`).
- `sync-now` — create this empty file to trigger a sync.
- `uninstall` — create this empty file and reboot to remove the mod (see Uninstall).

## What to expect while reading

- A position from another device arrives while a book is open in **Kobalt**: Kobalt's own
  "position changed elsewhere" prompt appears within about a minute, since Kobalt re-reads
  Nickel's library on that interval. Closing and reopening the book is quicker.
- The same while a book is open in **Nickel's reader**: a notification names the device and
  its percentage; close and reopen the book to move there (Nickel's reader cannot be moved
  from outside).
- Positions are compared by paragraph. A reader that reopens an applied position reports it
  as its own slightly different XPointer, which is not sent back as a new position.
- For plain (non-kepub) EPUB files Kobalt and Nickel's reader cannot read each other's
  positions: Kobalt stores only the percentage in Nickel's library, and it cannot decode
  Nickel's own location, so after a session in the other reader each would open at the
  start of the chapter. The mod keeps the two in step: after a Kobalt close it writes
  Kobalt's position into the library in Nickel's format, and after a Nickel close it gives
  Kobalt Nickel's position as an XPointer, so both readers open at the same place.

## Uninstall

1. Connect the Kobo over USB and create an empty file named `uninstall` in `.adds/kosync`
   (next to `config.ini`).
2. Eject. On the next start the mod sees the file, deletes its library from the firmware and
   removes the `KOSync` menu entry it generated, then Nickel carries on without it.
3. Delete the `.adds/kosync` folder yourself if you also want the settings, the log and the
   state file gone. Nothing else on the device is touched: Nickel's library keeps whatever
   positions were last written, and Kobalt's files are untouched.

Installing a newer `KoboRoot.tgz` over an existing installation needs no uninstall.

## Limits

- **Standby does not sync; close the book.** Nickel switches Wi-Fi off after a few idle
  minutes of reading, so at standby there is usually no connection, and an open book cannot
  be moved on wake anyway (Nickel's reader never, Kobalt after its one-minute poll). Rather
  than a sync that works only sometimes, the mod syncs on close, on open (with a reminder)
  and on request. Holding the device awake at standby to connect would need a hook on the
  power-manager function Kobalt also hooks, and NickelHook does not chain two mods on one
  function.
- The mod installs no hooks at all; everything goes through Nickel's exported functions and
  files, which is also why it survives Kobalt updates.

## Troubleshooting

- *The KOSync menu entry does nothing, and `log.txt` has no new "starting" line after a
  reboot*: the library was left under its fail-safe name. NickelHook protects Nickel from a
  crashing mod by renaming the library while it loads and restoring it a couple of seconds
  later; if Nickel is stopped inside that window (a hard reboot or kernel panic, or a
  launcher that replaces Nickel right after boot) the file keeps that name, Qt still loads
  it, and NickelHook refuses to start it, so the mod sits inert at every boot. Since v0.2.2
  the mod notices this at load and renames itself back, so one more restart heals it. On
  an older build, copy `KoboRoot.tgz` to `.kobo` again (settings, state and log are kept),
  or run `repair.sh` from NickelMenu with
  `menu_item :main :KOSync repair :cmd_output :9000 :sh /mnt/onboard/.adds/kosync/repair.sh`
  in a file under `.adds/nm`, then restart.

- *sign-in failed* in the log: check the username and password; the mod sends the MD5 of
  the password exactly as KOReader does. For a self-hosted server with a self-signed
  certificate set `insecure = true` or point `ca_file` at your CA.
- *N book(s) waiting for a network connection*: the device never came online. With
  `[wifi] mode = connect` the mod asks Nickel for Wi-Fi at most every ten minutes; tap
  Sync on the home screen or use the NickelMenu entry to connect right away.
- *cannot map Nickel's position*: the chapter Nickel points at was not found in the EPUB's
  spine. Only the percentage is kept in that case.
- A position from another device was not applied: with `pull = forward` it must be ahead
  of the local one; a reader with the book open also defers it. Turn on `verbose` to see
  each decision.

## Building

The Kobo package is built with [NickelTC](https://github.com/pgaskin/NickelTC):

```sh
docker run --rm -v "$PWD:/src" -w /src ghcr.io/pgaskin/nickeltc:1.0 make all strip koboroot
```

`tools/remote-build.sh [ssh-host]` does the same on a machine that has Docker and copies
`KoboRoot.tgz` to `build/`. `make test` builds and runs the host unit tests for the XHTML
position mapping, the EPUB reader and the digests; it needs a host Qt 5
(`tools/host-test.sh` runs it in a container).

## Credits

- [NickelHook](https://github.com/pgaskin/NickelHook) by Patrick Gaskin, the framework
  this mod is built on, and NickelMenu for the Wi-Fi symbols.
- [Kobalt](https://github.com/nicoverbruggen/kobalt) by Nico Verbruggen, whose
  `native_progress.cpp` and `engine_positions.cpp` document how Nickel stores positions
  and how its Volume API is driven.
- [KOReader](https://github.com/koreader/koreader) for the sync protocol and the
  partial-MD5 document id, and [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader)
  for its XPath resolver, which this mod's mapping mirrors.

MIT licence.

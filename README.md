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

- **Push.** When a book is closed, or the device goes to sleep with it open, the mod reads the position (Kobalt's XPointer, or
  Nickel's chapter position translated into an XPointer through the chapter's XHTML)
  and sends it to the server with the percentage read. Offline, it asks Nickel to turn
  Wi-Fi on and sends as soon as the device is online.
- **Pull.** Whenever the device is online it checks books against the server: the recently
  read books at start, after Nickel's own Sync, on request and every 15 minutes while
  online; a book when it is opened or the device wakes with it open; and the whole library
  from *Check whole library* on the KOSync screen. A newer position from another device is written into Kobalt's
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
4. Enter the server, username and password of the account you use in KOReader's
   *Progress sync*: either from the *KOSync* entry in NickelMenu's main menu, or by copying
   `.adds/kosync/config.sample.ini` to `config.ini` and filling in `[server]` over USB. No
   reboot is needed; the mod re-reads the file when it changes.
5. Check `.adds/kosync/log.txt` after the first sync: it should say
   `signed in to <server> as <user>`.

With [NickelMenu](https://pgaskin.net/NickelMenu/) installed, the mod adds a *KOSync* entry
to the main menu by itself (turn it off with `menu = false`). It opens the KOSync screen:
*Sync now*, *Check whole library*, the server, username and password (typed on an on-screen
keyboard), and the other settings as buttons that cycle through their values. `menu_log = true` adds a *KOSync log* entry that shows the last lines of the log.
After each sync Nickel's notification bubble lists what moved or was sent, three lines at
most ("and N more").

## Configuration

`config.sample.ini` documents every setting. The ones that matter:

| Setting | Default | Meaning |
|---|---|---|
| `[server] url` | `https://sync.koreader.rocks` | Any KOSync-compatible server |
| `[server] username`, `password` | | The account; `password_md5` can replace `password` |
| `[server] device` | `Kobo` | Device name shown on other readers |
| `[sync] document_id` | `binary` | `binary` (file content) or `filename`, as in KOReader |
| `[sync] pull` | `newer` | `newer` (newest wins), `forward` (only ahead), `off` |
| `[sync] recent_books` | `3` | Books checked per sync |
| `[sync] pull_interval_minutes` | `15` | Check interval while online |
| `[wifi] mode` | `connect` | `connect` asks Nickel for Wi-Fi when needed; `keep` never does |
| `[sync] on_open` | `true` | Check a book when opened, and on wake with it open |
| `[options] notify` | `true` | Nickel notification after a sync |
| `[options] menu`, `menu_log` | `true`, `false` | NickelMenu entries |
| `[options] verbose` | `false` | Log every decision |

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

## Uninstall

1. Connect the Kobo over USB and create an empty file named `uninstall` in `.adds/kosync`
   (next to `config.ini`).
2. Eject. On the next start the mod sees the file, deletes its library from the firmware and
   removes the `KOSync` menu entry it generated, then Nickel carries on without it.
3. Delete the `.adds/kosync` folder yourself if you also want the settings, the log and the
   state file gone. Nothing else on the device is touched: Nickel's library keeps whatever
   positions were last written, and Kobalt's files are untouched.

Installing a newer `KoboRoot.tgz` over an existing installation needs no uninstall.

## Troubleshooting

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

# Changelog

## v0.1.1 — 2026-10-11

- Uninstall also removes the generated NickelMenu entry; README documents the uninstall
  steps.

## v0.1.0 — 2026-10-11

First release.

- Pushes the reading position to a KOReader sync server when a book is closed in Kobalt or
  in Nickel's reader, or the device sleeps with it open; applies newer positions from other
  devices to both readers, also for books never opened on the Kobo.
- Positions are CREngine XPointers (exact for Kobalt; mapped through the chapter's XHTML
  for Nickel's kepub spans and Adobe points) and compared by paragraph, so a reopened
  position is not echoed back.
- A book only opened here (0%) never overrides another device's position; otherwise the
  newest wins (forward-only and off are options).
- Document ids by content (KOReader's partial MD5) or by file name.
- KOSync screen from a NickelMenu entry: sync buttons, server, username and password on an
  on-screen keyboard, and the other settings as switches and pop-up choices.
- Nickel notifications after a sync; checks when a book is opened or the device wakes.
- Asks Nickel for Wi-Fi when there is something to sync; also syncs after Nickel's own
  Sync and on a `sync-now` trigger file.
- Firmware 4.x (Qt 5) package only.

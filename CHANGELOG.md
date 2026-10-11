# Changelog

## Unreleased

- KOSync screen from the NickelMenu entry: sync buttons and on-device settings.
- Nickel notification after a sync, summarised to three lines.
- A book is checked when it is opened and when the device wakes with it open.
- A book only opened here (0%) never overrides another device's position; explicit
  syncs check the whole library; folder watches survive USB sessions.

## v0.1.0 — 2026-10-11

First release.

- Pushes the reading position to a KOReader sync server when a book is closed
  in Kobalt or in Nickel's reader; applies newer positions from other devices
  to both readers.
- Positions are CREngine XPointers (exact for Kobalt; mapped through the
  chapter's XHTML for Nickel's kepub spans and Adobe points).
- Document ids by content ("binary", KOReader's partial MD5) or by file name.
- Asks Nickel to connect to Wi-Fi when there is something to sync; also syncs
  after Nickel's own Sync and on a `sync-now` trigger file (NickelMenu sample
  included).
- Firmware 4.x (Qt 5) package only.

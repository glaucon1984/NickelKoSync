# Changelog

## Unreleased

- Shorter NickelHook fail-safe window (2 s instead of 5), so a Nickel stopped soon after
  boot is less likely to leave the mod uninstalled; README explains the symptom and the fix.

## v0.2.0 — 2026-10-12

- Plain EPUB files: after a Kobalt close, Kobalt's position is written into Nickel's
  library in Nickel's format; after a Nickel close, Nickel's position is given to Kobalt as
  an XPointer. Each reader opens where the other left off instead of at the chapter start.
- Opening a book in Nickel without moving no longer counts as a new position; positions are
  compared by paragraph, so a reopened position is not echoed back to the server.
- If the server holds a later position from this same device than the one it now has (for
  example after reinstalling Kobalt), that position is restored instead of the older one
  being sent back.
- A position parked while Nickel's reader had the book open is applied right after the
  reader closes, and a notice says the book is ready to reopen.
- Notifications are the mod's own notice box: tap to dismiss, or it disappears after a few
  seconds. Background runs that only sent positions stay quiet while a book is open.
- Standby and wake no longer trigger a sync: at standby Wi-Fi is usually off already and an
  open book cannot be moved on wake, so the behaviour was unpredictable. Close the book to
  sync; opening a book still checks and reminds. (A build between v0.1.1 and this one
  hooked the power manager to hold the sleep; that cut out Kobalt's hook on the same
  function and was withdrawn. The mod installs no hooks.)
- Whole-library check is manual only ("Check whole library"); queued books follow each
  other without waiting for the network timer.
- KOSync screen laid out like Kobo's settings, with switches, pop-up choices with
  explanations, a sans-serif keyboard page, and sync buttons at the bottom.

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

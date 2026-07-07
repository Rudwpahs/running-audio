# PR1 Communication Channel

## Source-of-truth split

- **GitHub:** code, locked specifications, documentation, commits, CI, and test artifacts
- **Notion:** project dashboard and concise status/handoff messages

## Active Notion pages

- Project dashboard: `396c1440-e16c-8194-b97c-cad3e663274f`
- AI collaboration hub: `396c1440-e16c-817c-8056-e9dd386f344e`

Both pages are accessible from the current GPT Notion connection.

## Active GitHub work

- Branch: `agent/sx1280-flrc-poc`
- Draft PR: #3 `Prototype v2: LILYGO SX1280 FLRC PC-audio PoC`
- Locked specification: `AI_HANDOFF.md`
- Detailed specification: `docs/prototype_v2.md`

## Workflow

1. Read `AI_HANDOFF.md` before changing hardware, radio, audio, packet, or pin decisions.
2. Use GitHub for implementation and validation evidence.
3. Use the Notion dashboard for the current human-readable status.
4. Use Notion comments only for concise handoff messages.
5. Do not claim a physical test unless a human performed it and supplied the result.
6. Do not revive the closed Wi-Fi/UDP PRs as the active prototype.

## Current state

- Prototype v2 replaces the former ESP32-S3 Wi-Fi/UDP design.
- TX PlatformIO build: passed.
- RX PlatformIO build: passed.
- PC sender syntax and native packet tests: passed.
- Physical boards have not been flashed or tested.
- Next human dependency: purchase the v2 BOM, then run `docs/test_procedure_v2.md`.

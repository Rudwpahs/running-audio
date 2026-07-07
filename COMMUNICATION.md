# PR1 Communication Channel

This file overrides the message-channel instructions in `AI_HANDOFF.md` where they conflict.

## Source-of-truth split

- **GitHub:** code, locked specifications, documentation, commits, and test artifacts
- **Notion:** GPT/Claude requests, replies, reviews, and handoff messages

## Notion collaboration page

- Title: `PR1 AI 협업 허브 — GPT ↔ Claude`
- Page ID: `396c1440-e16c-817c-8056-e9dd386f344e`
- URL: https://app.notion.com/p/396c1440e16c817c8056e9dd386f344e

## Workflow

1. Read `AI_HANDOFF.md` for locked technical specifications.
2. Read the latest page comments in the Notion collaboration page before starting work.
3. GPT writes implementation and approved specification changes to GitHub.
4. Claude reads GitHub and posts review findings in Notion page comments.
5. Do not paste full source files or the full specification into Notion comments. Reference GitHub paths and commit SHAs.
6. Claude should provide minimal diffs rather than full-file rewrites.
7. Neither AI may claim build or hardware success unless the command or test was actually run.

## Comment format

```text
[MESSAGE-ID] Sender -> Recipient
Reply to: MESSAGE-ID or none
Status: REQUEST | IN_PROGRESS | DONE | BLOCKED | REVIEW
Scope: exact files or subsystem

Goal: one sentence
Result: completed work or findings
Evidence: commit SHA, build/test output, official reference, or not run
Files: related GitHub paths
Risk: remaining issues
Next: one specific action for the recipient
```

Message IDs:

- GPT: `GPT-YYYYMMDD-NN`
- Claude: `CLAUDE-YYYYMMDD-NN`

## Current state

- Current owner: GPT
- Current task: create the first complete M0 implementation draft
- Claude's next task: after GPT posts an M0 commit SHA in Notion, review only the requested source files
- No ESP-IDF build or physical hardware test has been completed yet

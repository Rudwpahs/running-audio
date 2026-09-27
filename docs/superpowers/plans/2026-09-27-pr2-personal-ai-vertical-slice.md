# PR2 Personal AI / Ambient AI Vertical Slice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a software-only PR2 vertical slice that proves or falsifies the value of an ear-first Personal AI experience on an iPhone with existing Bluetooth earbuds, while measuring every major latency boundary and minimizing data sent off-device.

**Architecture:** Keep orchestration, schemas, privacy policy, memory policy, tool routing, failure semantics, and latency tracing in a provider-neutral Swift package. Put Apple Speech/AVFoundation/TTS and UI in an iOS adapter/app layer. The current GitHub account has no dedicated PR2 product repository, so this plan does not invent one and does not place PR2 product code in `Rudwpahs/running-audio`; implementation starts in a dedicated PR2 repository only after that repository actually exists or an explicitly approved alternative codebase is supplied.

**Tech Stack:** Swift 6.x, Swift Package Manager for provider-neutral core, XCTest/Swift Testing for core tests, Swift Concurrency (`async/await`, `AsyncStream`), iOS Speech framework, AVFoundation/AVAudioSession, SwiftData or SQLite for local persistent memory, `ContinuousClock`/monotonic timing, SwiftUI for the test shell.

**Spec:** `docs/superpowers/specs/2026-09-25-pr2-personal-ai-vertical-slice-design.md`

## Global Constraints

- No custom PR2 hardware, wake-word DSP, new radio, enclosure, battery, or ear-acoustic design in v0.1.
- Do not add PR2 product code to `Rudwpahs/running-audio`; that repository remains PR1.
- Primary flow: `trigger -> STT -> current context -> personal memory retrieval -> agent reasoning -> optional tool call -> response -> TTS`.
- Raw microphone audio remains local; cloud STT is not an implicit fallback.
- Long-term memory writes require explicit policy approval; whole transcripts are not automatically persisted as long-term memory.
- Cloud reasoning receives only finalized transcript, selected context, top-k memory snippets, required tool descriptors, and required tool results.
- Performance logs contain no transcript text or memory content.
- Principal interaction metric: `speech_end_to_first_audio`.
- Required separately measurable points: trigger -> STT start, STT complete, memory retrieval, model first token, tool request, tool response, TTS first audio, end-to-end.
- Fallbacks must cover network failure, STT failure, memory timeout, model timeout, tool failure/timeout, TTS failure, Bluetooth input loss, and privacy-mode cloud blocking.
- Product gate values remain those in the approved spec; tests must not claim they have been achieved before real-device traces exist.

## Repository / File Structure

The following paths are relative to the future dedicated PR2 repository. They are a file-layout contract, not a claim that the repository exists today.

```text
Package.swift
Sources/PR2Core/
  Schema/Identifiers.swift
  Schema/InteractionEvent.swift
  Schema/ConversationContext.swift
  Schema/MemoryModels.swift
  Schema/ToolModels.swift
  Schema/FailureModels.swift
  Timing/MonotonicClock.swift
  Timing/LatencyTracer.swift
  Timing/LatencyMetrics.swift
  Memory/MemoryStore.swift
  Memory/MemoryPolicy.swift
  Memory/InMemoryMemoryStore.swift
  Context/ContextProvider.swift
  Context/PrivacyGate.swift
  Tools/AgentTool.swift
  Tools/ToolRouter.swift
  Agent/Reasoner.swift
  Agent/AgentEvents.swift
  Audio/SpeechToText.swift
  Audio/SpeechOutput.swift
  Orchestration/InteractionState.swift
  Orchestration/InteractionOrchestrator.swift
  Fixtures/FakeSTT.swift
  Fixtures/FakeReasoner.swift
  Fixtures/FakeTool.swift
  Fixtures/FakeSpeechOutput.swift
Tests/PR2CoreTests/
  SchemaTests.swift
  LatencyTracerTests.swift
  MemoryPolicyTests.swift
  PrivacyGateTests.swift
  ToolRouterTests.swift
  InteractionOrchestratorTests.swift
PR2App/
  App/PR2App.swift
  Audio/AppleAudioRouteManager.swift
  Speech/AppleSpeechTranscriber.swift
  Speech/AppleSpeechOutput.swift
  Context/AppleContextProvider.swift
  Memory/PersistentMemoryStore.swift
  Tools/CalendarReadTool.swift
  Tools/NetworkReadTool.swift
  Reasoning/ReasonerAdapter.swift
  Views/MainPushToTalkView.swift
  Views/MemoryDebugView.swift
  Views/LatencyTraceView.swift
  Export/TraceExporter.swift
PR2AppTests/
  AdapterContractTests.swift
  PrivacyEnvelopeTests.swift
  FallbackIntegrationTests.swift
docs/
  DEVICE_TEST_MATRIX.md
  PRIVACY_BOUNDARY.md
  LATENCY_SCHEMA.md
```

## Review Focus

- Bluetooth headset disappears between trigger and speech end: fall back to iPhone mic without silently losing the trace or inventing success.
- Memory search exceeds deadline: continue with `memory_status = unavailable`, not `no_relevant_hit`, and never fabricate a memory.
- Cloud model times out after STT and memory succeed: terminate the model stage deterministically and do not invoke a tool or TTS with fabricated content.
- A tool returns failure or times out after the model requested it: feed a structured failure to the reasoner only if another bounded model turn is allowed; otherwise fail/partial-answer truthfully.
- TTS fails after a correct text response exists: preserve the response text and trace, expose `tts_failed`, and never mark the interaction as audio-successful.

---

### Task 1: Core schemas and message contracts

**Files:**
- Create: `Sources/PR2Core/Schema/Identifiers.swift`
- Create: `Sources/PR2Core/Schema/InteractionEvent.swift`
- Create: `Sources/PR2Core/Schema/ConversationContext.swift`
- Create: `Sources/PR2Core/Schema/MemoryModels.swift`
- Create: `Sources/PR2Core/Schema/ToolModels.swift`
- Create: `Sources/PR2Core/Schema/FailureModels.swift`
- Test: `Tests/PR2CoreTests/SchemaTests.swift`

**Interfaces:**
- Produces: `TraceID`, `SessionID`, `TurnID`, `Transcript`, `ConversationContext`, `CurrentContext`, `MemoryHit`, `MemoryStatus`, `ToolDescriptor`, `ToolCall`, `ToolResult`, `FailureStage`, `FailureReason`, `FallbackAction`, `InteractionEvent`.
- Consumes: Foundation only.

- [ ] **Step 1: Write schema tests**

Pin these requirements in `SchemaTests`: IDs are strongly typed; `MemoryStatus` distinguishes `.available`, `.noRelevantHit`, `.unavailable`; `ToolDescriptor` contains execution location, privacy scope, risk level and timeout; failures distinguish STT, memory, model, tool, TTS, network and audio-route stages.

- [ ] **Step 2: Run tests and verify failure**

Run: `swift test --filter SchemaTests`

Expected: FAIL because schema types do not exist.

- [ ] **Step 3: Implement the minimal schema types**

Use value types and enums. Do not add provider-specific SDK types to PR2Core.

- [ ] **Step 4: Run schema tests**

Run: `swift test --filter SchemaTests`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add Sources/PR2Core/Schema Tests/PR2CoreTests/SchemaTests.swift
git commit -m "feat: define PR2 interaction schemas"
```

### Task 2: Monotonic latency instrumentation

**Files:**
- Create: `Sources/PR2Core/Timing/MonotonicClock.swift`
- Create: `Sources/PR2Core/Timing/LatencyTracer.swift`
- Create: `Sources/PR2Core/Timing/LatencyMetrics.swift`
- Test: `Tests/PR2CoreTests/LatencyTracerTests.swift`
- Create: `docs/LATENCY_SCHEMA.md`

**Interfaces:**
- Produces: `protocol MonotonicClockSource { func nowNanos() -> UInt64 }`, `LatencyMarkName`, `LatencyMark`, `LatencyTrace`, `LatencyTracer.mark(_:)`, `LatencyMetrics.derive(from:)`.
- Consumes: `TraceID` from Task 1.

- [ ] **Step 1: Write failing latency tests**

Assert independent derivation of: `trigger_to_stt_start`, `speech_end_to_stt_final`, `memory_retrieval`, `model_ttft`, `model_to_tool_request`, `tool_rtt`, `tts_ttfa`, `speech_end_to_first_audio`, `trigger_to_first_audio`, and `end_to_end`. Assert missing marks yield an unavailable metric rather than zero. Assert marks cannot expose transcript or memory text fields.

- [ ] **Step 2: Run tests and verify failure**

Run: `swift test --filter LatencyTracerTests`

Expected: FAIL because tracer/metrics do not exist.

- [ ] **Step 3: Implement monotonic tracing**

Use injected clock values in nanoseconds. `LatencyTrace` stores mark name, time, and non-sensitive metadata only.

- [ ] **Step 4: Run latency tests**

Run: `swift test --filter LatencyTracerTests`

Expected: PASS.

- [ ] **Step 5: Document event names and derived metrics**

`docs/LATENCY_SCHEMA.md` must list exact marks: `trigger`, `stt_start`, `speech_start`, `speech_end`, `stt_final`, `memory_start`, `memory_end`, `context_start`, `context_end`, `model_request`, `model_first_token`, indexed `tool_request`/`tool_response`, `tts_request`, `tts_first_audio`, `response_complete`, plus terminal failure/interruption status.

- [ ] **Step 6: Commit**

```bash
git add Sources/PR2Core/Timing Tests/PR2CoreTests/LatencyTracerTests.swift docs/LATENCY_SCHEMA.md
git commit -m "feat: add PR2 latency tracing"
```

### Task 3: Short-term / long-term memory policy and retrieval abstraction

**Files:**
- Create: `Sources/PR2Core/Memory/MemoryStore.swift`
- Create: `Sources/PR2Core/Memory/MemoryPolicy.swift`
- Create: `Sources/PR2Core/Memory/InMemoryMemoryStore.swift`
- Test: `Tests/PR2CoreTests/MemoryPolicyTests.swift`

**Interfaces:**
- Produces: `MemoryScope.shortTerm`, `MemoryScope.longTerm`, `MemoryQuery`, `MemoryCandidate`, `MemorySearchResult`, `MemoryStore.search(_:policy:) async`, `MemoryStore.write(_:policy:) async throws`.
- Consumes: memory schema types from Task 1 and monotonic deadlines from Task 2.

- [ ] **Step 1: Write failing memory tests**

Assert: recent-turn short-term data and long-term items are separable; explicit long-term write succeeds when policy allows it; implicit whole-transcript long-term write is rejected; empty search returns `.noRelevantHit`; deadline expiry returns `.unavailable`; top-k is deterministic and bounded.

- [ ] **Step 2: Run tests and verify failure**

Run: `swift test --filter MemoryPolicyTests`

Expected: FAIL.

- [ ] **Step 3: Implement memory interfaces and deterministic in-memory store**

Use simple text/token scoring for the reference store. Do not add embeddings/vector DB in v0.1.

- [ ] **Step 4: Run memory tests**

Run: `swift test --filter MemoryPolicyTests`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add Sources/PR2Core/Memory Tests/PR2CoreTests/MemoryPolicyTests.swift
git commit -m "feat: add PR2 memory policy and retrieval"
```

### Task 4: Current context and privacy-minimized cloud envelope

**Files:**
- Create: `Sources/PR2Core/Context/ContextProvider.swift`
- Create: `Sources/PR2Core/Context/PrivacyGate.swift`
- Test: `Tests/PR2CoreTests/PrivacyGateTests.swift`
- Create: `docs/PRIVACY_BOUNDARY.md`

**Interfaces:**
- Produces: `ContextRequest`, `ContextSnapshot`, `PrivacyMode`, `CloudReasoningEnvelope`, `PrivacyGate.minimize(transcript:context:memory:tools:)`.
- Consumes: transcript, context, memory hits, tool descriptors from Tasks 1-3.

- [ ] **Step 1: Write failing privacy tests**

Assert default cloud envelope contains only finalized transcript, selected context fields, top-k memory snippets and required tool descriptors. Assert it has no raw audio field, full memory-store field, full calendar field, contacts field, GPS-history field, or transcript-bearing telemetry field. Assert privacy mode can block cloud reasoning entirely.

- [ ] **Step 2: Run tests and verify failure**

Run: `swift test --filter PrivacyGateTests`

Expected: FAIL.

- [ ] **Step 3: Implement context contracts and allowlist-based privacy minimization**

Use explicit allowlists; do not redact by best-effort regex after building an oversized payload.

- [ ] **Step 4: Run privacy tests**

Run: `swift test --filter PrivacyGateTests`

Expected: PASS.

- [ ] **Step 5: Document local/cloud boundary**

`docs/PRIVACY_BOUNDARY.md` must list local-only, cloud-eligible, permission-gated, and prohibited-by-default data classes.

- [ ] **Step 6: Commit**

```bash
git add Sources/PR2Core/Context Tests/PR2CoreTests/PrivacyGateTests.swift docs/PRIVACY_BOUNDARY.md
git commit -m "feat: add PR2 privacy boundary"
```

### Task 5: Tool abstraction, risk policy, timeout and failure semantics

**Files:**
- Create: `Sources/PR2Core/Tools/AgentTool.swift`
- Create: `Sources/PR2Core/Tools/ToolRouter.swift`
- Create: `Sources/PR2Core/Fixtures/FakeTool.swift`
- Test: `Tests/PR2CoreTests/ToolRouterTests.swift`

**Interfaces:**
- Produces: `protocol AgentTool`, `ToolRouter.execute(_:) async -> ToolExecutionResult`.
- Consumes: `ToolDescriptor`, `ToolCall`, `ToolResult`, failure types and privacy mode.

- [ ] **Step 1: Write failing tool-router tests**

Assert local read success; external read can be privacy-blocked; unsupported sensitive/destructive tool is rejected; timeout returns structured timeout; tool exception returns structured failure; tool request and response latency marks are separate.

- [ ] **Step 2: Run tests and verify failure**

Run: `swift test --filter ToolRouterTests`

Expected: FAIL.

- [ ] **Step 3: Implement tool protocol and router**

The router validates descriptor, risk, privacy mode and timeout before invoking the tool.

- [ ] **Step 4: Run tool tests**

Run: `swift test --filter ToolRouterTests`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add Sources/PR2Core/Tools Sources/PR2Core/Fixtures/FakeTool.swift Tests/PR2CoreTests/ToolRouterTests.swift
git commit -m "feat: add PR2 tool router"
```

### Task 6: Reasoner/STT/TTS contracts and deterministic fake adapters

**Files:**
- Create: `Sources/PR2Core/Agent/Reasoner.swift`
- Create: `Sources/PR2Core/Agent/AgentEvents.swift`
- Create: `Sources/PR2Core/Audio/SpeechToText.swift`
- Create: `Sources/PR2Core/Audio/SpeechOutput.swift`
- Create: `Sources/PR2Core/Fixtures/FakeSTT.swift`
- Create: `Sources/PR2Core/Fixtures/FakeReasoner.swift`
- Create: `Sources/PR2Core/Fixtures/FakeSpeechOutput.swift`
- Test: `Tests/PR2CoreTests/AdapterContractTests.swift`

**Interfaces:**
- Produces: `SpeechToText`, `Reasoner`, `SpeechOutput`, deterministic fake adapters and `AgentEvent` cases for first token, text delta, tool request, completion and failure.
- Consumes: schemas, privacy envelope and latency tracer from prior tasks.

- [ ] **Step 1: Write failing adapter contract tests**

Assert fake STT can emit start/final/failure; fake reasoner can emit first-token then direct answer or tool request; fake model timeout is distinguishable from network-unavailable; fake speech output can emit first-audio then completion or TTS failure.

- [ ] **Step 2: Run tests and verify failure**

Run: `swift test --filter AdapterContractTests`

Expected: FAIL.

- [ ] **Step 3: Implement contracts and fakes**

Do not import Apple-only frameworks into PR2Core.

- [ ] **Step 4: Run adapter tests**

Run: `swift test --filter AdapterContractTests`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add Sources/PR2Core/Agent Sources/PR2Core/Audio Sources/PR2Core/Fixtures Tests/PR2CoreTests/AdapterContractTests.swift
git commit -m "feat: add PR2 agent and audio contracts"
```

### Task 7: Failure/fallback state machine and end-to-end orchestrator

**Files:**
- Create: `Sources/PR2Core/Orchestration/InteractionState.swift`
- Create: `Sources/PR2Core/Orchestration/InteractionOrchestrator.swift`
- Test: `Tests/PR2CoreTests/InteractionOrchestratorTests.swift`

**Interfaces:**
- Produces: `InteractionState`, `InteractionOutcome`, `InteractionOrchestrator.run(trigger:) async -> InteractionOutcome`.
- Consumes: all contracts from Tasks 1-6.

- [ ] **Step 1: Write failing happy-path tests**

Cover direct question with no memory/tool, memory-dependent question, explicit memory write then retrieval, and tool-call question. Assert mark ordering and final outcome.

- [ ] **Step 2: Write failing fallback tests**

Cover: network failure -> local-only or explicit unavailable result; STT failure -> no reasoning call; memory timeout -> continue with `.unavailable`; model timeout -> no tool/TTS fabrication; tool failure/timeout -> structured failure path; TTS failure -> preserve response text and mark audio failure; Bluetooth route loss -> route fallback event and continued trace; privacy mode -> cloud reasoner blocked.

- [ ] **Step 3: Run tests and verify failure**

Run: `swift test --filter InteractionOrchestratorTests`

Expected: FAIL.

- [ ] **Step 4: Implement the minimal orchestrator**

State sequence: `idle -> triggered -> listening -> transcribing -> enriching -> reasoning -> optionalTool -> speaking -> completed/failed/interrupted`. Context and memory may execute concurrently after final transcript if their query inputs permit it. Every terminal path closes the trace exactly once.

- [ ] **Step 5: Run orchestrator tests**

Run: `swift test --filter InteractionOrchestratorTests`

Expected: PASS.

- [ ] **Step 6: Run complete core suite**

Run: `swift test`

Expected: all PR2Core tests PASS.

- [ ] **Step 7: Commit**

```bash
git add Sources/PR2Core/Orchestration Tests/PR2CoreTests/InteractionOrchestratorTests.swift
git commit -m "feat: orchestrate PR2 vertical slice"
```

### Task 8: iOS audio/STT/TTS adapters and push-to-talk shell

**Files:**
- Create: `PR2App/App/PR2App.swift`
- Create: `PR2App/Audio/AppleAudioRouteManager.swift`
- Create: `PR2App/Speech/AppleSpeechTranscriber.swift`
- Create: `PR2App/Speech/AppleSpeechOutput.swift`
- Create: `PR2App/Views/MainPushToTalkView.swift`
- Test: `PR2AppTests/AdapterContractTests.swift`
- Test: `PR2AppTests/FallbackIntegrationTests.swift`

**Interfaces:**
- Produces: foreground push-to-talk trigger, AVAudioSession route adapter, on-device Speech adapter when supported, local TTS adapter.
- Consumes: PR2Core contracts.

- [ ] **Step 1: Add iOS adapter contract tests with injected wrappers**

Assert route metadata is reported without embedding AVFoundation types in PR2Core; unsupported speech model maps to structured STT-unavailable; route loss can select iPhone mic; TTS error maps to `.tts` failure.

- [ ] **Step 2: Run iOS tests on the selected simulator/device target**

Expected: FAIL until adapters exist.

- [ ] **Step 3: Implement Apple adapters**

Use `AVAudioSession` and Apple Speech runtime capability checks. Do not send raw audio to a cloud STT fallback. Use local speech synthesis by default.

- [ ] **Step 4: Implement minimal SwiftUI push-to-talk screen**

Show trigger/listening state, live/final transcript, current audio route, compact response text, privacy-mode status and latest latency summary. No visual redesign work.

- [ ] **Step 5: Run app/unit tests**

Expected: PASS on supported Xcode/iOS environment.

- [ ] **Step 6: Commit**

```bash
git add PR2App PR2AppTests
git commit -m "feat: add PR2 iPhone audio vertical slice"
```

### Task 9: Persistent memory, context adapters, tools and reasoner adapter

**Files:**
- Create: `PR2App/Memory/PersistentMemoryStore.swift`
- Create: `PR2App/Context/AppleContextProvider.swift`
- Create: `PR2App/Tools/CalendarReadTool.swift`
- Create: `PR2App/Tools/NetworkReadTool.swift`
- Create: `PR2App/Reasoning/ReasonerAdapter.swift`
- Test: `PR2AppTests/PrivacyEnvelopeTests.swift`
- Test: `PR2AppTests/FallbackIntegrationTests.swift`

**Interfaces:**
- Produces: persistent local memory adapter, demand-driven current-context adapter, read-only tool fixtures, provider-neutral reasoner adapter.
- Consumes: PR2Core memory/context/tool/reasoner contracts.

- [ ] **Step 1: Write adapter integration tests**

Assert memory persists only allowed candidates; calendar is not read unless requested by tool/context intent; network tool failure is structured; cloud request payload is privacy-minimized; no long-lived model API key is compiled into the app.

- [ ] **Step 2: Run tests and verify failure**

Expected: FAIL until adapters exist.

- [ ] **Step 3: Implement local persistence and context adapters**

Use SwiftData or SQLite text search first; do not add embeddings.

- [ ] **Step 4: Implement two read-only tools and the reasoner adapter**

Initial tools: calendar read and one network read fixture. Provider secret, if needed, lives behind a relay/secure credential flow rather than in the app binary.

- [ ] **Step 5: Run tests**

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add PR2App/Memory PR2App/Context PR2App/Tools PR2App/Reasoning PR2AppTests
git commit -m "feat: connect PR2 memory context and tools"
```

### Task 10: Trace export, device matrix and product-value test gate

**Files:**
- Create: `PR2App/Views/MemoryDebugView.swift`
- Create: `PR2App/Views/LatencyTraceView.swift`
- Create: `PR2App/Export/TraceExporter.swift`
- Create: `docs/DEVICE_TEST_MATRIX.md`
- Test: `PR2AppTests/PrivacyEnvelopeTests.swift`

**Interfaces:**
- Produces: privacy-safe trace export and repeatable device acceptance matrix.
- Consumes: latency traces and interaction outcomes.

- [ ] **Step 1: Write privacy-safe export test**

Assert exported trace includes trace ID, audio route, STT engine, reasoner ID, network class, tool name, cache hit, durations, error code and completion status, and excludes transcript/memory text.

- [ ] **Step 2: Implement trace viewer/export**

Allow per-turn inspection plus JSON/CSV export without sensitive content.

- [ ] **Step 3: Write device test matrix**

Include: iPhone mic; one generic Bluetooth HFP headset; AirPods/high-quality route when available as optional comparison; Wi-Fi; cellular/network-limited/offline; direct question; memory question; memory write; tool call; network failure; STT failure fixture; memory timeout fixture; model timeout fixture; tool failure; TTS failure; Bluetooth loss; privacy mode.

- [ ] **Step 4: Run complete automated suite**

Run core `swift test` plus Xcode test suite on a Mac/iOS environment.

Expected: all automated tests PASS before any product-value claim.

- [ ] **Step 5: Run real-device measurement session**

Collect at least 30 representative successful no-tool turns and 30 tool-path turns before computing p50/p95 latency. Record task completion and ear-first-vs-phone choice separately from latency.

- [ ] **Step 6: Evaluate hardware gate**

Only open custom-hardware work if repeated-use value is demonstrated and the dominant residual friction is hardware-specific: trigger, microphone readiness/quality, wear comfort, leakage/privacy, battery or form factor.

- [ ] **Step 7: Commit**

```bash
git add PR2App/Views PR2App/Export docs/DEVICE_TEST_MATRIX.md PR2AppTests
git commit -m "test: add PR2 device and value gate"
```

## Plan Self-Review Result

- Spec coverage: architecture, module interfaces, event/message schema, context, short/long memory split, retrieval abstraction, tool abstraction, local/cloud boundary, minimal transmission, failure/fallback state machine, latency instrumentation, UI/debugging, tests and hardware gate are all mapped to tasks.
- Type consistency: core identifiers/schemas feed timing, memory, context, tools, adapters and orchestrator with one provider-neutral boundary.
- Review Focus: Bluetooth loss, memory deadline, model timeout, tool failure and TTS failure are each assigned explicit tests.
- Scope: product code remains isolated from PR1; iOS-specific work starts only after a real dedicated PR2 repository/codebase is available.
- Evidence boundary: no latency target or hardware value is treated as measured until real-device traces/user trials exist.

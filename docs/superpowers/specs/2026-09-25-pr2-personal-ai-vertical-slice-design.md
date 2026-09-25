# PR2 Personal AI / Ambient AI Vertical Slice — Design

Date: 2026-09-25
Status: approved conversational design, written-spec review pending
Scope: software-only vertical slice using an iPhone and existing Bluetooth earbuds

## 1. Purpose

PR2 v0.1 exists to test one product hypothesis before custom hardware work begins:

> Is a screen-light, ear-first personal AI interaction valuable enough that a person repeatedly chooses it over opening the phone?

The prototype must validate the experience, not prove a custom earbud design. It therefore uses an iPhone, existing Bluetooth earbuds, and an explicit trigger. Custom microphones, custom radios, always-on wake-word hardware, battery design, ear acoustics, and industrial design are outside this version.

## 2. Success criteria

The vertical slice is successful only if it can execute the complete path below on a real iPhone and produce traceable latency measurements:

`trigger -> STT -> current context -> personal memory retrieval -> agent reasoning -> optional tool call -> response -> TTS`

The implementation must also demonstrate deterministic fallback behavior when network, Bluetooth input, memory retrieval, or a tool fails.

### Product gate metrics

The first user-test gate is:

- task completion rate >= 80%
- memory-dependent task correctness >= 80%
- zero critical privacy surprises or unintended long-term-memory writes
- no-tool `speech_end -> first_audio` p50 <= 1.5 s
- no-tool `speech_end -> first_audio` p95 <= 3.0 s
- tool-path `speech_end -> first_audio` p50 <= 2.0 s for a fast local/network tool mix
- in representative situations, ear-first interaction is chosen instead of opening the phone in >= 60% of trials

These are engineering/product acceptance targets, not claimed current performance.

## 3. Platform and compatibility

The primary client is a native Swift iPhone application.

Speech input uses Apple Speech framework's `SpeechAnalyzer` with `SpeechTranscriber` when available. The design must check model and locale support at runtime and fall back to `DictationTranscriber` or a pluggable alternate STT adapter if required.

Bluetooth microphone input uses `AVAudioSession` with HFP-compatible input where supported. High-quality AirPods recording may be enabled on supported devices, but PR2 v0.1 must not require AirPods-specific behavior.

TTS uses an on-device speech output adapter by default. The output adapter must remain replaceable so a streaming cloud voice can be tested later without changing orchestration code.

## 4. Architecture

```text
TriggerSource
    |
    v
AudioCapture / AudioRouteManager
    |
    v
SpeechToText ----------------------+
    |                              |
    v                              |
FinalTranscript                    |
    |                              |
    +----> ContextProvider --------+----> ContextMinimizer / PrivacyGate
    |                              |
    +----> MemoryStore ------------+
                                   |
                                   v
                              AgentInput
                                   |
                                   v
                                Reasoner
                                   |
                          +--------+--------+
                          |                 |
                          v                 v
                    direct answer        ToolRouter
                                            |
                                            v
                                         AgentTool
                                            |
                                            +----> ToolResult ----+
                                                                  |
                                   +------------------------------+
                                   v
                              ResponseComposer
                                   |
                                   v
                              SpeechOutput
                                   |
                                   v
                              Earbuds / iPhone

All major boundaries emit LatencyTracer marks under one trace_id.
```

The orchestration layer owns sequence and policy. STT, memory, reasoning, tools, and TTS are adapters behind interfaces so implementation providers can change independently.

## 5. Module interfaces

The implementation should preserve the following conceptual Swift interfaces. Exact type names may change only if the implementation plan identifies a concrete language/API constraint.

```swift
protocol TriggerSource {
    var events: AsyncStream<TriggerEvent> { get }
}

protocol SpeechToText {
    func transcribe(_ audio: AudioStream) -> AsyncStream<TranscriptEvent>
}

protocol ContextProvider {
    func snapshot(for intent: IntentHint?) async -> ContextSnapshot
}

protocol MemoryStore {
    func search(_ query: String, policy: MemoryPolicy) async -> [MemoryHit]
    func write(_ candidate: MemoryCandidate, policy: MemoryPolicy) async throws
}

protocol Reasoner {
    func run(_ input: AgentInput, tools: [ToolDescriptor]) -> AsyncStream<AgentEvent>
}

protocol AgentTool {
    var descriptor: ToolDescriptor { get }
    func call(_ arguments: ToolArguments) async throws -> ToolResult
}

protocol SpeechOutput {
    func speak(_ stream: AsyncStream<String>) -> AsyncStream<AudioOutputEvent>
}

protocol TraceSink {
    func mark(_ event: LatencyEvent)
}
```

## 6. Trigger behavior

PR2 v0.1 uses an explicit foreground trigger, preferably push-to-talk.

Reasons:

- it isolates the value of the AI experience from wake-word accuracy and always-on audio policy
- it avoids prematurely introducing background execution constraints
- it gives a precise `t_trigger` timestamp
- it keeps privacy expectations obvious during early user testing

A custom wake word is not part of this implementation. A future wake-word gate opens only after the core experience passes the product gate.

## 7. Conversation context model

Each interaction has a durable `trace_id`, session identity, and turn identity.

```text
SessionContext
- session_id
- turn_id
- locale
- privacy_mode
- recent_turns
- rolling_summary

CurrentContext
- timestamp
- timezone
- audio_route
- network_state
- optional foreground-app context
- optional location, only when explicitly required and permissioned
- optional calendar context, only when explicitly required and permissioned
```

Context collection is demand-driven. The system must not attach full calendar, contacts, location history, or unrelated sensor data to every model request.

## 8. Memory design

### 8.1 Short-term memory

Short-term memory contains the active session's recent turns and a rolling summary.

Properties:

- local by default
- bounded by a fixed recent-turn window plus summary
- intended lifetime: current session to approximately one day
- no raw audio storage requirement
- safe to clear without affecting long-term personal memory

### 8.2 Long-term memory

Long-term memory contains stable preferences, named relationships, persistent project facts, explicit commitments, and user-approved reusable context.

PR2 v0.1 must not automatically convert the entire transcript into long-term memory.

A memory write is permitted only when at least one of these is true:

1. the person explicitly asks to remember something;
2. a deterministic rule classifies the statement as an explicit persistent preference/commitment and the product flow makes the write visible;
3. a future approved memory policy expands the rule.

The initial store should use SwiftData or SQLite with text/metadata search. Embeddings and a vector database are intentionally deferred until text retrieval has demonstrated a measurable recall limitation.

### 8.3 Memory retrieval payload

Retrieval happens on-device before model transmission. The reasoner receives only the top relevant snippets and metadata, not the entire memory store.

Each hit contains at minimum:

- `memory_id`
- `kind`
- `snippet`
- `relevance`
- `created_at`
- `source_confidence`

## 9. Local/cloud boundary

### Local by default

The following stay on the iPhone unless a future explicitly approved mode changes the boundary:

- microphone capture before STT
- raw audio
- STT when supported by on-device Speech framework
- memory database
- memory retrieval
- context filtering and redaction
- latency logs
- TTS by default

### Cloud-eligible

Only the minimum packet required for reasoning may leave the device:

- finalized user transcript
- explicitly selected context fields
- top-k memory snippets
- tool descriptors needed by the current request
- tool result needed to complete the response

The cloud request must not automatically contain raw audio, the full conversation archive, the full memory database, full calendar, contacts, GPS history, or transcript-bearing performance logs.

## 10. Reasoner abstraction

`Reasoner` must be provider-neutral.

The preferred architecture is a hybrid local-first path:

- local STT
- local privacy/context/memory stage
- pluggable reasoning provider
- local tools where practical
- local TTS

The reasoner adapter may later support:

- Apple on-device Foundation Models
- Apple Private Cloud Compute where eligible
- a third-party server model through a relay
- another on-device model

The application must not embed a long-lived cloud API key. Any third-party model requiring a secret uses a minimal relay service or another secure provider-specific credential flow.

## 11. Tool abstraction and safety

Each tool exposes a typed descriptor:

```text
ToolDescriptor
- name
- description
- input_schema
- execution_location: local | cloud
- privacy_scope
- risk_level
- timeout_ms
```

Risk levels for v0.1 are:

- `read_local`
- `read_external`
- `write_reversible`
- `sensitive_or_destructive`

Initial vertical-slice tools are intentionally small:

1. local memory search
2. local memory write
3. current time/basic device context
4. calendar read adapter
5. one network read tool suitable for latency testing

Sensitive or destructive actions are not needed to validate PR2 v0.1 and should remain disabled.

## 12. Agent flow

The orchestrator executes this sequence:

1. receive trigger and create `trace_id`
2. configure/confirm audio route
3. start capture and STT
4. detect speech end and finalize transcript
5. in parallel where possible, collect safe current context and retrieve memory
6. minimize/redact context according to policy
7. create `AgentInput`
8. invoke reasoner
9. if a tool is requested, validate tool/risk/policy, execute with timeout, and return result to reasoner
10. stream response text as soon as it is safe to speak
11. start TTS before the complete textual answer is finished when supported
12. close trace with completion or explicit failure status

## 13. Failure and fallback flow

Failures must be observable and never silently fabricated.

### Bluetooth input unavailable

- attempt iPhone microphone fallback
- record route change in trace metadata
- provide a short audible/visible status when the fallback materially changes behavior

### STT unsupported or model asset unavailable

- use configured alternate transcriber when available
- otherwise fail the turn with a clear local message
- never send raw audio to a cloud STT service without a separately approved privacy mode

### Memory retrieval failure

- continue with `memory_status = unavailable`
- do not imply that no memory exists

### No relevant memory

- continue with `memory_status = no_relevant_hit`
- do not invent a memory

### Network unavailable

- local-only capabilities remain available
- cloud reasoner/tools report unavailable
- if an on-device reasoner is configured, it may handle the turn

### Tool timeout/error

- terminate the tool call at its configured deadline
- expose structured error back to the reasoner
- allow a partial answer only if it can be truthful without the missing result

### Audio interruption

- close current trace as `interrupted`
- release/restore audio session predictably

### Privacy mode

When privacy mode is active:

- cloud reasoner disabled unless the mode explicitly allows it
- long-term-memory writes disabled by default
- context sources beyond the current turn require explicit policy permission

## 14. Latency instrumentation

Timing uses a monotonic clock and one `trace_id` per interaction.

Required marks:

```text
t_trigger
t_stt_start
t_speech_start
t_speech_end
t_stt_final

t_memory_start
t_memory_end

t_context_start
t_context_end

t_model_request
t_model_first_token

t_tool_request[n]
t_tool_response[n]

t_tts_request
t_tts_first_audio

t_response_complete
```

Derived metrics include:

- `trigger_to_stt_start`
- `speech_end_to_stt_final`
- `memory_retrieval`
- `context_collection`
- `model_ttft`
- `model_to_tool_request`
- `tool_rtt`
- `tts_ttfa`
- `speech_end_to_first_audio`
- `trigger_to_first_audio`
- `end_to_end`

The principal interaction metric is `speech_end_to_first_audio`, because total trigger-to-response time is dominated by how long the person speaks.

Performance logs must not contain transcript text or memory content. A trace may include:

```text
trace_id
audio_route
stt_engine
reasoner_id
network_class
tool_name
cache_hit
duration_ms
error_code
completion_status
```

## 15. Initial latency budget

These are prototype targets to test, not measured claims:

| Segment | Initial target |
| --- | ---: |
| trigger -> STT start | 50-200 ms warm |
| speech end -> final STT | 100-350 ms |
| local memory retrieval | 5-30 ms |
| model first token | 250-900 ms for a responsive remote model path |
| local tool response | <50 ms |
| typical network tool response | 150-1000 ms |
| TTS request -> first audio | 80-250 ms |
| speech end -> first audio, no tool | 0.7-1.8 s desired range |
| speech end -> first audio, network tool | 1.1-3.0 s desired range |

## 16. Prototype UI

The UI is intentionally functional rather than polished.

Required screens/components:

- one main push-to-talk surface
- live/final transcript status
- current audio route
- compact response text for debugging
- privacy mode indicator
- local memory inspection/reset screen for testability
- latency trace viewer/export
- developer toggle for reasoner adapter and selected tool fixtures

UI minimalization and industrial design are outside this version.

## 17. Test strategy

### Unit tests

- memory retrieval ranking and empty-hit semantics
- memory-write policy
- context minimization/redaction
- tool-risk policy and timeouts
- trace timestamp ordering and metric derivation
- fallback state machine

### Adapter tests

- deterministic fake STT stream
- deterministic fake reasoner with direct-answer and tool-call events
- fake tool success/timeout/failure
- fake TTS first-audio event

### End-to-end simulator tests

At minimum:

1. direct question, no memory/tool
2. memory-dependent question
3. explicit memory write then retrieval
4. tool-call question
5. offline path
6. Bluetooth route loss with microphone fallback
7. privacy-mode cloud block
8. tool timeout with truthful partial/failure response

### Real-device tests

Run on an actual iPhone with:

- iPhone microphone
- one generic Bluetooth earbud/headset HFP route
- AirPods/high-quality route when available, as an additional comparison only
- Wi-Fi and cellular/network-limited conditions

For every scenario, export latency traces and record success/failure separately from subjective quality.

## 18. Evidence boundary

PR2 v0.1 must not claim:

- that custom PR2 hardware is necessary
- that the system is truly always-on
- that a wake word works reliably
- that local memory retrieval is superior to a vector database before measurement
- that target latency has been achieved before device traces exist
- that generic Bluetooth audio quality represents the eventual PR2 hardware ceiling

## 19. Hardware gate

Custom PR2 hardware work begins only after the software experience passes enough of the product gate to identify a hardware-specific bottleneck.

A hardware gate is justified when user tests show recurring value but the dominant remaining friction is one or more of:

- phone interaction required to trigger
- microphone readiness/quality
- long-term wear comfort
- audio leakage/privacy
- battery life
- form factor

Until then, new custom ear hardware is out of scope.

## 20. Repository boundary

`Rudwpahs/running-audio` is PR1 and must not become the PR2 application repository.

This document is committed only on the isolated documentation branch `docs/pr2-pr3-alpha-specs-20260925` so the approved design has a reviewable Git record. The future PR2 product code must live in a dedicated repository or another explicitly approved codebase. No PR2 product code is to be merged into PR1 `main` as part of this work.

## 21. Implementation-plan boundary

The implementation plan following approval of this written spec may include:

- creation/scaffolding of a dedicated Swift app once a suitable repository exists
- protocol/data-model definitions
- fake adapters and deterministic tests first
- real SpeechAnalyzer/AVAudioSession adapters
- local memory store
- local context/privacy gate
- reasoner and tool router
- TTS adapter
- latency tracing and export
- real-device acceptance procedure

It must not include custom hardware, wake-word DSP, CGH/AR work, or PR1 RF changes.

## 22. Current technology basis verified 2026-09-25

The design is consistent with current Apple platform capabilities verified before this spec was written:

- `SpeechAnalyzer` manages asynchronous speech analysis and `SpeechTranscriber` provides general-purpose transcription.
- Apple describes the newer SpeechTranscriber model as on-device and suitable for live/long-form conversational transcription.
- `AVAudioSession.CategoryOptions.allowBluetoothHFP` exposes HFP devices for input with record/play-and-record categories.
- supported AirPods can opt into Bluetooth high-quality recording through AVFoundation capture-session configuration.
- Foundation Models currently supports language-model abstraction and tool calling, including on-device and provider-pluggable paths, but PR2 does not require Apple Intelligence availability for the entire architecture.

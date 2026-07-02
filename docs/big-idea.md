# The Big Idea

ImGuiAppLayer is a claim about structure: an application is one **module** among independent
modules — worker threads, async IO, external processes — and a frame is one pass of a
**safety-critical control loop**: ingest what the world reports, handle what it asks, publish what
you are, then render — without mutating anything. Make that explicit and user code falls into
orthogonal pieces a machine can reason about, mirror, and generate.

## The pipeline

Four layers run in fixed order every frame, each one phase of the loop:

| Layer | Phase | Contract |
|---|---|---|
| **Task** | *Ingest & update* | Gather the status of independent modules (a worker thread's file IO, an async job) and update app state: every control's `OnUpdate` runs here, in dependency order — the sole mutator of persistent state. |
| **Command** | *Handle intents* | Collect commands the app received — from its own controls (`OnGetCommand`) or, eventually, other modules — dedup, dispatch once each. |
| **Status** | *Publish* | Set the app's own status for other modules to query. (Today that manifests as the status bar; the queryable model is the endpoint.) |
| **Window** | *Render* | Presentation only. All state has been collected — render the current model of the world, mutating **nothing**. The name is Dear ImGui heritage; spiritually it is the Present phase. |

Task runs before Command, so state decided this frame commands this frame. Render runs last and is
pure. The order is not convention — it is the semantics generated code compiles against.

## The phase fix

Immediate-mode UI has an intrinsic wrinkle: input state arrives *during rendering*, so a naive
imgui app mixes state from multiple loop iterations. ImGuiAppLayer leverages that ephemerality
instead of fighting it. Each control keeps a three-part memory:

- **PersistData** — durable state. One writer: `OnUpdate`.
- **TempData** — this frame's raw input, recorded by `OnRender`, zeroed every frame. Small
  primitive copies, cache-hot, nearly free.
- **LastTempData** — one frame of memory, maintained by the framework.

Processing is deferred one frame: `OnUpdate` receives *both* temp buffers and compares them.
That single skew restores the strict in-phase sequence — take input, update world, render world —
and makes **events derived, never stored**:

```c++
if (temp_data->hovered ^ last_temp_data->hovered)   // it changed
if (temp_data->pressed && !last_temp_data->pressed) // it started (rising)
```

That `^` is the whole idiom: render records what *is*, update computes what *happened*.
No event queues, no callbacks, no stale flags — a pure function of two consecutive states.
(`ImAppRising / ImAppFalling / ImAppChanged` name the vocabulary; the raw operators remain idiomatic.)

And the discipline pays out: because OnUpdate is the sole mutator and every control's whole state lives in
registered storage, a byte snapshot of that storage IS the app at that frame. **Time travel is a theorem
here, not a feature** — `ImGuiAppStateHistory` snapshots/restores any framework app with zero per-app code,
the contract suite proves restore-and-replay reproduces trajectories byte-for-byte, and the Composer toolbar
exposes it as a scrubber ("App time": watch the Breathing timer run backwards).

## The type algebra

Dependencies are `const` pointers keyed by **type**: `app->Data` holds exactly one instance per
PersistData type. That is a functional dependency — one producer per type — and it makes push order
a topological order over the data-flow DAG. Cycles are detectable, ordering is computable,
and "wire A into B" has a single meaning.

## Why a node editor

Because every one of these concepts is a graph-theoretic object, the Composer is not a picture of
the app — it **is** the app, stated in its own algebra:

- the composition tree → drill-down scopes (Tab in, Esc out, breadcrumb path);
- the frame pipeline → execution-order rails and sequence badges;
- the `^` idiom → authorable events (`when <temp field> <edge> → <reaction>`) that generate the
  exact guarded blocks a demo author writes by hand;
- the invariants → machine-checked, not gesture-enforced: containment is a forest, edges are
  kind-well-formed, data types keep one producer, event writers are confluent.

The graph round-trips to C++ and back. Generation is flexible precisely because the model's
soundness is a checked relation, not a UI habit.

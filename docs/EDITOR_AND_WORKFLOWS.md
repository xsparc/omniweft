# Editor and user workflows

Status: proposed interaction design.

## Workspace layout

```text
+--------------------------------------------------------------------+
| World / checkpoint     Run | Pause | Step     Connection / budgets   |
+-------------------+---------------------------+--------------------+
| Scene + volumes   |                           | Typed inspector    |
| Search / tags     |       World viewport      | Selection revision |
| Agent sessions    |       Camera / picking    | Capabilities       |
| Asset catalog     |                           | Physical properties|
+-------------------+---------------------------+--------------------+
| Agent goal / plan | Preview diff / affected regions / estimated cost|
+--------------------------------------------------------------------+
| Command timeline: queued -> preparing -> committed -> presented     |
| Errors | undo/redo authoring | replay | checkpoint | diagnostics     |
+--------------------------------------------------------------------+
```

The editor presents data and operations already available to the SDK. An AI-only hidden mutation path would make debugging and user intervention unreliable.

## Human and AI collaboration

The user selects a world/region and grants a session bounded capabilities. They enter a goal or choose a scripted recipe. The agent queries available resources, proposes a plan and receives validation feedback. The editor shows objects/regions affected, resource estimates and unsupported steps. Depending on the host's adopted policy, low-risk permitted edits can commit automatically; destructive or high-impact operations can require preview approval. Preview is a runtime product capability and is distinct from repository PR approval.

Conflicts display the stale revision and affected selection. Refresh/replan is explicit. The UI never says an action succeeded merely because the model generated an explanation. It displays the engine receipt and, separately, whether the viewport has presented it.

## Living Workshop scenario

| Step | User or agent action | Engine feedback | Verification |
| --- | --- | --- | --- |
| 1 | Open a local empty world with a fixed seed | World ID, revision and offline provider status | No network credentials or downloads |
| 2 | Ask the scripted builder to create a courtyard | Preview of entities, materials and estimated cost | Public SDK receipts match expected object set |
| 3 | Carve a ramp across two voxel chunks | Pending cook, then updated terrain and collider | Occupancy, seam and raycast assertions |
| 4 | Place blocks and drop a ball | Physical motion and contact visualization | Solver invariants and no unsupported collider |
| 5 | Paint a sign and deform a decorative mesh | Exact texel selection and highlighted vertex set | Byte/vertex assertions plus GPU capture |
| 6 | Submit a deliberately over-budget plan | Actionable rejection, world stays usable | No mutation/allocation leak |
| 7 | Stop the agent during preparation | Cancellation or precise too-late receipt | No double application after reconnect |
| 8 | Save, restart and replay | Restored IDs/assets and command timeline | Declared replay tier and checkpoint checks |

Each step has a small standalone example first; the integrated scenario verifies interactions once those foundations pass.

## Undo, rewind and preview branches

Authoring undo is an inverse transaction with preconditions. It restores supported edited data; it does not reverse elapsed physical time or arbitrary provider side effects. A checkpoint/rewind restores the complete supported simulation state. A speculative branch has its own world ID and copy-on-write resources, quotas and garbage-collection roots. Merging a branch computes an explicit diff and checks live revisions; it cannot silently resolve conflicts.

The first editor can preview a command diff without a fully simulated branch. Simulation branches and branch merging are later roadmap capabilities. Label these states separately in the UI.

## Controls and accessibility

Provide documented camera movement, select/focus, transform controls, pause/step, undo/redo and a command palette. Remapping, keyboard navigation, readable focus, scalable text, non-color status cues and adjustable motion are required for public tool usability. An immediate-mode UI library does not automatically provide full assistive-technology support; investigate its accessibility limitations before claiming compliance.

Errors state the rejected operation, reason and repair action. Diagnostics expose queue depth, memory, pending geometry revision and physics/render state without requiring users to read implementation logs. Telemetry is off by default. Performance recordings and provider logging require explicit opt-in and redaction.

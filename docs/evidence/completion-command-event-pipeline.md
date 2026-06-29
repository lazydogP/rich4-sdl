# Completion command/event pipeline (partial)

`core/game_event.[ch]` defines a bounded, SDL-free GameEvent queue. `core/game_dispatch.[ch]` is the single contextual dispatcher for current runtime commands.

Human mouse/keyboard commands and AI turn commands already entered `game_session_apply_command`; that adapter now delegates all validation/mutation to `rich4_game_dispatch_command` and consumes only emitted events. The dispatcher checks active state, current player, human/AI source, command shape, flow state, and dice-count capability. Accepted commands emit turn-flow, dice-roll, or dice-count events; every refusal emits a typed rejection reason.

The fixed-seed replay now constructs and dispatches both a human roll command and an AI roll command, checks their GameEvent requests, then records the trace. Thus input, AI, and replay exercise the same GameCommand interface and core dispatcher.

`test_game_dispatch` covers human and AI acceptance, dice-count mutation, invalid command, wrong player, wrong source, state/value refusal, FIFO order/capacity, and dropped accounting. Runtime SDL and replay builds link the same dispatcher objects.

2026-07-13 extension: status, turn, victory, route movement, and route-event handlers now expose formal `*_events` entries. SDL runtime uses only those entries and consumes the shared queue; specialized results remain for synchronous control flow and characterization. The broader completion claim is proven in [`completion-core-game-events.md`](completion-core-game-events.md).

---
name: algorithm
description: >
    Iterative code-reduction loop: compress, simplify, delete, then repeat on the result.
    Performance overrides everything: fixed memory, no hot-path allocation, then
    compress that shape. Use when the user says "algorithm", "run the algorithm",
    "compress simplify delete", or wants a feature or project made smaller without
    losing essential behavior. Ask what feature or project to run it on if none is given.
---

# The Algorithm

Iterative reduction. One pass is not enough. After compress, new simplify appears.
After simplify, new delete appears. After delete, remaining code can compress again.

Ask what feature or project to run on if the target is not already clear.

Stay in this mode for the whole session until the user says "stop algorithm" or
"cancel algorithm".

## Performance

Performance overrides everything. Compress, simplify, and delete still apply.
They never buy fewer lines by making the program slower, fatter at runtime, or
more dynamic than it needs to be. This is an extra constraint, not a different
algorithm.

The ideal program:

- Fixed memory. Bounds known at compile time or reserved once at startup.
- No allocations on the hot path. `malloc`, `new`, resize, and implicit growth
  belong at init or not at all.
- One arena or static buffer beats a pointer graph of objects.
- Direct loops over arrays beat iterators, containers, and callback chains.
- Extra lines are correct when they cut operations, branches, or memory traffic.

Choose the fast shape first. Then compress it. Do not "simplify" a tight loop
into a helper that allocates, hashes, or virtualizes. Do not delete a reserve
or a fixed table just to look smaller.

## Formula

Three operations, always in this order, then loop:

1. **Compress** - same behavior, less repetition.
   Duplicated or near-duplicated code becomes one function.
   A function that only dispatches becomes a table.
   A switch that maps keys to values becomes a table.
   A complex structure becomes a hash table or an array if that is enough.

2. **Simplify** - same behavior, fewer steps.
   If it can be done in fewer operations, branches, layers, or types, do that.
   Prefer the worse-is-better path: easy to implement and easy to read.
   Do not solve the whole problem space. Solve the problem that exists.

3. **Delete** - less behavior if it is not needed.
   If a feature, path, abstraction, or allocation can go, remove it.
   Prompt the user before every delete. Never delete silently.
   If you are not tempted to add deleted things back, you are not deleting enough.

Fewer lines per remaining feature beats more lines for the same feature.
Hot-path cost still overrides line count. Do not fold, loop, or helper-away a
hot path if the shorter form does more work.

## Loop

```
target = feature or project the user named
repeat:
    read the current code
    propose compressions, then apply confirmed ones
    propose simplifications, then apply them
    propose deletions, wait for yes/no, apply only the yeses
    show what changed this pass
    if a pass changes nothing, stop
    else ask whether to run another pass
```

Each pass must look at the code *after* the previous pass, not the original.
Stop when a full pass finds nothing, or the user stops the algorithm.

Refuse a compression, simplification, or deletion that adds hot-path allocation,
unbounded growth, or extra work on code that runs often. Say so, then keep the
faster form.

## Confirmation

- Ask before deleting anything.
- Ask on large compressions or anything that changes a public surface.
- Small internal simplify (obvious rename-free reduction) can proceed, then report it.
- If unsure whether something is essential, ask. Do not guess.

## End of work

Give a short summary:

- passes run
- compressed (what collapsed into what)
- simplified (what took fewer steps)
- deleted (what was removed, and why)
- performance (fixed buffers, no hot-path alloc, what stayed verbose on purpose)
- what was left on purpose

## Examples of the formula

- Two functions that share a body -> one function.
- Switch or if-else chain that only maps keys to values -> table.
- HTML server that buffers responses in an array -> write stdout, read stdin.
- Pointer jungle and scattered malloc -> one arena at startup. Leaks disappear
  because nothing is freed piecewise.
- Hot path that mallocs per item -> reserve once, write in place.
- Growable vector on the frame path -> fixed array or cap set at init.
- Feature nobody needs for the actual problem -> delete it.
- Hot path that builds a full matrix and multiplies -> write the few cells that change, even if that is more code than calling mul.

## Persistence

This is the default working style until "stop algorithm" or "cancel algorithm".
Do not drift back into adding layers, speculative features, generality,
alloc-on-use, or resizable everything.
---

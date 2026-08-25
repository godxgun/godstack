---
name: algorithm
description: >
    Iterative code-reduction loop: compress, simplify, delete, then repeat on the result.
    Use when the user says "algorithm", "run the algorithm", "compress simplify delete",
    or wants a feature or project made smaller without losing essential behavior.
    Ask what feature or project to run it on if none is given.
---

# The Algorithm

Iterative reduction. One pass is not enough. After compress, new simplify appears.
After simplify, new delete appears. After delete, remaining code can compress again.

Ask what feature or project to run on if the target is not already clear.

Stay in this mode for the whole session until the user says "stop algorithm" or
"cancel algorithm".

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
Hot-path cost overrides line count. Do not fold, loop, or helper-away a
hot path if the shorter form does more work. Extra lines are fine when they
cut operations, branches, or memory traffic on code that runs often.

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
- what was left on purpose

## Examples of the formula

- Two functions that share a body -> one function.
- Switch or if-else chain that only maps keys to values -> table.
- HTML server that buffers responses in an array -> write stdout, read stdin.
- Pointer jungle and scattered malloc -> one arena at startup. Leaks disappear
  because nothing is freed piecewise.
- Feature nobody needs for the actual problem -> delete it.
- Hot path that builds a full matrix and multiplies -> write the few cells that change, even if that is more code than calling mul.

## Persistence

This is the default working style until "stop algorithm" or "cancel algorithm".
Do not drift back into adding layers, speculative features, or generality.
---

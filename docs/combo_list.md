# Combo List window

Training-mode overlay that shows combos you write yourself, one file per character.

- Open it from **F1 > Overlays > Combo List**.
- It only draws during a training match. Outside training it stays open but hidden, so it
  comes back by itself next time.
- It shows the combos for the character on the side you pick (P1 or P2).

## Files

`BBCF_IM\Combos\<Character>.txt` in the game folder, e.g. `BBCF_IM\Combos\Ragna.txt`.
The character name is the same one the palette folders use (`src/Game/characters.cpp`).

If the file does not exist, the window offers to write a commented starter file. The
window re-checks the file about twice a second and reloads it when you save, so you can
keep it open in a text editor while you practise.

## Format

```
# Lines starting with # are comments.
combo: Midscreen BnB
note: Shown under the combo name.
5B > 5C > 2C
6A > 236D

combo: Corner route
j.C > 5C > 2C > 5D
```

- `combo: <name>` starts a new combo.
- `note: <text>` adds a line of notes to the current combo.
- Any other line is one or more steps separated by `>` (`->` works too). A combo can
  spread its steps over several lines.
- Keywords are case-insensitive. CRLF line endings and a UTF-8 BOM are fine.
- Steps written before the first `combo:` line go in a combo called "Untitled", and a
  combo with no steps is skipped. Both show up as warnings at the top of the window.

## Ticking steps off

While you play, each step is marked as it lands: `[x]` landed, ` > ` next, `[!]` the step
where the wrong move landed. The status line says whether the attempt is in progress,
complete, dropped, or off route. A new combo (the dummy's hit counter starting again)
starts a new attempt automatically, and so does a training reset.

### Match keys

Without match keys, any move that lands ticks off the next step, so the window only checks
that you hit the right *number* of moves. To check the right moves, give a step the game's
name for the move after a `|`:

```
combo: Midscreen BnB
5B | NmlAtk5B > 5C | NmlAtk5C > jc | - > j.C | NmlAtkAirC
```

- `|` then a name: only that move ticks the step off. Case-insensitive.
- `A/B`: either name counts (for moves with an air or alternate version).
- `-`: a step that never hits (jump cancel, dash). It is ticked off when the step after it
  lands, and a trailing one does not stop a combo from completing.

The names above are examples of the pattern, not checked names. To get the real ones, tick
**Show move names**, land the combo once, then press **Copy as steps**. That copies the
landed moves paired with the selected combo's steps, ready to paste into the file.

### How hits are detected (and what that misses)

- A step lands when the dummy's combo hit counter goes up. The move credited is whatever
  your character is doing on that frame.
- Each move *instance* is credited once, so a multi-hit move is one step and doing the same
  move twice is two steps.
- Projectiles and summons that hit while your character is doing something else are
  credited to that something else. Leave those steps without a key, or use `A/B`.

## Code

| Piece | Where |
|---|---|
| Parser (plain C++, no Win32/ImGui) | `src/Game/ComboList/ComboParser.*` |
| Hit tracking rules (plain C++) | `src/Game/ComboList/ComboTracker.*` |
| File loading and change polling | `src/Game/ComboList/ComboLibrary.*` |
| Window | `src/Overlay/Window/ComboListWindow.*` |
| Menu button | `src/Overlay/Window/MainMenu/PageOverlays.cpp` (`Overlays_ComboList`) |

## Tests

The parser and tracker have tests that build anywhere with a C++14 compiler; see the top of
`tools/tests/combo_list_tests.cpp` for the one-line build command.

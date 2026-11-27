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
complete, dropped, or off route (and which move landed instead). A new combo (the dummy's
hit counter starting again) starts a new attempt automatically, and so does a training reset.

### What a step accepts

Steps are worked out from what you write; hover a step to see exactly what ticks it off.

| You write | Accepts | Notes |
|---|---|---|
| `5A`, `2B`, `6A`, `3C`, `5D (2nd)` | that normal only (`NmlAtk5A`, ...) | |
| `j.5C`, `j5C`, `sj.5C`, `j.2C` | that air normal only (`NmlAtkAIR5C`, ...) | shown as `j.5C` |
| `B+C` (`BC`, `6BC`, `Throw`), `4B+C`, `j.B+C` | forward, back and air throw | |
| `A+B` (`CT`), `6A+B` (`CA`), `EA` | Crush Trigger, Counter Assault, Exceed Accel | |
| `NmlAtk2A`, `ThrowExe`, any state name | that state | shown as the move: `NmlAtk2A` shows `2A` |
| `jc`, `sjc`, `dash`, `land`, `dl.`, `66`, `OD`, ... | nothing, never checked | auto-pass: ticks itself once the step before it lands |
| `236D`, `j.214B`, `[4]6D` (not in the moves file) | any special, never a normal or throw | marked `?` |
| `236D` with `236D = Skill1` in the moves file | that special only | |
| anything else | any move | marked `?` |

An attack always needs its direction number: write `j.5C`, not `j.C`. That keeps `jc`
(jump cancel) and `sjc` (super jump cancel) unambiguous. A normal the character does not
have (a typo) is marked `!` and still only accepts that exact state, so nothing else ticks it.

`step | name` overrides all of this for one step: `5B | NmlAtk5B`, `j.5C | j.5C/j.5B`
(either), or `| -` to make any step auto-pass.

### The moves file (specials)

Normals and throws follow a fixed naming scheme in the game's scripts, so they are decoded
(the same rules `ScrStateNames` uses for the dummy-action move picker, run backwards). A
character's specials have arbitrary names (`Skill1`, `GauntletHades`), so they need a
mapping, one file per character: `BBCF_IM\Combos\Moves\<Character>.txt`.

```
# notation = StateName[/StateName...]
236D = Skill1
214D = Skill2/Skill2Air
```

The window offers to create the file when a combo has unmapped specials; it lists every
special state in the character's script as comments. To find which state a special is, tick
**Show move names** and do the move: the list names the state each move was credited to.

If you write a combo with unmapped specials (`5C > 236D`) and complete it, **Copy moves
file lines** appears under the list. It copies a `notation = StateName` line for each step
that is not checked exactly yet, naming the move that ticked it off (`236D = Skill1`). It
only appears after a completed attempt: then the tracker itself matched every step to a
move, in order. After a dropped or off-route attempt, or a different combo, pairing the
selected combo's steps with whatever landed would be guesswork, so it is not offered.

### Copying the combo you just did

With **Show move names** ticked, **Copy combo** copies the last combo as a combo-file entry,
ready to paste into `<Character>.txt` (rename it from "Recorded"):

```
combo: Recorded
B+C > StandDrive > 236A > 6B > 5C > 3C > 236A > 6B > 5C > 3C
```

- Normals are written as notation (`5C`, `3C`, `j.5C`, `5D (2nd)`), and the universal moves
  as their alias (`B+C`, `4B+C`, `j.B+C`, `A+B`, `6A+B`, `EA`).
- A special is written the way the moves file names it (`236A` for `236A = Assault_A`). If
  the moves file does not name it, it is written as its state name (`StandDrive`), which
  tracks exactly; inputs for specials are never guessed. Replace the name with notation
  once you have mapped it.
- Each step resolves back to exactly the state that landed (anything that would not, for
  example because a moves-file line took over `2C`, is written as the state name instead).
- One step per move. A multi-hit move is one step. There is one exception: a later hit that
  landed after cancelling into a state that is credited elsewhere in the same combo gets
  its own step. A special cancelled into another special (`5D` into `236A`) counts as one
  move unless something names the second one; the copy names it (it is a step later in the
  combo), so it would count as its own move when tracked, and the copy has to list it.
- Movement (`jc`, `dash`, ...) is not recorded, so it is not in the copy. It is never
  checked anyway; add it back if you want it shown.

### How hits are detected (and what that misses)

- The character's script is read once per character load: every state name, and the
  cancel lists (`whiffCancel` / `hitOrBlockCancel`) that name the moves you can cancel into.
- The attacker's states are followed as they change. Some states start a move (any normal,
  a cancel target, a special out of neutral or out of a normal, anything a step or the
  moves file names). The rest continue the current move: `ThrowExe` after `NmlAtkThrow`,
  `NmlAtk5D_2nd` after `NmlAtk5D`, a special's later states.
- When the dummy's combo hit counter goes up, the move last started is credited, once. A
  multi-hit move is one step; the same move again is a new step. A projectile that hits
  after you recover still credits the special that threw it, if nothing new was started.
- A special cancelled straight into another special counts as one move unless the script
  lists the second as a cancel target or the moves file names it. Map supers and rekkas if
  they do not tick.

## Code

| Piece | Where |
|---|---|
| Parser (plain C++, no Win32/ImGui) | `src/Game/ComboList/ComboParser.*` |
| Notation to state names (plain C++) | `src/Game/ComboList/MoveTable.*` |
| Hit tracking rules, and the copy buttons' text (`ComboRecording`, plain C++) | `src/Game/ComboList/ComboTracker.*` |
| File loading and change polling | `src/Game/ComboList/ComboLibrary.*` |
| Window | `src/Overlay/Window/ComboListWindow.*` |
| Menu button | `src/Overlay/Window/MainMenu/PageOverlays.cpp` (`Overlays_ComboList`) |

## Tests

The parser, the notation round trip (`MoveTable::NotationFor`), the tracker and the copied
text have tests that build anywhere with a C++14 compiler; see the top of
`tools/tests/combo_list_tests.cpp` for the one-line build command.

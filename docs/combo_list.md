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
| `236A`, `623C`, `236236D`, `5D` (a special's input) | the move the game registers with that input | from the game's move list; `j.236A` is the same |
| `236D` with `236D = Skill1` in the moves file | that special only | the moves file always wins |
| a special input the game's list does not name | any special, never a normal or throw | marked `?` |
| anything else | any move | marked `?` |

An attack always needs its direction number: write `j.5C`, not `j.C`. That keeps `jc`
(jump cancel) and `sjc` (super jump cancel) unambiguous. A normal the character does not
have (a typo) is marked `!` and still only accepts that exact state, so nothing else ticks it.

`step | name` overrides all of this for one step: `5B | NmlAtk5B`, `j.5C | j.5C/j.5B`
(either), or `| -` to make any step optional.

### Optional steps

Put `| -` after a step to show it without checking it: `5B > 5C > 6D | - > 236A`. It passes
as soon as the step before it lands (or when the combo starts, if it comes first), and does
not count towards the n/m total. Use it for setups that do not hit, such as placing a summon,
or for anything you do not want checked. Movement words (`jc`, `sjc`, `dash`, `land`, `dl.`)
behave this way without the `| -`.

### Where specials get their input: the game's move list

Normals and throws follow a fixed naming scheme in the game's scripts, so they are decoded
(the same rules `ScrStateNames` uses for the dummy-action move picker, run backwards).
Specials have arbitrary names (`Assault_A`, `Skill1`), but each character's script registers
every move it can input together with that input (`registerMove("Assault_A", 2)` then
`moveInput(236)`, `moveInput(press A)`; commands 14001 / 14012 / 14002). The window turns
those registrations into notation, so `236A` tracks Kokonoe's `Assault_A` with no setup, and
the move list shows it as `236A (Assault_A)`.

The registrations are in a subroutine (the character's init), which is not in the script's
state index that `parse_scr` walks. `parse_scr_move_registrations` (with the scanner in
`src/Game/Scr/ScrMoveRegistrations.*`) finds them by scanning the script's code instead, kept
inside the script file's bounds from its FPAC entry. **Show move names** reports how many
were found; if it says the move list was not found, the debug log has an `[Scr]` line saying
why.

One move can be registered under several inputs: Kokonoe's 1D..9D are all `StandDrive`
(the direction only decides where the Graviton goes). Every one of those inputs then
resolves to that move and the step is shown as `5D`, but which direction you held is **not
checked** (hover says so). Telling them apart would need the held direction, and the mod's
battle-input hook that could provide it is disabled under Wine/Proton.

How sure each part is:
- Command IDs and sizes: high. They match the parser's own size tables and both public BBCF
  databases (bbscript `static_db/bbcf.ron`, bbtools `static_db/bbcf`).
- Input values: high for A-D (4/13/22/31) and 236/623/214/41236/421/63214/236236/214214
  (172-179), which the game's motion table and the repo's Ghidra notes confirm. Medium for
  22 (186) and charge 2-8 (189). Values the sources disagree on (632146) or that are not in
  the table (release/hold buttons, 360s) are not guessed: that move is listed with a `?` and
  its raw values, and needs a moves-file line.
- Air versus ground: not known. A ground and an air version registered with the same inputs
  both count for `236A`, and `j.236A` reads the same as `236A`.
- Follow-ups and rekkas that are checked inside a move rather than registered are not in the
  list; map them in the moves file if they matter.

### The moves file (overrides)

One file per character, `BBCF_IM\Combos\Moves\<Character>.txt`, for moves the game's list
does not name or names wrongly. A line here always wins.

```
# notation = StateName[/StateName...]
236D = Skill1
214D = Skill2/Skill2Air
```

The window offers to create the file when a combo has unmapped specials; it lists, as
comments, the moves read from the game's list (and any with inputs it could not name) and
every special state in the character's script. To find which state a special is, tick
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
B+C > 5D > 236A > 6B > 5C > 3C > 236A > 6B > 5C > 3C
```

- Normals are written as notation (`5C`, `3C`, `j.5C`, `5D (2nd)`), and the universal moves
  as their alias (`B+C`, `4B+C`, `j.B+C`, `A+B`, `6A+B`, `EA`).
- A special is written as its input: from the moves file if it names it, otherwise from the
  game's move list (`236A` for `Assault_A`, `5D` for Kokonoe's `StandDrive`). If neither
  knows it, it is written as its state name, which still tracks exactly; inputs are never
  guessed.
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
  a cancel target, a move the game's move list registers, a special out of neutral or out
  of a normal, anything a step or the moves file names). The rest continue the current move: `ThrowExe` after `NmlAtkThrow`,
  `NmlAtk5D_2nd` after `NmlAtk5D`, a special's later states.
- When the dummy's combo hit counter goes up, the move last started is credited, once. A
  multi-hit move is one step; the same move again is a new step. A projectile that hits
  after you recover still credits the special that threw it, if nothing new was started.
- A special cancelled straight into another special counts as one move unless the second is
  a registered move, a cancel target, or named by the moves file. Map rekkas and follow-ups
  if they do not tick.

## Code

| Piece | Where |
|---|---|
| Parser (plain C++, no Win32/ImGui) | `src/Game/ComboList/ComboParser.*` |
| Notation to state names (plain C++) | `src/Game/ComboList/MoveTable.*` |
| Finding the script's move registrations (plain C++) | `src/Game/Scr/ScrMoveRegistrations.*` |
| Hit tracking rules, and the copy buttons' text (`ComboRecording`, plain C++) | `src/Game/ComboList/ComboTracker.*` |
| File loading and change polling | `src/Game/ComboList/ComboLibrary.*` |
| Window | `src/Overlay/Window/ComboListWindow.*` |
| Menu button | `src/Overlay/Window/MainMenu/PageOverlays.cpp` (`Overlays_ComboList`) |

## Tests

The parser, the notation round trip (`MoveTable::NotationFor`), the tracker and the copied
text have tests that build anywhere with a C++14 compiler; see the top of
`tools/tests/combo_list_tests.cpp` for the one-line build command.

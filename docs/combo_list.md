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

### Reserved: match keys

A step can carry a match key after `|`, for example `5B | NmlAtk5B`. It is parsed and
stored but not used yet. It is reserved for marking steps off as they land.

## Code

| Piece | Where |
|---|---|
| Parser (plain C++, no Win32/ImGui) | `src/Game/ComboList/ComboParser.*` |
| File loading and change polling | `src/Game/ComboList/ComboLibrary.*` |
| Window | `src/Overlay/Window/ComboListWindow.*` |
| Menu button | `src/Overlay/Window/MainMenu/PageOverlays.cpp` (`Overlays_ComboList`) |

#pragma once
// Turns what a player writes in a combo file ("2A", "j.5C", "B+C", "NmlAtk2A", "236D") into the
// script state names the game uses, for one character. Plain C++ (no Win32, no game structs)
// so the rules can be tested on their own; the window feeds it the character's state list.
//
// Resolution order for a step:
//   1. The character's moves file (BBCF_IM\Combos\Moves\<Character>.txt): "236D = Skill1".
//   2. A state name written directly ("NmlAtk2A", "ThrowExe"), case-insensitive.
//   3. Direction + button notation, decoded with the same rules ScrStateNames uses to display
//      names, run the other way: 5C -> NmlAtk5C, 2D -> NmlAtk2D, j.5C -> NmlAtkAIR5C,
//      j.2C -> NmlAtkAIR2C, 5D (2nd) -> NmlAtk5D_2nd. An attack always has its direction
//      number: "j.C" is not read as an attack, so it can never be confused with "jc".
//   4. Built-in aliases for the universal moves: B+C -> throw, 4B+C, j.B+C, A+B (Crush
//      Trigger), 6A+B (Counter Assault), Exceed Accel.
//   5. A display name of any state ("Gauntlet Hades" for GauntletHades).
//   6. Movement: jc (jump cancel), sjc (super jump cancel), dash, land, dl., ... These are
//      AutoPass steps: shown in the list, never checked.
//   7. Anything that looks like a special input (236D, j.214B, [4]6D) but is not mapped:
//      matches any special, never a normal or a throw. Reported so it can be mapped.
//   8. Anything else: matches any move. Reported.

#include <map>
#include <set>
#include <string>
#include <vector>

enum class StepMatch
{
	Exact,      // only the listed state names count
	AnySpecial, // any special (not a normal, throw or common state); notation not mapped yet
	AnyMove,    // anything that hits; text not understood
	AutoPass,   // movement (jc, sjc, dash, land, dl.): never checked against a move; passes
	            // automatically once the step before it lands
};

struct ResolvedStep
{
	std::string display;            // what the window shows
	StepMatch match = StepMatch::AnyMove;
	std::vector<std::string> keys;  // for Exact: the accepted state names
	bool notInMoveList = false;     // Exact, but the character has no such state (likely typo)
};

class MoveTable
{
public:
	// stateNames: every state in the character's script. cancelTargets: every name that
	// appears in a whiffCancel/hitOrBlockCancel, i.e. moves the script lets you cancel into.
	void SetStates(const std::vector<std::string>& stateNames, const std::vector<std::string>& cancelTargets);

	// "notation = State[/State...]" lines from the character's moves file. '#' comments.
	// Returns the line numbers (1-based) it could not read.
	std::vector<int> SetUserMappings(const std::string& fileText);

	bool HasStates() const { return !m_states.empty(); }
	const std::vector<std::string>& States() const { return m_stateList; }

	// ComboStep::notation / ComboStep::matchKey in, resolved step out.
	ResolvedStep Resolve(const std::string& notation, const std::string& matchKey) const;

	// For the tracker: is entering `state` (after `previous`) the start of a new move, as
	// opposed to a continuation of the current one (ThrowExe after NmlAtkThrow, 5D_2nd after
	// 5D, a special's later states)? `steps` adds every state a combo step names.
	bool IsMoveStart(const std::string& state, const std::string& previous,
		const std::vector<ResolvedStep>& steps) const;

	bool IsSpecial(const std::string& state) const;
	bool IsUserMapped(const std::string& state) const;

	static std::string Normalize(const std::string& text);

	// How a state is shown: ScrStateNames' display form, except that air normals keep their
	// direction number (NmlAtkAIR5C -> "j.5C", where ScrStateNames says "j.C").
	static std::string DisplayName(const std::string& state);

private:
	// Lower-cased state name -> state name as the script spells it.
	std::map<std::string, std::string> m_states;
	std::vector<std::string> m_stateList;
	// Normalized display name -> state names.
	std::map<std::string, std::vector<std::string>> m_byDisplay;
	std::set<std::string> m_cancelTargets; // lower-cased
	// Normalized notation -> state names, from the moves file.
	std::map<std::string, std::vector<std::string>> m_user;
	std::set<std::string> m_userStates;    // lower-cased

	std::vector<std::string> KeysFor(const std::string& text, bool& outFound) const;
	std::string Canonical(const std::string& name) const;
};

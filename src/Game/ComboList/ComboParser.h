#pragma once
// Plain C++ (no Win32, no ImGui) so the combo file format can be tested on its own.
//
// File format, one combo file per character (BBCF_IM\Combos\<Character>.txt):
//
//   # A line starting with # is a comment.
//   combo: Midscreen BnB
//   note: Any line starting with "note:" is shown under the combo name.
//   5B > 5C > 2C
//   6A > 236D
//
// - "combo:" starts a new combo. Everything after it is the combo's name.
// - Every other non-empty line is a list of steps separated by '>'. A combo may spread its
//   steps over as many lines as you like. "->" works as a separator too.
// - A step may carry a match key after a '|', e.g. "5B | NmlAtk5B": the game's name for the
//   move, used to tick the step off when it lands. "A/B" accepts either name, and "-" marks
//   a step that never hits (e.g. "jc | -"). See ComboTracker.h for the matching rules.
// - The keywords are case-insensitive.

#include <string>
#include <vector>

struct ComboStep
{
	std::string notation; // what the player reads, e.g. "5B"
	std::string matchKey; // optional text after '|'; reserved for hit tracking
};

struct Combo
{
	std::string name;
	std::vector<std::string> notes;
	std::vector<ComboStep> steps;
};

struct ComboFileIssue
{
	int line; // 1-based line number in the file
	std::string message;
};

namespace ComboParser
{
	// Parses the whole file text. Never throws; problems are reported in outIssues and the
	// parser keeps going so one bad line does not hide every other combo in the file.
	void Parse(const std::string& text, std::vector<Combo>& outCombos, std::vector<ComboFileIssue>& outIssues);
}

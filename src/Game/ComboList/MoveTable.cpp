#include "MoveTable.h"

#include "Game/Scr/ScrStateNames.h"

#include <algorithm>
#include <cctype>

namespace
{
	std::string Lower(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return s;
	}

	std::string Trim(const std::string& s)
	{
		size_t b = 0;
		size_t e = s.size();
		while (b < e && std::isspace(static_cast<unsigned char>(s[b])))
			++b;
		while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
			--e;
		return s.substr(b, e - b);
	}

	bool StartsWith(const std::string& s, const std::string& prefix)
	{
		return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
	}

	bool IsButton(char c) { return c == 'a' || c == 'b' || c == 'c' || c == 'd'; }
	bool IsDigit(char c) { return c >= '1' && c <= '9'; }

	// Universal moves, by normalized notation. Only used when the character has the state
	// (or when no state list is available at all).
	struct Alias
	{
		const char* notation; // normalized
		const char* states;   // '/'-separated
	};
	const Alias kAliases[] = {
		{ "bc",             "NmlAtkThrow" },
		{ "5bc",            "NmlAtkThrow" },
		{ "6bc",            "NmlAtkThrow" },
		{ "throw",          "NmlAtkThrow" },
		{ "4bc",            "NmlAtkBackThrow" },
		{ "backthrow",      "NmlAtkBackThrow" },
		{ "jbc",            "NmlAtkAirThrow" },
		{ "airthrow",       "NmlAtkAirThrow" },
		{ "ab",             "NmlAtkGuardCrush" },
		{ "ct",             "NmlAtkGuardCrush" },
		{ "crushtrigger",   "NmlAtkGuardCrush" },
		{ "6ab",            "NmlAtkDeadAngle" },
		{ "ca",             "NmlAtkDeadAngle" },
		{ "counterassault", "NmlAtkDeadAngle" },
		{ "ea",             "NmlAtkExcite" },
		{ "exceedaccel",    "NmlAtkExcite" },
	};

	// Steps that are movement, not attacks: AutoPass. Overdrive activation is here too: it does
	// not hit.
	const char* const kAutoPass[] = {
		"jc", "sjc", "hjc", "djc", "dj", "sj", "hj", "j", "jump", "tk",
		"land", "landing", "dl", "delay",
		"dash", "66", "microdash", "md", "ad", "airdash", "j66", "backdash", "44", "bd", "walk",
		"od", "abcd", "overdrive",
	};

	// "j.", "j", "sj.", "dj.", "hj." in front of an attack all mean "in the air". Only taken off
	// when a direction number follows, so "jc" (jump cancel) is never read as j + C.
	bool StripAirPrefix(std::string& n)
	{
		const char* prefixes[] = { "sj", "dj", "hj", "j" };
		for (const char* p : prefixes)
		{
			const std::string prefix(p);
			if (StartsWith(n, prefix) && n.size() > prefix.size() && IsDigit(n[prefix.size()]))
			{
				n.erase(0, prefix.size());
				return true;
			}
		}
		return false;
	}

	// Normalized "2a" / "j5c" / "5aa" / "5d2nd" -> "NmlAtk2A" / "NmlAtkAIR5C" / ... or "".
	std::string NormalStateFor(const std::string& normalized)
	{
		std::string n = normalized;
		const bool air = StripAirPrefix(n);

		std::string suffix;
		const struct { const char* text; const char* state; } quals[] = {
			{ "2nd", "_2nd" }, { "3rd", "_3rd" },
		};
		for (const auto& q : quals)
		{
			const std::string t(q.text);
			if (n.size() > t.size() && n.compare(n.size() - t.size(), t.size(), t) == 0)
			{
				n.resize(n.size() - t.size());
				suffix = q.state;
				break;
			}
		}

		// An attack always has its direction number: 5A, 2B, j.5C. "A" or "j.C" on its own is
		// not read as one.
		if (n.empty() || !IsDigit(n[0]))
			return std::string();
		const std::string dir(1, n[0]);
		const std::string buttons = n.substr(1);
		const bool oneButton = buttons.size() == 1 && IsButton(buttons[0]);
		const bool doubled = buttons.size() == 2 && IsButton(buttons[0]) && buttons[0] == buttons[1];
		if (!oneButton && !doubled)
			return std::string();

		std::string upper = buttons;
		for (char& c : upper)
			c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
		return std::string("NmlAtk") + (air ? "AIR" : "") + dir + upper + suffix;
	}

	// 236D, 623C, j.214B, 632146D, [4]6D, 41236C: two or more direction digits before the
	// buttons, or a charge bracket.
	bool LooksLikeSpecial(const std::string& raw)
	{
		if (raw.find('[') != std::string::npos)
			return true;
		std::string n = MoveTable::Normalize(raw);
		StripAirPrefix(n);
		size_t digits = 0;
		while (digits < n.size() && IsDigit(n[digits]))
			++digits;
		return digits >= 2 && digits < n.size() && IsButton(n[digits]);
	}

	// Like Normalize but keeps '.', so "jc" stays apart from anything with a dot in it.
	// A trailing '.' ("dl.") is dropped.
	std::string AutoPassKey(const std::string& text)
	{
		std::string out;
		for (char c : text)
		{
			const unsigned char u = static_cast<unsigned char>(c);
			if (std::isspace(u) || c == '+')
				continue;
			out += static_cast<char>(std::tolower(u));
		}
		while (!out.empty() && out.back() == '.')
			out.pop_back();
		return out;
	}

	std::vector<std::string> SplitAlternatives(const std::string& text)
	{
		std::vector<std::string> out;
		size_t start = 0;
		while (start <= text.size())
		{
			size_t slash = text.find('/', start);
			if (slash == std::string::npos)
				slash = text.size();
			const std::string part = Trim(text.substr(start, slash - start));
			if (!part.empty())
				out.push_back(part);
			start = slash + 1;
		}
		return out;
	}

	bool HasQualifierSuffix(const std::string& state)
	{
		const char* suffixes[] = { "_2nd", "_3rd", "_2", "_3" };
		for (const char* s : suffixes)
		{
			const std::string suf(s);
			if (state.size() > suf.size() && state.compare(state.size() - suf.size(), suf.size(), suf) == 0)
				return true;
		}
		return false;
	}
}

std::string MoveTable::DisplayName(const std::string& state)
{
	const std::string air = "NmlAtkAIR";
	if (StartsWith(state, air) && state.size() >= air.size() + 2
		&& IsDigit(state[air.size()]) && std::isupper(static_cast<unsigned char>(state[air.size() + 1])))
	{
		// NmlAtkAIR5C -> j.5C, NmlAtkAIR2C -> j.2C, NmlAtkAIR5D_2nd -> j.5D (2nd)
		const std::string decoded = ScrStateNames::Display("NmlAtk" + state.substr(air.size()));
		return "j." + decoded;
	}
	return ScrStateNames::Display(state);
}

std::string MoveTable::Normalize(const std::string& text)
{
	std::string out;
	for (char c : text)
	{
		const unsigned char u = static_cast<unsigned char>(c);
		if (std::isspace(u) || c == '+' || c == '.' || c == '(' || c == ')' || c == '_' || c == '-')
			continue;
		out += static_cast<char>(std::tolower(u));
	}
	return out;
}

void MoveTable::SetStates(const std::vector<std::string>& stateNames, const std::vector<std::string>& cancelTargets)
{
	m_states.clear();
	m_stateList.clear();
	m_byDisplay.clear();
	m_cancelTargets.clear();

	for (const std::string& name : stateNames)
	{
		if (name.empty() || !m_states.emplace(Lower(name), name).second)
			continue;
		m_stateList.push_back(name);
		// Our display form only: ScrStateNames shows NmlAtkAIR5C as "j.C", which would let a
		// numberless "j.C" (or "jc") resolve to it.
		m_byDisplay[Normalize(DisplayName(name))].push_back(name);
	}
	for (const std::string& target : cancelTargets)
		m_cancelTargets.insert(Lower(target));
}

std::vector<int> MoveTable::SetUserMappings(const std::string& fileText)
{
	m_user.clear();
	m_userStates.clear();
	std::vector<int> badLines;

	int lineNumber = 0;
	size_t pos = 0;
	while (pos <= fileText.size())
	{
		size_t eol = fileText.find('\n', pos);
		if (eol == std::string::npos)
			eol = fileText.size();
		std::string line = Trim(fileText.substr(pos, eol - pos));
		pos = eol + 1;
		++lineNumber;

		if (lineNumber == 1 && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF)
			line = Trim(line.substr(3)); // UTF-8 BOM
		if (line.empty() || line[0] == '#')
			continue;

		const size_t eq = line.find('=');
		const std::string lhs = eq == std::string::npos ? std::string() : Normalize(line.substr(0, eq));
		const std::vector<std::string> rhs = eq == std::string::npos
			? std::vector<std::string>() : SplitAlternatives(line.substr(eq + 1));
		if (lhs.empty() || rhs.empty())
		{
			badLines.push_back(lineNumber);
			continue;
		}

		std::vector<std::string>& states = m_user[lhs];
		for (const std::string& s : rhs)
		{
			const std::string canonical = Canonical(s);
			states.push_back(canonical);
			m_userStates.insert(Lower(canonical));
		}
	}
	return badLines;
}

std::string MoveTable::Canonical(const std::string& name) const
{
	auto it = m_states.find(Lower(name));
	return it == m_states.end() ? name : it->second;
}

std::vector<std::string> MoveTable::KeysFor(const std::string& text, bool& outFound) const
{
	outFound = true;
	const std::string n = Normalize(text);

	// 1. The player's own mapping wins over everything.
	auto user = m_user.find(n);
	if (user != m_user.end())
		return user->second;

	// 2. A state name written as is.
	auto state = m_states.find(Lower(Trim(text)));
	if (state != m_states.end())
		return { state->second };

	// 3. Direction + button.
	const std::string normal = NormalStateFor(n);
	if (!normal.empty())
	{
		outFound = m_states.empty() || m_states.count(Lower(normal)) > 0;
		return { Canonical(normal) };
	}

	// 4. Universal moves.
	for (const Alias& alias : kAliases)
	{
		if (n == alias.notation)
		{
			std::vector<std::string> keys;
			for (const std::string& s : SplitAlternatives(alias.states))
			{
				if (m_states.empty() || m_states.count(Lower(s)))
					keys.push_back(Canonical(s));
			}
			if (keys.empty())
			{
				outFound = false;
				return SplitAlternatives(alias.states);
			}
			return keys;
		}
	}

	// 5. A state's display name ("Gauntlet Hades").
	auto byDisplay = m_byDisplay.find(n);
	if (byDisplay != m_byDisplay.end())
		return byDisplay->second;

	outFound = false;
	return {};
}

ResolvedStep MoveTable::Resolve(const std::string& notation, const std::string& matchKey) const
{
	ResolvedStep step;
	step.display = notation;

	// A state name written as the step shows as the move ("NmlAtk2A" -> "2A").
	if (matchKey.empty())
	{
		auto state = m_states.find(Lower(Trim(notation)));
		if (state != m_states.end())
			step.display = DisplayName(state->second);
		else if (StartsWith(Trim(notation), "NmlAtk") || StartsWith(Trim(notation), "CmnAct"))
			step.display = DisplayName(Trim(notation));
	}

	if (matchKey == "-")
	{
		step.match = StepMatch::AutoPass;
		return step;
	}

	if (!matchKey.empty())
	{
		// Explicit key: each '/' alternative may itself be a state name or notation.
		step.match = StepMatch::Exact;
		for (const std::string& alt : SplitAlternatives(matchKey))
		{
			bool found = false;
			std::vector<std::string> keys = KeysFor(alt, found);
			if (keys.empty())
				keys.push_back(alt); // unknown name: trust what was written
			if (!found && HasStates())
				step.notInMoveList = true;
			step.keys.insert(step.keys.end(), keys.begin(), keys.end());
		}
		return step;
	}

	// Movement words are checked before state lookup so "dash" is not taken for a state name,
	// unless the player mapped the word themselves.
	if (!m_user.count(Normalize(notation)))
	{
		const std::string key = AutoPassKey(notation);
		for (const char* word : kAutoPass)
		{
			if (key == word)
			{
				step.match = StepMatch::AutoPass;
				return step;
			}
		}
	}

	bool found = false;
	step.keys = KeysFor(notation, found);
	if (!step.keys.empty())
	{
		step.match = StepMatch::Exact;
		step.notInMoveList = !found && HasStates();
		return step;
	}

	step.match = LooksLikeSpecial(notation) ? StepMatch::AnySpecial : StepMatch::AnyMove;
	return step;
}

bool MoveTable::IsSpecial(const std::string& state) const
{
	return ScrStateNames::Categorize(state) == ScrStateNames::Category::Special;
}

bool MoveTable::IsUserMapped(const std::string& state) const
{
	return m_userStates.count(Lower(state)) > 0;
}

bool MoveTable::IsMoveStart(const std::string& state, const std::string& previous,
	const std::vector<ResolvedStep>& steps) const
{
	const std::string lower = Lower(state);

	// Anything a step names explicitly, or the player mapped, is a move of its own.
	for (const ResolvedStep& step : steps)
	{
		if (step.match != StepMatch::Exact)
			continue;
		for (const std::string& key : step.keys)
		{
			if (Lower(key) == lower)
				return true;
		}
	}
	if (m_userStates.count(lower))
		return true;

	switch (ScrStateNames::Categorize(state))
	{
	case ScrStateNames::Category::Normal:
		// NmlAtk5D_2nd is the automatic second part of 5D, unless the script lists it as
		// something you cancel into (then it is its own input).
		return !HasQualifierSuffix(state) || m_cancelTargets.count(lower) > 0;

	case ScrStateNames::Category::Common:
		return false; // standing, jumping, dashing, landing: not an attack

	case ScrStateNames::Category::Special:
		break;
	}

	if (m_cancelTargets.count(lower))
		return true;
	if (previous.empty())
		return true;

	switch (ScrStateNames::Categorize(previous))
	{
	case ScrStateNames::Category::Common:
		return true; // a special out of neutral
	case ScrStateNames::Category::Normal:
		// A normal cancelled into a special starts it; a throw handing over to its hit
		// state (NmlAtkThrow -> ThrowExe) does not.
		return Lower(previous).find("throw") == std::string::npos;
	case ScrStateNames::Category::Special:
		return false; // a special's own later states (Skill1 -> Skill1Exe)
	}
	return false;
}

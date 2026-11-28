#include "ComboLibrary.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/characters.h"

#include <Windows.h>

#include <fstream>
#include <sstream>

namespace
{
	const char* const kCombosFolder = "BBCF_IM\\Combos";
	const char* const kMovesFolder = "BBCF_IM\\Combos\\Moves";

	const char* const kStarterText =
		"# Combo list for %CHAR%\n"
		"# Shown by the mod's \"Combo List\" window in training mode (F1 > Overlays).\n"
		"# The window re-reads this file when you save it, so you can edit it while the game runs.\n"
		"#\n"
		"# combo: <name>    starts a new combo\n"
		"# note: <text>     shown under the combo name\n"
		"# Any other line is a list of steps separated by '>'.\n"
		"# A combo can spread its steps over several lines.\n"
		"# Lines starting with # are ignored.\n"
		"#\n"
		"# Normals and universal moves are checked exactly: 5A, 2B, 6A, j.5C, j.2C, B+C (throw),\n"
		"# 4B+C, j.B+C, A+B (Crush Trigger), 6A+B (Counter Assault). Attacks always need their\n"
		"# direction number (j.5C, not j.C). The game's own state name works too (NmlAtk2A).\n"
		"# Specials are written as their input (236A, 623C, 236236D): the window reads each move's\n"
		"# input from the game. If one is not recognised, map it in Moves\\%CHAR%.txt; until then it\n"
		"# accepts any special.\n"
		"#\n"
		"# Optional / untracked steps: put \" | -\" after a step and it is shown but never checked.\n"
		"# It passes as soon as the step before it lands. Use it for setups that do not hit\n"
		"# (placing a summon, a delay) or anything you do not want checked:\n"
		"#     5B > 5C > 6D | - > 236A\n"
		"# Movement (jc = jump cancel, sjc = super jump cancel, dash, land, dl.) works that way\n"
		"# without the \" | -\".\n"
		"#\n"
		"# Tip: tick \"Show move names\" in the window, do a combo, then \"Copy combo\" to get it in\n"
		"# this format, ready to paste.\n"
		"\n"
		"combo: Example - replace me\n"
		"note: Placeholder steps, not a real route.\n"
		"5A > 5B > 5C\n"
		"2C > 6A\n"
		"\n"
		"combo: Example with an optional step - replace me\n"
		"note: The 5D step is shown but never checked.\n"
		"5B > 5C > 5D | - > 2C\n";

	const char* const kMovesHeader =
		"# Moves for %CHAR%, used by the Combo List window to check steps exactly.\n"
		"#\n"
		"#   notation = StateName          e.g.  236D = Skill1\n"
		"#   notation = StateA/StateB      either state counts (e.g. a ground and an air version)\n"
		"#\n"
		"# Most moves do not need a line: normals and throws are worked out from their state names,\n"
		"# and specials from the game's own move list (listed below). Add a line for a move the\n"
		"# window gets wrong or cannot name; a line here always wins.\n"
		"#\n"
		"# To find a move's state name: tick \"Show move names\" in the Combo List window and do the\n"
		"# move; it shows the move each hit was credited to.\n";

	std::string ReplaceAll(std::string text, const std::string& from, const std::string& to)
	{
		size_t pos = 0;
		while ((pos = text.find(from, pos)) != std::string::npos)
		{
			text.replace(pos, from.size(), to);
			pos += to.size();
		}
		return text;
	}
}

// ---------------------------------------------------------------------------------------------

void WatchedTextFile::SetPath(const std::string& relativePath)
{
	m_relativePath = relativePath;
	m_fullPath = relativePath.empty() ? std::string() : GamePath(relativePath);
	m_exists = false;
	m_read = false;
	m_writeTime = 0;
	m_size = 0;
	m_text.clear();
}

bool WatchedTextFile::ReadStamp(uint64_t& outWriteTime, uint64_t& outSize) const
{
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (m_fullPath.empty() || !GetFileAttributesExA(m_fullPath.c_str(), GetFileExInfoStandard, &data))
		return false;
	if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		return false;

	outWriteTime = (static_cast<uint64_t>(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
	outSize = (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
	return true;
}

bool WatchedTextFile::Refresh(bool force)
{
	uint64_t writeTime = 0;
	uint64_t size = 0;
	const bool exists = ReadStamp(writeTime, size);
	if (!force && m_read && exists == m_exists && writeTime == m_writeTime && size == m_size)
		return false;

	m_read = true;
	m_exists = exists;
	m_writeTime = writeTime;
	m_size = size;
	m_text.clear();
	if (!exists)
		return true;

	std::ifstream file(m_fullPath, std::ios::in | std::ios::binary);
	if (!file)
	{
		LOG(2, "[ComboList] Could not open '%s'\n", m_fullPath.c_str());
		return true;
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	m_text = buffer.str();
	return true;
}

// ---------------------------------------------------------------------------------------------

std::string ComboLibrary::RelativePathFor(int charIndex)
{
	if (isCharacterIndexOutOfBound(charIndex))
		return std::string();
	return std::string(kCombosFolder) + "\\" + getCharacterNameByIndexA(charIndex) + ".txt";
}

std::string ComboLibrary::MovesRelativePathFor(int charIndex)
{
	if (isCharacterIndexOutOfBound(charIndex))
		return std::string();
	return std::string(kMovesFolder) + "\\" + getCharacterNameByIndexA(charIndex) + ".txt";
}

void ComboLibrary::Load(int charIndex)
{
	m_charIndex = charIndex;
	m_combosFile.SetPath(RelativePathFor(charIndex));
	m_movesFile.SetPath(MovesRelativePathFor(charIndex));
	m_combosFile.Refresh(true);
	m_movesFile.Refresh(true);
	ParseCombos();
	++m_generation;
}

void ComboLibrary::ParseCombos()
{
	m_combos.clear();
	m_issues.clear();
	if (!m_combosFile.Exists())
		return;

	ComboParser::Parse(m_combosFile.Text(), m_combos, m_issues);
	LOG(2, "[ComboList] Loaded %d combos (%d issues) from '%s'\n",
		static_cast<int>(m_combos.size()), static_cast<int>(m_issues.size()), m_combosFile.FullPath().c_str());
}

bool ComboLibrary::ReloadIfChanged()
{
	if (m_combosFile.FullPath().empty())
		return false;

	const bool combosChanged = m_combosFile.Refresh();
	const bool movesChanged = m_movesFile.Refresh();
	if (combosChanged)
		ParseCombos();
	if (combosChanged || movesChanged)
		++m_generation;
	return combosChanged || movesChanged;
}

bool ComboLibrary::WriteNewFile(const std::string& fullPath, const std::string& text)
{
	CreateDirectoryA(GamePath("BBCF_IM").c_str(), NULL);
	CreateDirectoryA(GamePath(kCombosFolder).c_str(), NULL);
	CreateDirectoryA(GamePath(kMovesFolder).c_str(), NULL);

	// Never overwrite: the player's own files are the one thing here that cannot be regenerated.
	if (fullPath.empty() || GetFileAttributesA(fullPath.c_str()) != INVALID_FILE_ATTRIBUTES)
		return false;

	std::ofstream file(fullPath, std::ios::out | std::ios::binary);
	if (!file)
	{
		LOG(2, "[ComboList] Could not create '%s'\n", fullPath.c_str());
		return false;
	}
	file << text;
	return true;
}

bool ComboLibrary::CreateStarterFile()
{
	if (isCharacterIndexOutOfBound(m_charIndex))
		return false;

	const std::string name = getCharacterNameByIndexA(m_charIndex);
	const bool written = WriteNewFile(m_combosFile.FullPath(), ReplaceAll(kStarterText, "%CHAR%", name));
	m_combosFile.Refresh(true);
	ParseCombos();
	++m_generation;
	return written;
}

bool ComboLibrary::CreateMovesFile(const std::vector<std::string>& registeredLines, const std::vector<std::string>& specialStates)
{
	if (isCharacterIndexOutOfBound(m_charIndex))
		return false;

	const std::string name = getCharacterNameByIndexA(m_charIndex);
	std::string text = ReplaceAll(kMovesHeader, "%CHAR%", name);
	text += "#\n# Read from the game's own move list (already used; no need to copy these). A \"?\" line\n"
		"# has inputs the window could not name: add it below with its notation.\n";
	if (registeredLines.empty())
		text += "#   (none found)\n";
	for (const std::string& line : registeredLines)
		text += "#   " + line + "\n";
	text += "#\n# Every special state in " + name + "'s script, for reference. Many are follow-ups or effects\n"
		"# rather than moves you input; the move is usually the first state of a family.\n";
	if (specialStates.empty())
		text += "#   (The script could not be read, so the list is empty.)\n";
	for (const std::string& state : specialStates)
		text += "#   " + state + "\n";
	text += "\n";

	const bool written = WriteNewFile(m_movesFile.FullPath(), text);
	m_movesFile.Refresh(true);
	++m_generation;
	return written;
}

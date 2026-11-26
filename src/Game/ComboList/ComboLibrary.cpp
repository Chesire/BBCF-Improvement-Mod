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

	const char* const kStarterText =
		"# Combo list for %CHAR%\n"
		"# Shown by the mod's \"Combo list\" window in training mode (F1 > Overlays).\n"
		"# The window re-reads this file when you save it, so you can edit it while the game runs.\n"
		"#\n"
		"# combo: <name>    starts a new combo\n"
		"# note: <text>     shown under the combo name\n"
		"# Any other line is a list of steps separated by '>'.\n"
		"# A combo can spread its steps over several lines.\n"
		"# Lines starting with # are ignored.\n"
		"\n"
		"combo: Example - replace me\n"
		"note: Placeholder steps, not a real route.\n"
		"5A > 5B > 5C\n"
		"2C > 6A\n";

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

std::string ComboLibrary::RelativePathFor(int charIndex)
{
	if (isCharacterIndexOutOfBound(charIndex))
		return std::string();
	return std::string(kCombosFolder) + "\\" + getCharacterNameByIndexA(charIndex) + ".txt";
}

void ComboLibrary::Load(int charIndex)
{
	m_charIndex = charIndex;
	m_relativePath = RelativePathFor(charIndex);
	m_fullPath = m_relativePath.empty() ? std::string() : GamePath(m_relativePath);
	ReadFile();
}

bool ComboLibrary::ReadStamp(uint64_t& outWriteTime, uint64_t& outSize) const
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

void ComboLibrary::ReadFile()
{
	++m_generation;
	m_combos.clear();
	m_issues.clear();
	m_writeTime = 0;
	m_size = 0;
	m_fileExists = ReadStamp(m_writeTime, m_size);
	if (!m_fileExists)
		return;

	std::ifstream file(m_fullPath, std::ios::in | std::ios::binary);
	if (!file)
	{
		m_issues.push_back({ 0, "The file exists but could not be opened." });
		LOG(2, "[ComboList] Could not open '%s'\n", m_fullPath.c_str());
		return;
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	ComboParser::Parse(buffer.str(), m_combos, m_issues);
	LOG(2, "[ComboList] Loaded %d combos (%d issues) from '%s'\n",
		static_cast<int>(m_combos.size()), static_cast<int>(m_issues.size()), m_fullPath.c_str());
}

bool ComboLibrary::ReloadIfChanged()
{
	if (m_fullPath.empty())
		return false;

	uint64_t writeTime = 0;
	uint64_t size = 0;
	const bool exists = ReadStamp(writeTime, size);
	if (exists == m_fileExists && writeTime == m_writeTime && size == m_size)
		return false;

	ReadFile();
	return true;
}

bool ComboLibrary::CreateStarterFile()
{
	if (m_fullPath.empty() || isCharacterIndexOutOfBound(m_charIndex))
		return false;

	CreateDirectoryA(GamePath("BBCF_IM").c_str(), NULL);
	CreateDirectoryA(GamePath(kCombosFolder).c_str(), NULL);

	// Never overwrite: the user's combos are the one thing here that cannot be regenerated.
	if (GetFileAttributesA(m_fullPath.c_str()) != INVALID_FILE_ATTRIBUTES)
	{
		ReadFile();
		return false;
	}

	std::ofstream file(m_fullPath, std::ios::out | std::ios::binary);
	if (!file)
	{
		LOG(2, "[ComboList] Could not create '%s'\n", m_fullPath.c_str());
		return false;
	}
	file << ReplaceAll(kStarterText, "%CHAR%", getCharacterNameByIndexA(m_charIndex));
	file.close();

	ReadFile();
	return m_fileExists;
}

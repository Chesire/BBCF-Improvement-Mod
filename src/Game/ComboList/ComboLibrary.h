#pragma once
#include "ComboParser.h"

#include <cstdint>
#include <string>
#include <vector>

// The combos for one character, read from BBCF_IM\Combos\<Character>.txt next to BBCF.exe.
// The file is re-read when it changes on disk, so it can be edited while the game runs.
class ComboLibrary
{
public:
	// Loads the file for this character. Safe to call with the same index again; it reloads.
	void Load(int charIndex);

	// Re-reads the file if its timestamp or size changed. Returns true if it reloaded.
	bool ReloadIfChanged();

	// Writes a commented starter file for the loaded character if none exists yet.
	bool CreateStarterFile();

	int CharIndex() const { return m_charIndex; }
	bool FileExists() const { return m_fileExists; }
	const std::string& RelativePath() const { return m_relativePath; }
	const std::vector<Combo>& Combos() const { return m_combos; }
	const std::vector<ComboFileIssue>& Issues() const { return m_issues; }

	static std::string RelativePathFor(int charIndex);

private:
	bool ReadStamp(uint64_t& outWriteTime, uint64_t& outSize) const;
	void ReadFile();

	int m_charIndex = -1;
	bool m_fileExists = false;
	uint64_t m_writeTime = 0;
	uint64_t m_size = 0;
	std::string m_relativePath;
	std::string m_fullPath;
	std::vector<Combo> m_combos;
	std::vector<ComboFileIssue> m_issues;
};

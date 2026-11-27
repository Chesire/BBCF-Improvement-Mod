#pragma once
#include "ComboParser.h"

#include <cstdint>
#include <string>
#include <vector>

// One text file next to BBCF.exe that is re-read when it changes on disk, so it can be
// edited while the game runs.
class WatchedTextFile
{
public:
	void SetPath(const std::string& relativePath);
	// Re-reads the file if its timestamp or size changed. Returns true if it reloaded.
	bool Refresh(bool force = false);

	bool Exists() const { return m_exists; }
	const std::string& Text() const { return m_text; }
	const std::string& RelativePath() const { return m_relativePath; }
	const std::string& FullPath() const { return m_fullPath; }

private:
	bool ReadStamp(uint64_t& outWriteTime, uint64_t& outSize) const;

	std::string m_relativePath;
	std::string m_fullPath;
	bool m_exists = false;
	bool m_read = false;
	uint64_t m_writeTime = 0;
	uint64_t m_size = 0;
	std::string m_text;
};

// The files for one character, in BBCF_IM\Combos next to BBCF.exe:
//   <Character>.txt        the combos (ComboParser format)
//   Moves\<Character>.txt  "notation = StateName" lines for moves the window cannot work out
//                          by itself, i.e. the character's specials (MoveTable)
class ComboLibrary
{
public:
	// Loads the files for this character. Safe to call with the same index again; it reloads.
	void Load(int charIndex);

	// Re-reads either file if it changed. Returns true if anything reloaded.
	bool ReloadIfChanged();

	// Writes a commented starter combo file for the loaded character if none exists yet.
	bool CreateStarterFile();

	// Writes a moves file listing the given special states as comments, if none exists yet.
	bool CreateMovesFile(const std::vector<std::string>& specialStates);

	int CharIndex() const { return m_charIndex; }
	bool FileExists() const { return m_combosFile.Exists(); }
	const std::string& RelativePath() const { return m_combosFile.RelativePath(); }
	const std::vector<Combo>& Combos() const { return m_combos; }
	const std::vector<ComboFileIssue>& Issues() const { return m_issues; }

	bool MovesFileExists() const { return m_movesFile.Exists(); }
	const std::string& MovesRelativePath() const { return m_movesFile.RelativePath(); }
	const std::string& MovesText() const { return m_movesFile.Text(); }

	// Changes every time either file is (re)read, so holders of indices know to start over.
	unsigned Generation() const { return m_generation; }

	static std::string RelativePathFor(int charIndex);
	static std::string MovesRelativePathFor(int charIndex);

private:
	void ParseCombos();
	static bool WriteNewFile(const std::string& fullPath, const std::string& text);

	int m_charIndex = -1;
	unsigned m_generation = 0;
	WatchedTextFile m_combosFile;
	WatchedTextFile m_movesFile;
	std::vector<Combo> m_combos;
	std::vector<ComboFileIssue> m_issues;
};

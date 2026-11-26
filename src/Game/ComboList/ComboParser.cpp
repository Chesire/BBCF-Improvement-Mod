#include "ComboParser.h"

#include <cctype>

namespace
{
	bool IsSpace(char c)
	{
		return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
	}

	std::string Trim(const std::string& s)
	{
		size_t begin = 0;
		size_t end = s.size();
		while (begin < end && IsSpace(s[begin]))
			++begin;
		while (end > begin && IsSpace(s[end - 1]))
			--end;
		return s.substr(begin, end - begin);
	}

	// Case-insensitive "does line start with keyword" (keyword is lower-case ASCII).
	bool StartsWithKeyword(const std::string& line, const char* keyword, std::string& outRest)
	{
		size_t i = 0;
		for (; keyword[i] != '\0'; ++i)
		{
			if (i >= line.size())
				return false;
			const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(line[i])));
			if (c != keyword[i])
				return false;
		}
		outRest = Trim(line.substr(i));
		return true;
	}

	// "5B -" (left over from "5B -> 5C") becomes "5B".
	std::string StripArrowTail(const std::string& piece)
	{
		std::string s = Trim(piece);
		while (!s.empty() && (s.back() == '-' || s.back() == '='))
			s.pop_back();
		return Trim(s);
	}

	void AddSteps(const std::string& line, Combo& combo)
	{
		size_t start = 0;
		while (start <= line.size())
		{
			size_t sep = line.find('>', start);
			if (sep == std::string::npos)
				sep = line.size();

			const std::string piece = StripArrowTail(line.substr(start, sep - start));
			if (!piece.empty())
			{
				ComboStep step;
				const size_t bar = piece.find('|');
				if (bar == std::string::npos)
				{
					step.notation = piece;
				}
				else
				{
					step.notation = Trim(piece.substr(0, bar));
					step.matchKey = Trim(piece.substr(bar + 1));
					if (step.notation.empty())
						step.notation = step.matchKey;
				}
				if (!step.notation.empty())
					combo.steps.push_back(step);
			}

			start = sep + 1;
		}
	}

	void FinishCombo(std::vector<Combo>& combos, std::vector<ComboFileIssue>& issues, int startLine)
	{
		if (!combos.empty() && combos.back().steps.empty())
		{
			issues.push_back({ startLine, "Combo \"" + combos.back().name + "\" has no steps, so it was skipped." });
			combos.pop_back();
		}
	}
}

namespace ComboParser
{
	void Parse(const std::string& rawText, std::vector<Combo>& outCombos, std::vector<ComboFileIssue>& outIssues)
	{
		outCombos.clear();
		outIssues.clear();

		std::string text = rawText;
		// UTF-8 BOM, which Notepad likes to add.
		if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF
			&& static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF)
		{
			text.erase(0, 3);
		}

		bool inCombo = false;
		int comboStartLine = 0;
		int lineNumber = 0;
		size_t pos = 0;

		while (pos <= text.size())
		{
			size_t eol = text.find('\n', pos);
			if (eol == std::string::npos)
				eol = text.size();
			const std::string line = Trim(text.substr(pos, eol - pos));
			pos = eol + 1;
			++lineNumber;

			if (line.empty() || line[0] == '#')
				continue;

			std::string rest;
			if (StartsWithKeyword(line, "combo:", rest))
			{
				if (inCombo)
					FinishCombo(outCombos, outIssues, comboStartLine);

				Combo combo;
				combo.name = rest.empty() ? "Combo " + std::to_string(outCombos.size() + 1) : rest;
				outCombos.push_back(combo);
				inCombo = true;
				comboStartLine = lineNumber;
				continue;
			}

			if (StartsWithKeyword(line, "note:", rest))
			{
				if (!inCombo)
				{
					outIssues.push_back({ lineNumber, "Note is not under a \"combo:\" line, so it was ignored." });
					continue;
				}
				if (!rest.empty())
					outCombos.back().notes.push_back(rest);
				continue;
			}

			if (!inCombo)
			{
				// Keep the steps rather than dropping them; the warning says how to name them.
				outIssues.push_back({ lineNumber, "Steps before the first \"combo:\" line were put in a combo called \"Untitled\"." });
				Combo combo;
				combo.name = "Untitled";
				outCombos.push_back(combo);
				inCombo = true;
				comboStartLine = lineNumber;
			}

			AddSteps(line, outCombos.back());
		}

		if (inCombo)
			FinishCombo(outCombos, outIssues, comboStartLine);
	}
}

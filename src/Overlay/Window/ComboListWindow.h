#pragma once
#include "IWindow.h"

#include "Game/ComboList/ComboLibrary.h"
#include "Game/ComboList/ComboTracker.h"
#include "Game/ComboList/MoveTable.h"

// Training-mode window that shows combos written in BBCF_IM\Combos\<Character>.txt and ticks
// the steps off as they land. It only draws while a training match is running; outside of one
// it stays "open" but hidden, the same way Frame history behaves, so the user does not have to
// reopen it every session.
class ComboListWindow : public IWindow
{
public:
	ComboListWindow(const std::string& windowTitle, bool windowClosable,
		ImGuiWindowFlags windowFlags = 0)
		: IWindow(windowTitle, windowClosable, windowFlags) {}
	~ComboListWindow() override = default;

	void Update() override;
	bool WantsMouseCursor() const override;

protected:
	void BeforeDraw() override;
	void Draw() override;
	void AfterDraw() override;

private:
	static bool IsUsableNow();
	int WatchedCharIndex() const;
	void SyncLibrary();
	void LoadMoveList();
	void ApplyMovesFile();
	void ResolveSelectedCombo();
	void DrawBody();
	void DrawHeader();
	void DrawMissingFile();
	void DrawIssues();
	void DrawSelector();
	void UpdateTracker();
	void DrawStatus();
	void DrawSteps(const Combo& combo);
	void DrawResolutionNotes();
	void DrawMoveNames(const Combo& combo);
	const char* StepMarker(size_t index, bool tracking) const;
	ImVec4 StepColor(size_t index, bool tracking) const;
	void DrawTextSize();

	ComboLibrary m_library;
	int m_watchedSide = 0;      // 0 = P1, 1 = P2
	int m_selectedCombo = 0;
	int m_framesUntilPoll = 0;
	bool m_oneLine = false;
	bool m_showMoveNames = false;
	float m_fontScale = 1.0f;

	// The watched character's moves: built from its script once per character, plus the
	// moves file, re-applied whenever that file changes.
	MoveTable m_moves;
	int m_moveListChar = -1;
	int m_moveListSide = -1;
	std::vector<std::string> m_specialStates; // for the moves file template
	std::vector<int> m_movesFileBadLines;
	unsigned m_movesGeneration = 0;
	unsigned m_tableVersion = 0;

	// The selected combo's steps, resolved against m_moves.
	std::vector<ResolvedStep> m_resolved;
	int m_resolvedCombo = -1;
	unsigned m_resolvedGeneration = 0;
	unsigned m_resolvedTableVersion = 0;

	ComboTracker m_tracker;
	int m_trackedSide = -1;
};

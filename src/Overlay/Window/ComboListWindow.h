#pragma once
#include "IWindow.h"

#include "Game/ComboList/ComboLibrary.h"

// Training-mode window that shows combos written in BBCF_IM\Combos\<Character>.txt.
// It only draws while a training match is running; outside of one it stays "open" but hidden,
// the same way Frame history behaves, so the user does not have to reopen it every session.
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
	void DrawBody();
	void DrawHeader();
	void DrawMissingFile();
	void DrawIssues();
	void DrawSelector();
	void DrawSteps(const Combo& combo);
	void DrawTextSize();

	ComboLibrary m_library;
	int m_watchedSide = 0;      // 0 = P1, 1 = P2
	int m_selectedCombo = 0;
	int m_framesUntilPoll = 0;
	bool m_oneLine = false;
	float m_fontScale = 1.0f;
};

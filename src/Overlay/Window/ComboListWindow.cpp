#include "ComboListWindow.h"

#include "Core/interfaces.h"
#include "Game/characters.h"
#include "Game/gamestates.h"
#include "Overlay/imgui_utils.h"

#include <imgui.h>

#include <cstring>

namespace
{
	// How often to check the combo file for edits. Half a second is quick enough to feel
	// live while editing, and one file stat per 30 frames costs nothing.
	const int kPollIntervalFrames = 30;

	const float kWrapWidth = 340.0f;

	const ImVec4 kWarningColor(1.0f, 0.8f, 0.3f, 1.0f);
	const ImVec4 kStepNumberColor(0.6f, 0.6f, 0.6f, 1.0f);
	const ImVec4 kLandedColor(0.35f, 0.9f, 0.4f, 1.0f);
	const ImVec4 kMissedColor(1.0f, 0.4f, 0.35f, 1.0f);
	const ImVec4 kNextStepColor(1.0f, 0.85f, 0.3f, 1.0f);
}

bool ComboListWindow::IsUsableNow()
{
	return g_gameVals.pGameMode && g_gameVals.pGameState
		&& isInMatch()
		&& *g_gameVals.pGameMode == GameMode_Training;
}

void ComboListWindow::Update()
{
	// Stay open but draw nothing outside training, so the window comes back by itself.
	if (!IsUsableNow())
		return;

	IWindow::Update();
}

bool ComboListWindow::WantsMouseCursor() const
{
	return m_windowOpen && IsUsableNow();
}

void ComboListWindow::BeforeDraw()
{
	ImGui::SetNextWindowPos(ImVec2(40, 160), ImGuiCond_FirstUseEver);
	// Real font size rather than SetWindowFontScale, so large text is crisp on a TV.
	ImGui::PushFont(NULL, ImGui::GetStyle().FontSizeBase * m_fontScale);
}

void ComboListWindow::AfterDraw()
{
	ImGui::PopFont();
}

int ComboListWindow::WatchedCharIndex() const
{
	const Player& player = m_watchedSide == 0 ? g_interfaces.player1 : g_interfaces.player2;
	if (player.IsCharDataNullPtr())
		return -1;

	const int charIndex = player.GetData()->charIndex;
	return isCharacterIndexOutOfBound(charIndex) ? -1 : charIndex;
}

void ComboListWindow::SyncLibrary()
{
	const int charIndex = WatchedCharIndex();
	if (charIndex < 0)
		return;

	if (charIndex != m_library.CharIndex())
	{
		m_library.Load(charIndex);
		m_selectedCombo = 0;
		m_framesUntilPoll = kPollIntervalFrames;
		return;
	}

	if (--m_framesUntilPoll <= 0)
	{
		m_framesUntilPoll = kPollIntervalFrames;
		m_library.ReloadIfChanged();
	}

	const int count = static_cast<int>(m_library.Combos().size());
	if (m_selectedCombo >= count)
		m_selectedCombo = count > 0 ? count - 1 : 0;
}

void ComboListWindow::Draw()
{
	// The window auto-sizes, so text needs a fixed width to wrap at; window-edge wrapping
	// (TextWrapped) would just let long notes stretch the window instead.
	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kWrapWidth * m_fontScale);
	DrawBody();
	ImGui::PopTextWrapPos();
}

void ComboListWindow::DrawBody()
{
	SyncLibrary();

	if (WatchedCharIndex() < 0)
	{
		ImGui::TextDisabled("Waiting for the character to load...");
		return;
	}

	DrawHeader();

	if (!m_library.FileExists())
	{
		DrawMissingFile();
		return;
	}

	DrawIssues();

	const std::vector<Combo>& combos = m_library.Combos();
	if (combos.empty())
	{
		ImGui::Text("No combos in this file yet. Add a \"combo:\" line followed by its steps.");
		ImGui::TextDisabled("%s", m_library.RelativePath().c_str());
		return;
	}

	const Combo& combo = combos[m_selectedCombo];
	UpdateTracker(combo);

	DrawSelector();
	ImGui::Separator();
	DrawStatus(combo);
	DrawSteps(combo);

	ImGui::Separator();
	DrawTextSize();
	ImGui::Checkbox("Show move names", &m_showMoveNames);
	ImGui::ShowHelpMarkerSameLine("Shows the game's name for each move that lands. Put a name after a '|' on a step (\"5B | NmlAtk5B\") so only that move ticks the step off.");
	if (m_showMoveNames)
		DrawMoveNames(combo);
}

void ComboListWindow::UpdateTracker(const Combo& combo)
{
	if (m_selectedCombo != m_trackedCombo || m_watchedSide != m_trackedSide
		|| m_library.Generation() != m_trackedGeneration)
	{
		m_tracker.Reset(&combo);
		m_trackedCombo = m_selectedCombo;
		m_trackedSide = m_watchedSide;
		m_trackedGeneration = m_library.Generation();
	}

	const Player& attackerPlayer = m_watchedSide == 0 ? g_interfaces.player1 : g_interfaces.player2;
	const Player& defenderPlayer = m_watchedSide == 0 ? g_interfaces.player2 : g_interfaces.player1;
	if (attackerPlayer.IsCharDataNullPtr() || defenderPlayer.IsCharDataNullPtr() || !g_gameVals.pFrameCount)
		return;

	const CharData* attacker = attackerPlayer.GetData();
	const CharData* defender = defenderPlayer.GetData();

	ComboTrackerFrame frame;
	// Bounded copy: the game's buffer is fixed-size and not guaranteed to be terminated.
	frame.attackerAction.assign(attacker->currentAction,
		strnlen(attacker->currentAction, sizeof(attacker->currentAction)));
	frame.attackerStateChanges = attacker->stateChangedCount;
	frame.defenderHitCount = defender->hitCount;
	frame.frameCount = *g_gameVals.pFrameCount;
	m_tracker.OnFrame(frame, combo);
}

void ComboListWindow::DrawStatus(const Combo& combo)
{
	size_t hitSteps = 0;
	size_t landed = 0;
	for (size_t i = 0; i < combo.steps.size(); ++i)
	{
		if (ComboTracker::IsNonHitStep(combo.steps[i]))
			continue;
		++hitSteps;
		if (m_tracker.StepState(i) == ComboStepState::Landed)
			++landed;
	}

	switch (m_tracker.RunState())
	{
	case ComboRunState::Idle:
		ImGui::TextDisabled("Waiting for the first hit");
		break;
	case ComboRunState::InProgress:
		ImGui::TextColored(kNextStepColor, "%d/%d", static_cast<int>(landed), static_cast<int>(hitSteps));
		break;
	case ComboRunState::Completed:
		ImGui::TextColored(kLandedColor, "Complete! %d/%d", static_cast<int>(landed), static_cast<int>(hitSteps));
		break;
	case ComboRunState::Dropped:
		ImGui::TextColored(kMissedColor, "Dropped at %d/%d", static_cast<int>(landed), static_cast<int>(hitSteps));
		break;
	case ComboRunState::OffRoute:
		ImGui::TextColored(kMissedColor, "Off route: %s landed", m_tracker.OffRouteAction().c_str());
		break;
	}
}

void ComboListWindow::DrawHeader()
{
	ImGui::RadioButton("P1", &m_watchedSide, 0);
	ImGui::SameLine();
	ImGui::RadioButton("P2", &m_watchedSide, 1);
	ImGui::SameLine();
	ImGui::Text("%s", getCharacterNameByIndexA(m_library.CharIndex()).c_str());

	ImGui::SameLine();
	if (ImGui::SmallButton("Reload"))
	{
		m_library.Load(m_library.CharIndex());
	}
}

void ComboListWindow::DrawMissingFile()
{
	const std::string& name = getCharacterNameByIndexA(m_library.CharIndex());
	ImGui::Text("No combo file for %s yet.", name.c_str());
	ImGui::TextDisabled("%s", m_library.RelativePath().c_str());
	if (ImGui::Button("Create a starter file"))
	{
		m_library.CreateStarterFile();
	}
	ImGui::ShowHelpMarkerSameLine("Writes a commented example file in your game folder. Edit it in any text editor; this window picks up changes when you save.");
}

void ComboListWindow::DrawIssues()
{
	const std::vector<ComboFileIssue>& issues = m_library.Issues();
	if (issues.empty())
		return;

	ImGui::PushStyleColor(ImGuiCol_Text, kWarningColor);
	const bool open = ImGui::TreeNode("ComboListIssues", "%d problem(s) in the file", static_cast<int>(issues.size()));
	ImGui::PopStyleColor();
	if (!open)
		return;

	for (const ComboFileIssue& issue : issues)
	{
		if (issue.line > 0)
			ImGui::Text("Line %d: %s", issue.line, issue.message.c_str());
		else
			ImGui::Text("%s", issue.message.c_str());
	}
	ImGui::TreePop();
}

void ComboListWindow::DrawSelector()
{
	const std::vector<Combo>& combos = m_library.Combos();
	const int count = static_cast<int>(combos.size());

	if (ImGui::ArrowButton("##ComboListPrev", ImGuiDir_Left))
		m_selectedCombo = (m_selectedCombo + count - 1) % count;
	ImGui::SameLine();

	ImGui::SetNextItemWidth(220.0f * m_fontScale);
	if (ImGui::BeginCombo("##ComboListPicker", combos[m_selectedCombo].name.c_str()))
	{
		for (int i = 0; i < count; ++i)
		{
			ImGui::PushID(i);
			const bool selected = i == m_selectedCombo;
			if (ImGui::Selectable(combos[i].name.c_str(), selected))
				m_selectedCombo = i;
			if (selected)
				ImGui::SetItemDefaultFocus();
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	if (ImGui::ArrowButton("##ComboListNext", ImGuiDir_Right))
		m_selectedCombo = (m_selectedCombo + 1) % count;

	ImGui::SameLine();
	ImGui::TextDisabled("%d/%d", m_selectedCombo + 1, count);
}

void ComboListWindow::DrawSteps(const Combo& combo)
{
	for (const std::string& note : combo.notes)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::Text("%s", note.c_str());
		ImGui::PopStyleColor();
	}

	const bool tracking = m_tracker.RunState() != ComboRunState::Idle;

	if (m_oneLine)
	{
		// Coloured pieces laid out by hand, since one Text() call can only have one colour.
		const float wrapX = ImGui::GetCursorPosX() + kWrapWidth * m_fontScale;
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		for (size_t i = 0; i < combo.steps.size(); ++i)
		{
			const std::string piece = (i > 0 ? "> " : "") + combo.steps[i].notation;
			if (i > 0)
			{
				ImGui::SameLine(0.0f, spacing);
				if (ImGui::GetCursorPosX() + ImGui::CalcTextSize(piece.c_str()).x > wrapX)
					ImGui::NewLine();
			}
			ImGui::TextColored(StepColor(combo, i, tracking), "%s", piece.c_str());
		}
		return;
	}

	for (size_t i = 0; i < combo.steps.size(); ++i)
	{
		ImGui::TextColored(kStepNumberColor, "%s%2d.", StepMarker(i, tracking), static_cast<int>(i + 1));
		ImGui::SameLine();
		ImGui::TextColored(StepColor(combo, i, tracking), "%s", combo.steps[i].notation.c_str());
	}
}

const char* ComboListWindow::StepMarker(size_t index, bool tracking) const
{
	switch (m_tracker.StepState(index))
	{
	case ComboStepState::Landed: return "[x]";
	case ComboStepState::Missed: return "[!]";
	default: break;
	}
	if (tracking && m_tracker.RunState() == ComboRunState::InProgress && index == m_tracker.NextStep())
		return " > ";
	return "[ ]";
}

ImVec4 ComboListWindow::StepColor(const Combo& combo, size_t index, bool tracking) const
{
	switch (m_tracker.StepState(index))
	{
	case ComboStepState::Landed: return kLandedColor;
	case ComboStepState::Missed: return kMissedColor;
	default: break;
	}
	if (tracking && m_tracker.RunState() == ComboRunState::InProgress && index == m_tracker.NextStep())
		return kNextStepColor;
	if (ComboTracker::IsNonHitStep(combo.steps[index]))
		return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
	return ImGui::GetStyleColorVec4(ImGuiCol_Text);
}

void ComboListWindow::DrawMoveNames(const Combo& combo)
{
	const Player& attackerPlayer = m_watchedSide == 0 ? g_interfaces.player1 : g_interfaces.player2;
	if (!attackerPlayer.IsCharDataNullPtr())
	{
		const CharData* attacker = attackerPlayer.GetData();
		const std::string now(attacker->currentAction, strnlen(attacker->currentAction, sizeof(attacker->currentAction)));
		ImGui::TextDisabled("Now:");
		ImGui::SameLine();
		ImGui::TextUnformatted(now.c_str());
	}

	const std::vector<std::string>& moves = m_tracker.LandedMoves();
	if (moves.empty())
	{
		ImGui::TextDisabled("Land a combo to list its moves.");
		return;
	}

	ImGui::TextDisabled("Last combo:");
	for (size_t i = 0; i < moves.size(); ++i)
		ImGui::Text("%2d. %s", static_cast<int>(i + 1), moves[i].c_str());

	if (ImGui::Button("Copy as steps"))
	{
		// Pair the landed moves with this combo's hitting steps, in order, so the result can
		// be pasted over the combo's step lines as-is. Extra moves are written bare.
		std::string text;
		size_t step = 0;
		for (size_t i = 0; i < moves.size(); ++i)
		{
			while (step < combo.steps.size() && ComboTracker::IsNonHitStep(combo.steps[step]))
			{
				text += combo.steps[step].notation + " | - > ";
				++step;
			}
			if (step < combo.steps.size())
				text += combo.steps[step++].notation + " | ";
			text += moves[i];
			if (i + 1 < moves.size())
				text += " > ";
		}
		ImGui::SetClipboardText(text.c_str());
	}
	ImGui::ShowHelpMarkerSameLine("Copies the moves as \"notation | name > ...\", matched up with this combo's steps in order. Check the pairing before pasting it into the file.");
}

void ComboListWindow::DrawTextSize()
{
	ImGui::Checkbox("One line", &m_oneLine);
	ImGui::SameLine();
	ImGui::TextDisabled("Size");
	ImGui::SameLine();
	if (ImGui::SmallButton(" - "))
		m_fontScale = m_fontScale > 0.8f ? m_fontScale - 0.2f : m_fontScale;
	ImGui::SameLine();
	if (ImGui::SmallButton(" + "))
		m_fontScale = m_fontScale < 2.8f ? m_fontScale + 0.2f : m_fontScale;
}

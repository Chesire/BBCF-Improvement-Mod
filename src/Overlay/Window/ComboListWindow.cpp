#include "ComboListWindow.h"

#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/characters.h"
#include "Game/gamestates.h"
#include "Game/Scr/ScrStateNames.h"
#include "Game/Scr/ScrStateReader.h"
#include "Overlay/imgui_utils.h"

#include <imgui.h>

#include <cstring>
#include <set>

namespace
{
	// How often to check the files for edits. Half a second is quick enough to feel live while
	// editing, and two file stats per 30 frames cost nothing.
	const int kPollIntervalFrames = 30;

	const float kWrapWidth = 340.0f;

	const ImVec4 kWarningColor(1.0f, 0.8f, 0.3f, 1.0f);
	const ImVec4 kStepNumberColor(0.6f, 0.6f, 0.6f, 1.0f);
	const ImVec4 kLandedColor(0.35f, 0.9f, 0.4f, 1.0f);
	const ImVec4 kMissedColor(1.0f, 0.4f, 0.35f, 1.0f);
	const ImVec4 kNextStepColor(1.0f, 0.85f, 0.3f, 1.0f);

	std::string StateOf(const CharData* data)
	{
		// Bounded copy: the game's buffer is fixed-size and not guaranteed to be terminated.
		return std::string(data->currentAction, strnlen(data->currentAction, sizeof(data->currentAction)));
	}

	// "236A (Assault_A)", "2A (NmlAtk2A)", or just the name when nothing better is known.
	std::string Describe(const std::string& state, const MoveTable& table)
	{
		const std::string notation = table.NotationFor(state);
		if (notation != state)
			return notation + " (" + state + ")";
		const std::string display = MoveTable::DisplayName(state);
		return display == state ? state : display + " (" + state + ")";
	}

	const char* MatchSuffix(const ResolvedStep& step)
	{
		if (step.match == StepMatch::Exact && step.notInMoveList)
			return " !";
		if (step.match == StepMatch::AnySpecial || step.match == StepMatch::AnyMove)
			return " ?";
		return "";
	}

	void StepTooltip(const ResolvedStep& step)
	{
		if (!ImGui::IsItemHovered())
			return;

		ImGui::BeginTooltip();
		switch (step.match)
		{
		case StepMatch::Exact:
		{
			std::string keys;
			for (const std::string& key : step.keys)
				keys += (keys.empty() ? "" : " or ") + key;
			ImGui::Text("Ticked off by %s", keys.c_str());
			if (step.notInMoveList)
				ImGui::TextColored(kWarningColor, "This character has no such state. Typo?");
			if (step.directionUnchecked)
				ImGui::TextDisabled("The game starts this same move from several inputs (e.g. 5D and 6D),\nso which direction you held is not checked.");
			break;
		}
		case StepMatch::AnySpecial:
			ImGui::Text("Not mapped yet: any special ticks it off.");
			ImGui::TextDisabled("Add it to the moves file to check it exactly.");
			break;
		case StepMatch::AnyMove:
			ImGui::Text("Not understood: any move ticks it off.");
			break;
		case StepMatch::AutoPass:
			ImGui::Text("Movement: not checked. Passes when the step before it lands.");
			break;
		}
		ImGui::EndTooltip();
	}
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

void ComboListWindow::LoadMoveList()
{
	// The script is read once per character (and side): it is the list of every state the
	// character has, plus the cancel lists that say which of them are moves of their own.
	std::vector<std::string> names;
	std::set<std::string> cancelTargets;
	std::vector<RegisteredMove> registered;
	m_specialStates.clear();

	std::vector<scrState*> states = parse_scr(GetBbcfBaseAdress(), m_watchedSide + 1);
	for (scrState* state : states)
	{
		names.push_back(state->name);
		cancelTargets.insert(state->whiff_cancel.begin(), state->whiff_cancel.end());
		cancelTargets.insert(state->hit_or_block_cancel.begin(), state->hit_or_block_cancel.end());
		if (ScrStateNames::Categorize(state->name) == ScrStateNames::Category::Special)
			m_specialStates.push_back(state->name);
		delete state;
	}

	m_moves.SetStates(names, std::vector<std::string>(cancelTargets.begin(), cancelTargets.end()));
	// The moves the script registers with their inputs: where 236A = Assault_A comes from.
	for (const scrMoveRegistration& reg : parse_scr_move_registrations(GetBbcfBaseAdress(), m_watchedSide + 1))
		registered.push_back({ reg.name, reg.type, reg.inputs });
	m_moves.SetRegisteredMoves(registered);
	m_moveListChar = m_library.CharIndex();
	m_moveListSide = m_watchedSide;
	m_movesGeneration = 0; // force the moves file to be applied again
	LOG(2, "[ComboList] Move list for P%d: %d states, %d cancel targets, %d specials, %d registered moves (%d with a notation).\n",
		m_watchedSide + 1, static_cast<int>(names.size()), static_cast<int>(cancelTargets.size()),
		static_cast<int>(m_specialStates.size()), static_cast<int>(m_moves.RegisteredCount()),
		static_cast<int>(m_moves.RegisteredWithNotation()));
}

void ComboListWindow::ApplyMovesFile()
{
	m_movesFileBadLines = m_moves.SetUserMappings(m_library.MovesText());
	m_movesGeneration = m_library.Generation();
	++m_tableVersion;
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
	}
	else if (--m_framesUntilPoll <= 0)
	{
		m_framesUntilPoll = kPollIntervalFrames;
		m_library.ReloadIfChanged();
	}

	if (m_moveListChar != charIndex || m_moveListSide != m_watchedSide)
		LoadMoveList();
	if (m_movesGeneration != m_library.Generation())
		ApplyMovesFile();

	const int count = static_cast<int>(m_library.Combos().size());
	if (m_selectedCombo >= count)
		m_selectedCombo = count > 0 ? count - 1 : 0;
}

void ComboListWindow::ResolveSelectedCombo()
{
	if (m_resolvedCombo == m_selectedCombo && m_resolvedGeneration == m_library.Generation()
		&& m_resolvedTableVersion == m_tableVersion)
	{
		return;
	}

	m_resolved.clear();
	const Combo& combo = m_library.Combos()[m_selectedCombo];
	for (const ComboStep& step : combo.steps)
		m_resolved.push_back(m_moves.Resolve(step.notation, step.matchKey));

	m_resolvedCombo = m_selectedCombo;
	m_resolvedGeneration = m_library.Generation();
	m_resolvedTableVersion = m_tableVersion;
	m_tracker.Reset(m_resolved.size());
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
	ResolveSelectedCombo();
	UpdateTracker();

	DrawSelector();
	ImGui::Separator();
	DrawStatus();
	DrawSteps(combo);
	DrawResolutionNotes();

	ImGui::Separator();
	DrawTextSize();
	ImGui::Checkbox("Show move names", &m_showMoveNames);
	ImGui::ShowHelpMarkerSameLine("Lists the moves of the last combo with the game's name for each, and lets you copy that combo into the combo file.");
	if (m_showMoveNames)
		DrawMoveNames(combo);
}

void ComboListWindow::UpdateTracker()
{
	if (m_watchedSide != m_trackedSide)
	{
		m_tracker.Reset(m_resolved.size());
		m_trackedSide = m_watchedSide;
	}

	const Player& attackerPlayer = m_watchedSide == 0 ? g_interfaces.player1 : g_interfaces.player2;
	const Player& defenderPlayer = m_watchedSide == 0 ? g_interfaces.player2 : g_interfaces.player1;
	if (attackerPlayer.IsCharDataNullPtr() || defenderPlayer.IsCharDataNullPtr() || !g_gameVals.pFrameCount)
		return;

	const CharData* attacker = attackerPlayer.GetData();
	const CharData* defender = defenderPlayer.GetData();

	ComboTrackerFrame frame;
	frame.attackerAction = StateOf(attacker);
	frame.attackerStateChanges = attacker->stateChangedCount;
	frame.defenderHitCount = defender->hitCount;
	frame.frameCount = *g_gameVals.pFrameCount;
	m_tracker.OnFrame(frame, m_resolved, m_moves);
}

void ComboListWindow::DrawStatus()
{
	size_t hitSteps = 0;
	size_t landed = 0;
	for (size_t i = 0; i < m_resolved.size(); ++i)
	{
		if (m_resolved[i].match == StepMatch::AutoPass)
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
		ImGui::TextColored(kMissedColor, "Off route: %s landed", Describe(m_tracker.OffRouteMove(), m_moves).c_str());
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
		m_moveListChar = -1; // re-read the script too
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
		for (size_t i = 0; i < m_resolved.size(); ++i)
		{
			const std::string piece = (i > 0 ? "> " : "") + m_resolved[i].display + MatchSuffix(m_resolved[i]);
			if (i > 0)
			{
				ImGui::SameLine(0.0f, spacing);
				if (ImGui::GetCursorPosX() + ImGui::CalcTextSize(piece.c_str()).x > wrapX)
					ImGui::NewLine();
			}
			ImGui::TextColored(StepColor(i, tracking), "%s", piece.c_str());
			StepTooltip(m_resolved[i]);
		}
		return;
	}

	for (size_t i = 0; i < m_resolved.size(); ++i)
	{
		ImGui::TextColored(kStepNumberColor, "%s%2d.", StepMarker(i, tracking), static_cast<int>(i + 1));
		ImGui::SameLine();
		ImGui::TextColored(StepColor(i, tracking), "%s%s", m_resolved[i].display.c_str(), MatchSuffix(m_resolved[i]));
		StepTooltip(m_resolved[i]);
	}
}

void ComboListWindow::DrawResolutionNotes()
{
	std::string loose;
	std::string unknown;
	std::string missing;
	for (const ResolvedStep& step : m_resolved)
	{
		std::string* list = nullptr;
		if (step.match == StepMatch::AnySpecial)
			list = &loose;
		else if (step.match == StepMatch::AnyMove)
			list = &unknown;
		else if (step.match == StepMatch::Exact && step.notInMoveList)
			list = &missing;
		if (list && list->find(step.display) == std::string::npos)
			*list += (list->empty() ? "" : ", ") + step.display;
	}

	if (loose.empty() && unknown.empty() && missing.empty() && m_movesFileBadLines.empty())
		return;

	ImGui::Spacing();
	if (!missing.empty())
		ImGui::TextColored(kWarningColor, "Not in this character's moves (!): %s", missing.c_str());
	if (!loose.empty())
		ImGui::TextColored(kWarningColor, "Not mapped yet, any special counts (?): %s", loose.c_str());
	if (!unknown.empty())
		ImGui::TextColored(kWarningColor, "Not understood, any move counts (?): %s", unknown.c_str());
	if (!m_movesFileBadLines.empty())
	{
		std::string lines;
		for (int line : m_movesFileBadLines)
			lines += (lines.empty() ? "" : ", ") + std::to_string(line);
		ImGui::TextColored(kWarningColor, "Moves file lines not understood: %s", lines.c_str());
	}

	if (!loose.empty() || !unknown.empty())
	{
		ImGui::TextDisabled("Map them in %s", m_library.MovesRelativePath().c_str());
		if (!m_library.MovesFileExists())
		{
			if (ImGui::SmallButton("Create moves file"))
				m_library.CreateMovesFile(m_moves.RegisteredMoveLines(), m_specialStates);
			ImGui::ShowHelpMarkerSameLine("Writes the file listing, as comments, the moves read from the game (with their inputs) and every special state in this character's script, ready to fill in as \"236D = StateName\".");
		}
	}
}

const char* ComboListWindow::StepMarker(size_t index, bool tracking) const
{
	if (index < m_resolved.size() && m_resolved[index].match == StepMatch::AutoPass)
		return m_tracker.StepState(index) == ComboStepState::Landed ? "[x]" : " ~ "; // never checked
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

ImVec4 ComboListWindow::StepColor(size_t index, bool tracking) const
{
	if (index < m_resolved.size() && m_resolved[index].match == StepMatch::AutoPass)
		return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
	switch (m_tracker.StepState(index))
	{
	case ComboStepState::Landed: return kLandedColor;
	case ComboStepState::Missed: return kMissedColor;
	default: break;
	}
	if (tracking && m_tracker.RunState() == ComboRunState::InProgress && index == m_tracker.NextStep())
		return kNextStepColor;
	return ImGui::GetStyleColorVec4(ImGuiCol_Text);
}

void ComboListWindow::DrawMoveNames(const Combo& combo)
{
	const Player& attackerPlayer = m_watchedSide == 0 ? g_interfaces.player1 : g_interfaces.player2;
	if (!attackerPlayer.IsCharDataNullPtr())
	{
		ImGui::TextDisabled("State:");
		ImGui::SameLine();
		ImGui::TextUnformatted(Describe(StateOf(attackerPlayer.GetData()), m_moves).c_str());
	}
	if (!m_tracker.CurrentMove().empty())
	{
		ImGui::TextDisabled("Move:");
		ImGui::SameLine();
		ImGui::TextUnformatted(Describe(m_tracker.CurrentMove(), m_moves).c_str());
	}
	if (!m_moves.HasStates())
		ImGui::TextColored(kWarningColor, "The script could not be read; normals are not checked against it.");
	else if (m_moves.RegisteredCount() == 0)
		ImGui::TextColored(kWarningColor, "The game's move list was not found; specials need the moves file.");
	else
		ImGui::TextDisabled("Game move list: %d moves, %d with inputs read",
			static_cast<int>(m_moves.RegisteredCount()), static_cast<int>(m_moves.RegisteredWithNotation()));

	const std::vector<ComboHit>& hits = m_tracker.Hits();
	if (hits.empty())
	{
		ImGui::TextDisabled("Land a combo to list its moves.");
		return;
	}

	ImGui::TextDisabled("Last combo:");
	int moveNumber = 0;
	for (const ComboHit& hit : hits)
	{
		if (hit.newMove)
			ImGui::Text("%2d. %s", ++moveNumber, Describe(hit.move, m_moves).c_str());
		else
			ImGui::TextDisabled("     + hit in %s", hit.state.c_str());
	}

	if (moveNumber > 0) // at least one move credited, so there is a step to copy
	{
		if (ImGui::Button("Copy combo"))
			ImGui::SetClipboardText(ComboRecording::RecordedComboText(hits, m_moves).c_str());
		ImGui::ShowHelpMarkerSameLine("Copies the combo you just did as a \"combo: Recorded\" entry. Paste it into the combo file and rename it. Normals and throws are written as notation, specials as the moves file names them, or as their state name if it does not.");
	}

	// Only once the selected combo was completed: then the tracker itself paired each step
	// with the move that ticked it off. After any other combo, pairing by position would be
	// guesswork (a throw "= 5A").
	const std::string movesLines = ComboRecording::MovesFileLines(combo, m_resolved, hits, m_tracker.RunState());
	if (!movesLines.empty())
	{
		if (ImGui::Button("Copy moves file lines"))
			ImGui::SetClipboardText(movesLines.c_str());
		ImGui::ShowHelpMarkerSameLine("This combo was completed with steps that are not mapped yet. Copies a \"notation = StateName\" line for each, naming the move that ticked it off. Paste them into the moves file.");
	}
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

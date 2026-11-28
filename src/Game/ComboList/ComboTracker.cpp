#include "ComboTracker.h"

#include <cctype>
#include <set>

namespace
{
	bool EqualsIgnoreCase(const std::string& a, const std::string& b)
	{
		if (a.size() != b.size())
			return false;
		for (size_t i = 0; i < a.size(); ++i)
		{
			if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
				return false;
		}
		return true;
	}

	std::string Lower(std::string s)
	{
		for (char& c : s)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return s;
	}

	bool IsAutoPass(const ResolvedStep& step)
	{
		return step.match == StepMatch::AutoPass;
	}

	// Moves `next` past AutoPass steps, passing each one.
	void PassAutoSteps(const std::vector<ResolvedStep>& steps, std::vector<ComboStepState>& states, size_t& next)
	{
		while (next < steps.size() && IsAutoPass(steps[next]))
			states[next++] = ComboStepState::Landed;
	}
}

bool ComboTracker::Matches(const ResolvedStep& step, const std::string& move, const MoveTable& table)
{
	switch (step.match)
	{
	case StepMatch::Exact:
		for (const std::string& key : step.keys)
		{
			if (EqualsIgnoreCase(key, move))
				return true;
		}
		return false;
	case StepMatch::AnySpecial:
		return table.IsSpecial(move);
	case StepMatch::AnyMove:
		return true;
	case StepMatch::AutoPass:
		return false;
	}
	return false;
}

ComboStepState ComboTracker::StepState(size_t index) const
{
	return index < m_steps.size() ? m_steps[index] : ComboStepState::Pending;
}

void ComboTracker::Reset(size_t stepCount)
{
	m_steps.assign(stepCount, ComboStepState::Pending);
	m_runState = ComboRunState::Idle;
	m_nextStep = 0;
	m_offRouteMove.clear();
	m_offRouteHits = 0;
	m_moveStep = static_cast<size_t>(-1);
	m_hits.clear();
	// Keep the last sample and the current move: forgetting them would make an in-flight
	// combo look like a new one.
}

void ComboTracker::StartRun(const std::vector<ResolvedStep>& steps)
{
	m_steps.assign(steps.size(), ComboStepState::Pending);
	m_runState = ComboRunState::InProgress;
	m_nextStep = 0;
	// Leading movement (a dash or jump before the first hit) passes as the combo starts.
	PassAutoSteps(steps, m_steps, m_nextStep);
	m_offRouteMove.clear();
	m_offRouteHits = 0;
	m_moveStep = static_cast<size_t>(-1);
	m_hits.clear();
}

void ComboTracker::EndRun(const std::vector<ResolvedStep>& steps)
{
	if (m_runState == ComboRunState::InProgress)
	{
		// NextStep already passes movement steps, so reaching the end means every hit landed.
		size_t next = m_nextStep;
		while (next < steps.size() && IsAutoPass(steps[next]))
			++next;
		m_runState = next >= steps.size() ? ComboRunState::Completed : ComboRunState::Dropped;
	}
}

void ComboTracker::CreditMove(const std::string& move, const std::vector<ResolvedStep>& steps, const MoveTable& table)
{
	if (m_runState != ComboRunState::InProgress)
		return; // already completed or off route: only keep recording

	size_t candidate = m_nextStep;
	while (candidate < steps.size() && IsAutoPass(steps[candidate]))
		++candidate;

	if (candidate >= steps.size())
	{
		// More hits than the route has. The route itself was done.
		m_runState = ComboRunState::Completed;
		return;
	}

	if (!Matches(steps[candidate], move, table))
	{
		m_steps[candidate] = ComboStepState::Missed;
		m_offRouteMove = move;
		m_runState = ComboRunState::OffRoute;
		return;
	}

	// Movement steps are never checked: the ones after a landed step pass straight away.
	m_steps[candidate] = ComboStepState::Landed;
	m_moveStep = candidate;
	m_nextStep = candidate + 1;
	PassAutoSteps(steps, m_steps, m_nextStep);
	if (m_nextStep >= steps.size())
		m_runState = ComboRunState::Completed;
}

void ComboTracker::OnFrame(const ComboTrackerFrame& frame, const std::vector<ResolvedStep>& steps, const MoveTable& table)
{
	if (m_steps.size() != steps.size())
		Reset(steps.size());

	if (!m_haveSample || frame.frameCount < m_lastFrame)
	{
		// First sample, or a training reset (position reset, round restart): start clean.
		if (m_haveSample)
			Reset(steps.size());
		m_haveSample = true;
		m_lastFrame = frame.frameCount;
		m_lastHitCount = frame.defenderHitCount;
		m_lastStateChanges = frame.attackerStateChanges;
		m_lastState = frame.attackerAction;
		m_move.clear();
		m_moveCredited = true;
		return;
	}

	if (frame.frameCount == m_lastFrame)
		return; // paused, or drawn twice in one game frame
	m_lastFrame = frame.frameCount;

	if (frame.attackerStateChanges != m_lastStateChanges || frame.attackerAction != m_lastState)
	{
		if (table.IsMoveStart(frame.attackerAction, m_lastState, steps))
		{
			m_move = frame.attackerAction;
			m_moveCredited = false;
			m_moveStep = static_cast<size_t>(-1);
			m_moveHits = 0;
		}
		m_lastStateChanges = frame.attackerStateChanges;
		m_lastState = frame.attackerAction;
	}

	const int hits = frame.defenderHitCount;
	if (hits < m_lastHitCount)
	{
		EndRun(steps);
		// The counter can go straight from N to 1 when a new combo starts the same frame.
		m_lastHitCount = 0;
	}

	if (hits > m_lastHitCount)
	{
		if (m_lastHitCount == 0)
			StartRun(steps);

		const bool newMove = !m_moveCredited && !m_move.empty();
		m_hits.push_back({ m_move.empty() ? frame.attackerAction : m_move, frame.attackerAction, newMove });
		if (newMove)
		{
			m_moveCredited = true;
			m_moveHits = 1;
			CreditMove(m_move, steps, table);
		}
		else
		{
			++m_moveHits;
		}

		// "3C(1)": the step's move landing more hits than it allows breaks the route, even
		// if the combo was already complete (the extra hit is part of it).
		if (m_moveStep < steps.size() && steps[m_moveStep].hitLimit > 0 && m_moveHits > steps[m_moveStep].hitLimit
			&& (m_runState == ComboRunState::InProgress || m_runState == ComboRunState::Completed))
		{
			m_steps[m_moveStep] = ComboStepState::Missed;
			m_offRouteMove = m_move;
			m_offRouteHits = m_moveHits;
			m_runState = ComboRunState::OffRoute;
			m_moveStep = static_cast<size_t>(-1);
		}
	}

	m_lastHitCount = hits;
}

std::vector<std::string> ComboRecording::RecordedMoves(const std::vector<ComboHit>& hits)
{
	// Every state credited as a move will be named by a step of the copy, so entering any of
	// them starts a move when the copy is tracked.
	std::set<std::string> named;
	for (const ComboHit& hit : hits)
	{
		if (hit.newMove)
			named.insert(Lower(hit.move));
	}

	std::vector<std::string> moves;
	for (size_t i = 0; i < hits.size(); ++i)
	{
		const ComboHit& hit = hits[i];
		const bool inNamedState = named.count(Lower(hit.state)) > 0;
		if (hit.newMove)
		{
			// The move started, then the attacker moved on into a named state before the hit
			// landed: tracking the copy, that state is the move the hit goes to.
			const bool movedOn = !EqualsIgnoreCase(hit.state, hit.move) && inNamedState;
			moves.push_back(movedOn ? hit.state : hit.move);
		}
		else if (i > 0 && inNamedState && !EqualsIgnoreCase(hit.state, hits[i - 1].state))
		{
			// A later hit after moving into a named state: a move of its own for the copy. The
			// same state as the hit before is a multi-hit move, not a new one. A first hit
			// that credited nothing stays that way: no move was started for it.
			moves.push_back(hit.state);
		}
	}
	return moves;
}

std::string ComboRecording::RecordedComboText(const std::vector<ComboHit>& hits, const MoveTable& table)
{
	std::string steps;
	for (const std::string& state : RecordedMoves(hits))
	{
		std::string step = table.NotationFor(state);
		if (!table.ResolvesExactlyTo(step, state))
			step = state + " | " + state;
		steps += (steps.empty() ? "" : " > ") + step;
	}
	if (steps.empty())
		return std::string();
	return "combo: Recorded\n" + steps + "\n";
}

std::string ComboRecording::MovesFileLines(const Combo& combo, const std::vector<ResolvedStep>& resolved,
	const std::vector<ComboHit>& hits, ComboRunState runState)
{
	if (runState != ComboRunState::Completed || combo.steps.size() != resolved.size())
		return std::string();

	// The tracker credited moves to the steps in this same order (CreditMove), skipping
	// movement, until the last step landed; any credits after that are extra hits.
	std::vector<std::string> moves;
	for (const ComboHit& hit : hits)
	{
		if (hit.newMove)
			moves.push_back(hit.move);
	}

	std::string out;
	std::set<std::string> seen;
	size_t move = 0;
	for (size_t i = 0; i < resolved.size() && move < moves.size(); ++i)
	{
		if (IsAutoPass(resolved[i]))
			continue;
		const std::string& landed = moves[move++];
		if (resolved[i].match == StepMatch::Exact)
			continue; // already checked exactly; nothing to map
		const std::string line = combo.steps[i].notation + " = " + landed + "\n";
		if (seen.insert(line).second)
			out += line;
	}
	return out;
}

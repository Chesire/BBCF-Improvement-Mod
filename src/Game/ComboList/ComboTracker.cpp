#include "ComboTracker.h"

#include <cctype>

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
			CreditMove(m_move, steps, table);
		}
	}

	m_lastHitCount = hits;
}

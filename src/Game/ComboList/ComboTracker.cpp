#include "ComboTracker.h"

#include <cctype>

namespace
{
	bool EqualsIgnoreCase(const std::string& a, size_t aBegin, size_t aEnd, const std::string& b)
	{
		if (aEnd - aBegin != b.size())
			return false;
		for (size_t i = 0; i < b.size(); ++i)
		{
			if (std::tolower(static_cast<unsigned char>(a[aBegin + i])) != std::tolower(static_cast<unsigned char>(b[i])))
				return false;
		}
		return true;
	}
}

bool ComboTracker::KeyMatches(const std::string& key, const std::string& action)
{
	if (key.empty())
		return true;

	size_t start = 0;
	while (start <= key.size())
	{
		size_t slash = key.find('/', start);
		if (slash == std::string::npos)
			slash = key.size();

		size_t b = start;
		size_t e = slash;
		while (b < e && key[b] == ' ')
			++b;
		while (e > b && key[e - 1] == ' ')
			--e;
		if (e > b && EqualsIgnoreCase(key, b, e, action))
			return true;

		start = slash + 1;
	}
	return false;
}

bool ComboTracker::IsNonHitStep(const ComboStep& step)
{
	return step.matchKey == "-";
}

ComboStepState ComboTracker::StepState(size_t index) const
{
	return index < m_steps.size() ? m_steps[index] : ComboStepState::Pending;
}

void ComboTracker::Reset(const Combo* combo)
{
	m_steps.assign(combo ? combo->steps.size() : 0, ComboStepState::Pending);
	m_runState = ComboRunState::Idle;
	m_nextStep = 0;
	m_offRouteAction.clear();
	m_landedMoves.clear();
	// Keep the last sample: forgetting it would make an in-flight combo look like a new one.
}

void ComboTracker::StartRun(const Combo& combo)
{
	m_steps.assign(combo.steps.size(), ComboStepState::Pending);
	m_runState = ComboRunState::InProgress;
	m_nextStep = 0;
	m_offRouteAction.clear();
	m_landedMoves.clear();
}

void ComboTracker::EndRun(const Combo& combo)
{
	if (m_runState == ComboRunState::InProgress)
	{
		// Trailing non-hit steps (e.g. a final dash) cannot be seen landing; do not hold them
		// against the player if everything that hits did.
		size_t next = m_nextStep;
		while (next < combo.steps.size() && IsNonHitStep(combo.steps[next]))
			++next;
		m_runState = next >= combo.steps.size() ? ComboRunState::Completed : ComboRunState::Dropped;
	}
}

void ComboTracker::CreditHit(const std::string& action, const Combo& combo)
{
	m_landedMoves.push_back(action);

	if (m_runState != ComboRunState::InProgress)
		return; // already completed or off route: only keep recording

	size_t candidate = m_nextStep;
	while (candidate < combo.steps.size() && IsNonHitStep(combo.steps[candidate]))
		++candidate;

	if (candidate >= combo.steps.size())
	{
		// More hits than the route has. The route itself was done.
		m_runState = ComboRunState::Completed;
		return;
	}

	if (!KeyMatches(combo.steps[candidate].matchKey, action))
	{
		m_steps[candidate] = ComboStepState::Missed;
		m_offRouteAction = action;
		m_runState = ComboRunState::OffRoute;
		return;
	}

	for (size_t i = m_nextStep; i <= candidate; ++i)
		m_steps[i] = ComboStepState::Landed;
	m_nextStep = candidate + 1;

	size_t remaining = m_nextStep;
	while (remaining < combo.steps.size() && IsNonHitStep(combo.steps[remaining]))
		++remaining;
	if (remaining >= combo.steps.size())
	{
		for (size_t i = m_nextStep; i < combo.steps.size(); ++i)
			m_steps[i] = ComboStepState::Landed;
		m_nextStep = combo.steps.size();
		m_runState = ComboRunState::Completed;
	}
}

void ComboTracker::OnFrame(const ComboTrackerFrame& frame, const Combo& combo)
{
	if (m_steps.size() != combo.steps.size())
		Reset(&combo);

	if (!m_haveSample)
	{
		m_haveSample = true;
		m_lastFrame = frame.frameCount;
		m_lastHitCount = frame.defenderHitCount;
		m_lastStateChanges = frame.attackerStateChanges;
		m_lastAction = frame.attackerAction;
		return;
	}

	if (frame.frameCount == m_lastFrame)
		return; // paused, or drawn twice in one game frame

	if (frame.frameCount < m_lastFrame)
	{
		// Training reset (position reset, round restart). Start clean.
		Reset(&combo);
		m_lastFrame = frame.frameCount;
		m_lastHitCount = frame.defenderHitCount;
		m_lastStateChanges = frame.attackerStateChanges;
		m_lastAction = frame.attackerAction;
		return;
	}
	m_lastFrame = frame.frameCount;

	if (frame.attackerStateChanges != m_lastStateChanges || frame.attackerAction != m_lastAction)
	{
		++m_instance;
		m_lastStateChanges = frame.attackerStateChanges;
		m_lastAction = frame.attackerAction;
	}

	const int hits = frame.defenderHitCount;
	if (hits < m_lastHitCount)
	{
		EndRun(combo);
		// The counter can go straight from N to 1 when a new combo starts the same frame.
		m_lastHitCount = 0;
	}

	if (hits > m_lastHitCount)
	{
		if (m_lastHitCount == 0)
			StartRun(combo);

		if (m_instance != m_creditedInstance)
		{
			m_creditedInstance = m_instance;
			CreditHit(frame.attackerAction, combo);
		}
	}

	m_lastHitCount = hits;
}

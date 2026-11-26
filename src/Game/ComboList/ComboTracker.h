#pragma once
// Plain C++ (no Win32, no ImGui, no game structs) so the matching rules can be tested on
// their own. The window samples the game once per game frame and feeds ComboTrackerFrame in.
//
// How a hit is credited:
// - A combo starts when the defender's hit counter goes from 0 to above 0, and ends when it
//   drops (back to 0, or straight to 1 for a new combo).
// - When the hit counter rises, the attacker's current move landed. Each move *instance*
//   (a new state, even of the same move) is credited at most once, so a multi-hit move
//   counts as one step.
// - The landed move is compared with the next step's match key:
//     * empty key      -> any move counts (the step only checks that *something* landed)
//     * "-"            -> a step that never hits (dash, jump cancel); it is ticked off
//                         automatically when a later step lands
//     * "A/B"          -> either name counts
//   Comparison is case-insensitive.
// - A landed move that does not match ends the attempt as "off route" until the next combo.

#include "ComboParser.h"

#include <string>
#include <vector>

struct ComboTrackerFrame
{
	std::string attackerAction;  // the attacker's current action/state name
	int attackerStateChanges = 0; // increments whenever the attacker enters a new state
	int defenderHitCount = 0;     // the defender's combo hit counter
	unsigned frameCount = 0;      // the game's frame counter; going backwards means a reset
};

enum class ComboStepState
{
	Pending,
	Landed,
	Missed, // the step where an off-route move landed instead
};

enum class ComboRunState
{
	Idle,       // waiting for the first hit
	InProgress,
	Completed,
	Dropped,    // the combo ended before the last step
	OffRoute,   // a move landed that was not the next step
};

class ComboTracker
{
public:
	// Forget the current attempt. Call when the selected combo or the file changes.
	void Reset(const Combo* combo);

	// Feed one sample per game frame. Samples with the same frameCount are ignored.
	void OnFrame(const ComboTrackerFrame& frame, const Combo& combo);

	ComboRunState RunState() const { return m_runState; }
	ComboStepState StepState(size_t index) const;
	size_t NextStep() const { return m_nextStep; }
	int Hits() const { return m_lastHitCount; }

	// For OffRoute: what landed instead.
	const std::string& OffRouteAction() const { return m_offRouteAction; }

	// Every move instance that landed during the current (or last) combo, in order. This is
	// how a player finds the match keys for their own combo: do it once, copy the list.
	const std::vector<std::string>& LandedMoves() const { return m_landedMoves; }

	static bool KeyMatches(const std::string& key, const std::string& action);
	static bool IsNonHitStep(const ComboStep& step);

private:
	void StartRun(const Combo& combo);
	void EndRun(const Combo& combo);
	void CreditHit(const std::string& action, const Combo& combo);

	std::vector<ComboStepState> m_steps;
	ComboRunState m_runState = ComboRunState::Idle;
	size_t m_nextStep = 0;
	std::string m_offRouteAction;
	std::vector<std::string> m_landedMoves;

	bool m_haveSample = false;
	unsigned m_lastFrame = 0;
	int m_lastHitCount = 0;
	int m_lastStateChanges = 0;
	std::string m_lastAction;
	int m_instance = 0;         // bumps on each new attacker state
	int m_creditedInstance = -1; // the instance whose hit was last credited
};

#pragma once
// Plain C++ (no Win32, no ImGui, no game structs) so the matching rules can be tested on
// their own. The window samples the game once per game frame and feeds ComboTrackerFrame in.
//
// How a hit is credited:
// - A combo starts when the defender's hit counter goes from 0 to above 0, and ends when it
//   drops (back to 0, or straight to 1 for a new combo).
// - The attacker's states are followed as they change. MoveTable::IsMoveStart decides which
//   states begin a move (NmlAtk2A, NmlAtkThrow, a special out of neutral) and which only
//   continue one (ThrowExe, NmlAtk5D_2nd, a special's later states).
// - When the hit counter rises, the move that was last started is the one that landed. Each
//   move is credited once, so a multi-hit move is one step, and doing a move again is a new
//   move. Hits with no uncredited move (a move's later hits, a projectile after recovery)
//   are recorded but tick nothing.
// - The landed move is compared with the next step (MoveTable::Resolve):
//     Exact      -> one of the step's state names
//     AnySpecial -> any special (never a normal, throw or common state)
//     AnyMove    -> anything
//     AutoPass   -> never checked; passes as soon as the step before it lands (or when
//                   the combo starts, if it comes first). Never the next step, not counted.
// - A landed move that does not match ends the attempt as "off route" until the next combo.

#include "MoveTable.h"

#include <string>
#include <vector>

struct ComboTrackerFrame
{
	std::string attackerAction;   // the attacker's current state name
	int attackerStateChanges = 0; // increments whenever the attacker enters a new state
	int defenderHitCount = 0;     // the defender's combo hit counter
	unsigned frameCount = 0;      // the game's frame counter; going backwards means a reset
};

enum class ComboStepState
{
	Pending,
	Landed,     // for an AutoPass step: passed
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

struct ComboHit
{
	std::string move;  // the move credited (or, for a follow-up hit, the move it belongs to)
	std::string state; // the state the attacker was in when the hit landed
	bool newMove;      // false for a move's later hits
};

class ComboTracker
{
public:
	// Forget the current attempt. Call when the selected combo or the file changes.
	void Reset(size_t stepCount);

	// Feed one sample per game frame. Samples with the same frameCount are ignored.
	void OnFrame(const ComboTrackerFrame& frame, const std::vector<ResolvedStep>& steps, const MoveTable& table);

	ComboRunState RunState() const { return m_runState; }
	ComboStepState StepState(size_t index) const;
	size_t NextStep() const { return m_nextStep; }

	// For OffRoute: the move that landed instead.
	const std::string& OffRouteMove() const { return m_offRouteMove; }

	// Every hit of the current (or last) combo, in order.
	const std::vector<ComboHit>& Hits() const { return m_hits; }

	// The move the attacker is currently in, as the tracker sees it.
	const std::string& CurrentMove() const { return m_move; }

	static bool Matches(const ResolvedStep& step, const std::string& move, const MoveTable& table);

private:
	void StartRun(const std::vector<ResolvedStep>& steps);
	void EndRun(const std::vector<ResolvedStep>& steps);
	void CreditMove(const std::string& move, const std::vector<ResolvedStep>& steps, const MoveTable& table);

	std::vector<ComboStepState> m_steps;
	ComboRunState m_runState = ComboRunState::Idle;
	size_t m_nextStep = 0;
	std::string m_offRouteMove;
	std::vector<ComboHit> m_hits;

	bool m_haveSample = false;
	unsigned m_lastFrame = 0;
	int m_lastHitCount = 0;
	int m_lastStateChanges = 0;
	std::string m_lastState;

	std::string m_move;          // the last move started
	bool m_moveCredited = true;  // it has already been credited with a hit
};

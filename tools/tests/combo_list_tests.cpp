// Tests for the combo list parser and hit tracker. No Windows or game needed:
//   g++ -std=c++14 -I src tools/tests/combo_list_tests.cpp src/Game/ComboList/ComboParser.cpp src/Game/ComboList/ComboTracker.cpp src/Game/ComboList/MoveTable.cpp src/Game/Scr/ScrStateNames.cpp -o combo_list_tests
//   ./combo_list_tests
// (MSVC: cl /EHsc /I src tools\tests\combo_list_tests.cpp src\Game\ComboList\ComboParser.cpp src\Game\ComboList\ComboTracker.cpp src\Game\ComboList\MoveTable.cpp src\Game\Scr\ScrStateNames.cpp)
#include "Game/ComboList/ComboParser.h"
#include "Game/ComboList/ComboTracker.h"
#include "Game/ComboList/MoveTable.h"

#include <cstdio>
#include <cstdlib>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)

static std::vector<Combo> Parse(const std::string& text, std::vector<ComboFileIssue>* issuesOut = nullptr)
{
	std::vector<Combo> combos;
	std::vector<ComboFileIssue> issues;
	ComboParser::Parse(text, combos, issues);
	if (issuesOut)
		*issuesOut = issues;
	return combos;
}

static void ParserTests()
{
	std::vector<ComboFileIssue> issues;
	std::vector<Combo> c = Parse("# comment\ncombo: BnB\nnote: midscreen\n5B > 5C > 2C\n6A > 236D\n", &issues);
	CHECK(c.size() == 1 && issues.empty());
	CHECK(c[0].name == "BnB" && c[0].notes.size() == 1 && c[0].steps.size() == 5);
	CHECK(c[0].steps[4].notation == "236D");

	c = Parse("\xEF\xBB\xBF" "COMBO: A\r\n5A->5B -> 5C\r\n\r\nCombo:\r\nj.C >> 5C\r\n", &issues);
	CHECK(c.size() == 2 && issues.empty());
	CHECK(c[0].steps.size() == 3 && c[0].steps[1].notation == "5B");
	CHECK(c[1].name == "Combo 2" && c[1].steps.size() == 2);

	c = Parse("combo: k\n5B | NmlAtk5B > | NmlAtk5C > jc|- > dash | -\n", &issues);
	CHECK(c[0].steps.size() == 4);
	CHECK(c[0].steps[0].matchKey == "NmlAtk5B");
	CHECK(c[0].steps[1].notation == "NmlAtk5C");
	CHECK(c[0].steps[2].notation == "jc" && c[0].steps[2].matchKey == "-");
	CHECK(c[0].steps[3].matchKey == "-");

	c = Parse("note: x\n5A > 5B\ncombo: empty\ncombo: real\n[4]6D > 236D[D]\n", &issues);
	CHECK(c.size() == 2 && issues.size() == 3);
	CHECK(c[0].name == "Untitled" && c[1].steps[0].notation == "[4]6D" && c[1].steps[1].notation == "236D[D]");

	CHECK(Parse("").empty());
	CHECK(Parse("combo: z\n5A").size() == 1);
}

// A made-up character whose state names follow the real script's patterns: NmlAtk* normals
// with AIR and _2nd forms, a throw that hands over to ThrowExe, specials with follow-up
// states, and a super the script lists as a cancel target.
static MoveTable TestTable(const std::string& movesFile = "")
{
	MoveTable t;
	t.SetStates({
		"CmnActStand", "CmnActCrouch", "CmnActJumpUpper", "CmnActJumpLanding", "CmnActFDash",
		"NmlAtk5A", "NmlAtk5B", "NmlAtk5C", "NmlAtk2A", "NmlAtk2B", "NmlAtk2C", "NmlAtk6A", "NmlAtk3C",
		"NmlAtkAIR5B", "NmlAtkAIR5C", "NmlAtkAIR2C", "NmlAtk5D", "NmlAtk5D_2nd",
		"NmlAtkThrow", "ThrowExe", "NmlAtkBackThrow", "NmlAtkAirThrow",
		"NmlAtkGuardCrush", "NmlAtkDeadAngle", "NmlAtkExcite",
		"Skill1", "Skill1Exe", "Skill2", "GauntletHades", "UltimateA",
	}, { "NmlAtk5B", "NmlAtk5C", "NmlAtk2C", "UltimateA" });
	if (!movesFile.empty())
		t.SetUserMappings(movesFile);
	return t;
}

static ResolvedStep R(const MoveTable& t, const std::string& notation, const std::string& key = "")
{
	return t.Resolve(notation, key);
}

static bool IsExact(const ResolvedStep& s, const std::string& state)
{
	return s.match == StepMatch::Exact && s.keys.size() == 1 && s.keys[0] == state && !s.notInMoveList;
}

static void MoveTableTests()
{
	const MoveTable t = TestTable("# comment\n236D = Skill1\n214D=Skill2/Skill1Exe\nbroken line\n");

	CHECK(IsExact(R(t, "2A"), "NmlAtk2A"));
	CHECK(IsExact(R(t, "5a"), "NmlAtk5A"));
	CHECK(IsExact(R(t, "6A"), "NmlAtk6A"));
	CHECK(IsExact(R(t, "j.5C"), "NmlAtkAIR5C"));
	CHECK(IsExact(R(t, "j5C"), "NmlAtkAIR5C"));
	CHECK(IsExact(R(t, "j.5c"), "NmlAtkAIR5C"));
	CHECK(IsExact(R(t, "sj.5C"), "NmlAtkAIR5C"));
	CHECK(IsExact(R(t, "dj.5C"), "NmlAtkAIR5C"));
	CHECK(IsExact(R(t, "j.2C"), "NmlAtkAIR2C"));
	CHECK(IsExact(R(t, "5D (2nd)"), "NmlAtk5D_2nd"));
	CHECK(IsExact(R(t, "B+C"), "NmlAtkThrow"));
	CHECK(IsExact(R(t, "6BC"), "NmlAtkThrow"));
	CHECK(IsExact(R(t, "Throw"), "NmlAtkThrow"));
	CHECK(IsExact(R(t, "4B+C"), "NmlAtkBackThrow"));
	CHECK(IsExact(R(t, "j.B+C"), "NmlAtkAirThrow"));
	CHECK(IsExact(R(t, "A+B"), "NmlAtkGuardCrush"));
	CHECK(IsExact(R(t, "6A+B"), "NmlAtkDeadAngle"));
	CHECK(IsExact(R(t, "Gauntlet Hades"), "GauntletHades"));

	// A state name as the step: tracked exactly, shown as the move.
	ResolvedStep raw = R(t, "NmlAtk2A");
	CHECK(IsExact(raw, "NmlAtk2A") && raw.display == "2A");
	CHECK(R(t, "nmlatkair5c").display == "j.5C");
	CHECK(R(t, "NmlAtkAIR2C").display == "j.2C");
	CHECK(MoveTable::DisplayName("NmlAtkAIR5D_2nd") == "j.5D (2nd)");
	CHECK(MoveTable::DisplayName("NmlAtk5C") == "5C");
	CHECK(MoveTable::DisplayName("NmlAtkAirThrow") == "Air Throw");
	CHECK(R(t, "B+C").display == "B+C"); // aliases keep what was written

	// The moves file.
	CHECK(IsExact(R(t, "236D"), "Skill1"));
	ResolvedStep two = R(t, "214D");
	CHECK(two.match == StepMatch::Exact && two.keys.size() == 2 && two.keys[1] == "Skill1Exe");

	// Unmapped specials only accept specials; unknown text accepts anything.
	CHECK(R(t, "623C").match == StepMatch::AnySpecial);
	CHECK(R(t, "j.214B").match == StepMatch::AnySpecial);
	CHECK(R(t, "[4]6D").match == StepMatch::AnySpecial);
	CHECK(R(t, "632146D").match == StepMatch::AnySpecial);
	CHECK(R(t, "mystery").match == StepMatch::AnyMove);

	// Movement auto-passes; jc / sjc are jump and super jump cancels.
	CHECK(R(t, "jc").match == StepMatch::AutoPass);
	CHECK(R(t, "sjc").match == StepMatch::AutoPass);
	CHECK(R(t, "JC").match == StepMatch::AutoPass);
	CHECK(R(t, "dl.").match == StepMatch::AutoPass);
	CHECK(R(t, "66").match == StepMatch::AutoPass);
	CHECK(R(t, "ABCD").match == StepMatch::AutoPass);
	CHECK(R(t, "anything", "-").match == StepMatch::AutoPass);

	// An attack always carries its direction number: without one it is not read as an attack.
	CHECK(R(t, "j.C").match == StepMatch::AnyMove);
	CHECK(R(t, "C").match == StepMatch::AnyMove);
	CHECK(R(t, "jC").match == StepMatch::AutoPass); // jc, whatever the case

	// A normal this character does not have is still exact (so nothing else ticks it) but
	// flagged.
	ResolvedStep missing = R(t, "6C");
	CHECK(missing.match == StepMatch::Exact && missing.keys[0] == "NmlAtk6C" && missing.notInMoveList);

	// Explicit keys may be notation or state names.
	CHECK(IsExact(R(t, "5B", "NmlAtk5B"), "NmlAtk5B"));
	ResolvedStep alt = R(t, "j.5C", "j.5C/j.5B");
	CHECK(alt.keys.size() == 2 && alt.keys[1] == "NmlAtkAIR5B");

	// Without the character's state list, normals still resolve (unchecked).
	MoveTable empty;
	CHECK(IsExact(R(empty, "2B"), "NmlAtk2B"));

	// Bad moves-file lines are reported.
	MoveTable bad;
	CHECK(bad.SetUserMappings("236D = Skill1\nnope\n= x\n").size() == 2);

	// What starts a move and what continues one.
	const std::vector<ResolvedStep> none;
	CHECK(t.IsMoveStart("NmlAtk5A", "CmnActStand", none));
	CHECK(!t.IsMoveStart("NmlAtk5D_2nd", "NmlAtk5D", none));
	CHECK(!t.IsMoveStart("ThrowExe", "NmlAtkThrow", none));
	CHECK(!t.IsMoveStart("CmnActJumpUpper", "NmlAtkAIR5C", none));
	CHECK(t.IsMoveStart("Skill2", "CmnActStand", none));
	CHECK(t.IsMoveStart("Skill2", "NmlAtk5C", none));
	CHECK(!TestTable().IsMoveStart("Skill2", "Skill1", none));
	CHECK(t.IsMoveStart("Skill2", "Skill1", none));       // mapped in the moves file
	CHECK(t.IsMoveStart("UltimateA", "Skill1", none));    // a cancel target
}

struct Sim
{
	MoveTable table;
	std::vector<ResolvedStep> steps;
	ComboTracker t;
	unsigned f = 100;
	int sc = 0;
	std::string act = "CmnActStand";
	int hits = 0;

	explicit Sim(const std::string& text, const std::string& movesFile = "") : table(TestTable(movesFile))
	{
		const Combo combo = Parse(text).at(0);
		for (const ComboStep& step : combo.steps)
			steps.push_back(table.Resolve(step.notation, step.matchKey));
		t.Reset(steps.size());
		Tick();
	}
	void Tick()
	{
		ComboTrackerFrame fr;
		fr.attackerAction = act;
		fr.attackerStateChanges = sc;
		fr.defenderHitCount = hits;
		fr.frameCount = f++;
		t.OnFrame(fr, steps, table);
	}
	void Move(const std::string& a) { act = a; ++sc; Tick(); Tick(); }
	void Hit() { ++hits; Tick(); Tick(); }
	void Drop() { hits = 0; Tick(); }
	ComboRunState State() const { return t.RunState(); }
};

static void TrackerTests()
{
	{ // the reported bug: 5A > 5B > 5C > 2C must not accept 5A > 2A > 5A > 2A
		Sim s("combo: a\n5A > 5B > 5C > 2C\n");
		s.Move("NmlAtk5A"); s.Hit();
		CHECK(s.State() == ComboRunState::InProgress && s.t.NextStep() == 1);
		s.Move("NmlAtk2A"); s.Hit();
		CHECK(s.State() == ComboRunState::OffRoute && s.t.StepState(1) == ComboStepState::Missed);
		CHECK(s.t.OffRouteMove() == "NmlAtk2A");
		s.Move("NmlAtk5A"); s.Hit(); s.Move("NmlAtk2A"); s.Hit();
		CHECK(s.State() == ComboRunState::OffRoute);
	}
	{ // ...and does accept the real thing
		Sim s("combo: a\n5A > 5B > 5C > 2C\n");
		s.Move("NmlAtk5A"); s.Hit(); s.Move("NmlAtk5B"); s.Hit();
		s.Move("NmlAtk5C"); s.Hit(); s.Move("NmlAtk2C"); s.Hit();
		CHECK(s.State() == ComboRunState::Completed && s.t.Hits().size() == 4);
	}
	{ // a throw is NmlAtkThrow handing over to ThrowExe: one move, and only B+C ticks it
		Sim s("combo: a\nB+C > 2C\n");
		s.Move("NmlAtkThrow"); s.Move("ThrowExe"); s.Hit(); s.Hit(); s.Hit();
		CHECK(s.State() == ComboRunState::InProgress && s.t.NextStep() == 1);
		CHECK(s.t.Hits().size() == 3 && s.t.Hits()[0].move == "NmlAtkThrow" && s.t.Hits()[0].state == "ThrowExe");
		CHECK(!s.t.Hits()[1].newMove);
		s.Move("CmnActStand"); s.Move("NmlAtk2C"); s.Hit();
		CHECK(s.State() == ComboRunState::Completed);
	}
	{ // ...and a throw does not count as 5A
		Sim s("combo: a\n5A > 5B\n");
		s.Move("NmlAtkThrow"); s.Move("ThrowExe"); s.Hit();
		CHECK(s.State() == ComboRunState::OffRoute && s.t.OffRouteMove() == "NmlAtkThrow");
	}
	{ // the same move twice is two moves; a multi-hit move is one
		Sim s("combo: a\n5A > 5A > 5B\n");
		s.Move("NmlAtk5A"); s.Hit(); s.Hit(); s.Move("NmlAtk5A"); s.Hit(); s.Move("NmlAtk5B"); s.Hit();
		CHECK(s.State() == ComboRunState::Completed);
	}
	{ // 5D's automatic second part does not count as the next step
		Sim s("combo: a\n5D > 2C\n");
		s.Move("NmlAtk5D"); s.Hit(); s.Move("NmlAtk5D_2nd"); s.Hit();
		CHECK(s.State() == ComboRunState::InProgress && s.t.NextStep() == 1);
		s.Move("NmlAtk2C"); s.Hit();
		CHECK(s.State() == ComboRunState::Completed);
	}
	{ // air normals with a jump cancel between them: jc passes as soon as j.5C lands
		Sim s("combo: a\nj.5C > jc > j.5C\n");
		s.Move("NmlAtkAIR5C"); s.Hit();
		CHECK(s.t.StepState(1) == ComboStepState::Landed && s.t.NextStep() == 2);
		s.Move("CmnActJumpUpper"); s.Move("NmlAtkAIR5C"); s.Hit();
		CHECK(s.State() == ComboRunState::Completed);
	}
	{ // j.5C is the air one: a ground 5C does not count
		Sim s("combo: a\n5B > sjc > j.5C\n");
		s.Move("NmlAtk5B"); s.Hit(); s.Move("NmlAtk5C"); s.Hit();
		CHECK(s.State() == ComboRunState::OffRoute && s.t.OffRouteMove() == "NmlAtk5C");
	}
	{ // movement is never checked and passes by itself, first, middle or last
		Sim s("combo: a\ndash > 5A > dl. > 5B > land\n");
		CHECK(s.t.StepState(0) == ComboStepState::Pending); // nothing passes before the combo
		s.Move("CmnActFDash"); s.Move("NmlAtk5A"); s.Hit();
		CHECK(s.t.StepState(0) == ComboStepState::Landed && s.t.StepState(1) == ComboStepState::Landed);
		CHECK(s.t.StepState(2) == ComboStepState::Landed && s.t.NextStep() == 3);
		s.Move("NmlAtk5B"); s.Hit();
		CHECK(s.State() == ComboRunState::Completed && s.t.StepState(4) == ComboStepState::Landed);
	}
	{ // an unmapped special accepts a special...
		Sim s("combo: a\n5C > 236D\n");
		s.Move("NmlAtk5C"); s.Hit(); s.Move("Skill1"); s.Hit();
		CHECK(s.State() == ComboRunState::Completed);
	}
	{ // ...but not a normal
		Sim s("combo: a\n5C > 236D\n");
		s.Move("NmlAtk5C"); s.Hit(); s.Move("NmlAtk2C"); s.Hit();
		CHECK(s.State() == ComboRunState::OffRoute);
	}
	{ // a mapped special accepts only its own state
		Sim s("combo: a\n5C > 236D\n", "236D = Skill1\n");
		s.Move("NmlAtk5C"); s.Hit(); s.Move("Skill2"); s.Hit();
		CHECK(s.State() == ComboRunState::OffRoute && s.t.OffRouteMove() == "Skill2");
	}
	{ // a special's follow-up states are part of it; a projectile hit after recovering still
	  // credits the special
		Sim s("combo: a\n236D > 2C\n", "236D = Skill1\n");
		s.Move("Skill1"); s.Move("Skill1Exe"); s.Move("CmnActStand"); s.Hit();
		CHECK(s.t.NextStep() == 1 && s.t.Hits()[0].move == "Skill1");
		s.Move("NmlAtk2C"); s.Hit();
		CHECK(s.State() == ComboRunState::Completed);
	}
	{ // special into super (a cancel target) is two moves
		Sim s("combo: a\n236D > 632146D\n");
		s.Move("Skill1"); s.Hit(); s.Move("UltimateA"); s.Hit();
		CHECK(s.State() == ComboRunState::Completed);
	}
	{ // dropped, then the next combo starts a fresh attempt
		Sim s("combo: a\n5B > 5C > 2C\n");
		s.Move("NmlAtk5B"); s.Hit(); s.Move("NmlAtk5C"); s.Hit(); s.Drop();
		CHECK(s.State() == ComboRunState::Dropped && s.t.NextStep() == 2);
		s.Move("NmlAtk5B"); s.Hit();
		CHECK(s.State() == ComboRunState::InProgress && s.t.NextStep() == 1 && s.t.Hits().size() == 1);
	}
	{ // hit counter jumps N -> 1 for a new combo; frame counter going back is a reset
		Sim s("combo: a\n5B > 5C\n");
		s.Move("NmlAtk5B"); s.Hit(); s.Move("NmlAtk5C"); s.Hit();
		CHECK(s.State() == ComboRunState::Completed);
		s.Move("NmlAtk5B"); s.hits = 1; s.Tick();
		CHECK(s.State() == ComboRunState::InProgress && s.t.NextStep() == 1);
		s.f = 5; s.Tick();
		CHECK(s.State() == ComboRunState::Idle);
	}
	{ // a repeated frame number (pause, frame step) is ignored
		Sim s("combo: a\n5B\n");
		ComboTrackerFrame fr;
		fr.attackerAction = "NmlAtk5B";
		fr.attackerStateChanges = s.sc + 1;
		fr.defenderHitCount = 1;
		fr.frameCount = s.f - 1;
		s.t.OnFrame(fr, s.steps, s.table);
		CHECK(s.State() == ComboRunState::Idle);
	}
}

int main()
{
	ParserTests();
	MoveTableTests();
	TrackerTests();
	if (g_failures == 0)
		std::printf("All combo list tests passed.\n");
	return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

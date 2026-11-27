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

	explicit Sim(const std::string& text, const std::string& movesFile = "") : Sim(TestTable(movesFile), text) {}
	Sim(const MoveTable& moveTable, const std::string& text) : table(moveTable)
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

// Resolve(NotationFor(state)) must come back Exact with that state, for every state.
static bool RoundTrips(const MoveTable& t, const std::string& state)
{
	const ResolvedStep s = R(t, t.NotationFor(state));
	if (s.match != StepMatch::Exact)
		return false;
	for (const std::string& key : s.keys)
	{
		if (key == state)
			return true;
	}
	return false;
}

static void NotationTests()
{
	const MoveTable plain = TestTable();
	CHECK(plain.NotationFor("NmlAtk5C") == "5C");
	CHECK(plain.NotationFor("NmlAtk3C") == "3C");
	CHECK(plain.NotationFor("NmlAtkAIR5C") == "j.5C");
	CHECK(plain.NotationFor("NmlAtkAIR2C") == "j.2C");
	CHECK(plain.NotationFor("NmlAtk5D_2nd") == "5D (2nd)");
	CHECK(plain.NotationFor("NmlAtkThrow") == "B+C");
	CHECK(plain.NotationFor("NmlAtkBackThrow") == "4B+C");
	CHECK(plain.NotationFor("NmlAtkAirThrow") == "j.B+C");
	CHECK(plain.NotationFor("NmlAtkGuardCrush") == "A+B");
	CHECK(plain.NotationFor("NmlAtkDeadAngle") == "6A+B");
	CHECK(plain.NotationFor("NmlAtkExcite") == "EA");
	CHECK(plain.NotationFor("nmlatk2a") == "2A"); // spelled as the script spells it
	// Specials and other states: never a guessed input, the state name itself.
	CHECK(plain.NotationFor("Skill1") == "Skill1");
	CHECK(plain.NotationFor("GauntletHades") == "GauntletHades");
	CHECK(plain.NotationFor("ThrowExe") == "ThrowExe");
	CHECK(plain.NotationFor("CmnActFDash") == "CmnActFDash");

	// The moves file's notation, as written; the first line naming a state wins.
	const MoveTable mapped = TestTable("236D = Skill1\n214D=Skill2/Skill1Exe\nj.236D = Skill1\n");
	CHECK(mapped.NotationFor("Skill1") == "236D");
	CHECK(mapped.NotationFor("Skill2") == "214D");
	CHECK(mapped.NotationFor("Skill1Exe") == "214D");
	CHECK(mapped.NotationFor("GauntletHades") == "GauntletHades");

	// A moves-file line that takes a normal's notation over: the normal falls back to its
	// state name rather than to text that now means Skill1.
	const MoveTable taken = TestTable("2C = Skill1\n");
	CHECK(taken.NotationFor("NmlAtk2C") == "NmlAtk2C");
	CHECK(taken.NotationFor("Skill1") == "2C");

	// Every state of every table round-trips.
	const MoveTable* tables[] = { &plain, &mapped, &taken };
	for (const MoveTable* t : tables)
	{
		for (const std::string& state : t->States())
		{
			const bool ok = RoundTrips(*t, state);
			if (!ok)
				std::printf("  no round trip: %s -> %s\n", state.c_str(), t->NotationFor(state).c_str());
			CHECK(ok);
		}
	}

	// Without a state list, normals and universal moves still round-trip; a special's name
	// cannot (nothing says it is a state), which RecordedComboText covers with a match key.
	MoveTable empty;
	CHECK(RoundTrips(empty, "NmlAtk5C") && RoundTrips(empty, "NmlAtkAIR5C") && RoundTrips(empty, "NmlAtkThrow"));
	CHECK(!empty.ResolvesExactlyTo(empty.NotationFor("Assault_A"), "Assault_A"));
}

// Parses copied text and checks it is one combo whose every step resolves Exact to the
// expected state, and nothing else.
static bool PastesAs(const std::string& text, const MoveTable& t, const std::vector<std::string>& states)
{
	std::vector<ComboFileIssue> issues;
	const std::vector<Combo> combos = Parse(text, &issues);
	if (combos.size() != 1 || !issues.empty() || combos[0].steps.size() != states.size())
		return false;
	for (size_t i = 0; i < states.size(); ++i)
	{
		const ComboStep& step = combos[0].steps[i];
		if (!IsExact(R(t, step.notation, step.matchKey), states[i]))
			return false;
	}
	return true;
}

// A Kokonoe-like character for the reported case. CmnActStand comes between the throw and
// 5D (StandDrive) as it does after a throw. Assault_A is not a cancel target, so out of
// StandDrive it is a continuation unless something names it.
static MoveTable ReportTable(const std::string& movesFile = "")
{
	MoveTable t;
	t.SetStates({
		"CmnActStand", "NmlAtk5A", "NmlAtk5B", "NmlAtk5C", "NmlAtk2C", "NmlAtk6A", "NmlAtk6B", "NmlAtk3C",
		"NmlAtkThrow", "ThrowExe", "StandDrive", "Assault_A",
	}, { "NmlAtk6B", "NmlAtk5C", "NmlAtk3C" });
	if (!movesFile.empty())
		t.SetUserMappings(movesFile);
	return t;
}

// The combo from the report: throw, 5D cancelled into 236A, then 6B > 5C > 3C > 236A loops.
static void PlayReportedCombo(Sim& s)
{
	s.Move("NmlAtkThrow"); s.Move("ThrowExe"); s.Hit(); s.Hit(); s.Hit();
	s.Move("CmnActStand"); s.Move("StandDrive"); s.Hit(); s.Move("Assault_A"); s.Hit();
	for (int loop = 0; loop < 3; ++loop)
	{
		s.Move("NmlAtk6B"); s.Hit(); s.Hit();
		s.Move("NmlAtk5C"); s.Hit();
		s.Move("NmlAtk3C"); s.Hit();
		if (loop < 2)
		{
			s.Move("Assault_A"); s.Hit(); s.Hit();
		}
	}
}

static void RecordingTests()
{
	{ // the report: the starter combo selected, a different combo done
		const MoveTable table = ReportTable();
		Sim s(table, "combo: starter\n5A > 5B > 5C > 2C > 6A\n");
		PlayReportedCombo(s);
		CHECK(s.State() == ComboRunState::OffRoute);

		// The hits are the ones the window listed: 13 credited moves, the throw's and 6B's
		// later hits, and the hit in Assault_A credited to StandDrive.
		std::string listed;
		for (const ComboHit& hit : s.t.Hits())
			listed += (hit.newMove ? " " + hit.move : " +" + hit.state);
		CHECK(listed == " NmlAtkThrow +ThrowExe +ThrowExe StandDrive +Assault_A"
			" NmlAtk6B +NmlAtk6B NmlAtk5C NmlAtk3C Assault_A +Assault_A"
			" NmlAtk6B +NmlAtk6B NmlAtk5C NmlAtk3C Assault_A +Assault_A"
			" NmlAtk6B +NmlAtk6B NmlAtk5C NmlAtk3C");

		// Off route, so no moves file lines: pairing "5A" with the throw was the bug.
		CHECK(ComboRecording::MovesFileLines(Parse("combo: starter\n5A > 5B > 5C > 2C > 6A\n")[0],
			s.steps, s.t.Hits(), s.State()).empty());

		// The copy: the whole combo, the hit in Assault_A after StandDrive as its own step.
		const std::string text = ComboRecording::RecordedComboText(s.t.Hits(), table);
		std::printf("  report, no moves file:\n%s", text.c_str());
		CHECK(text == "combo: Recorded\n"
			"B+C > StandDrive > Assault_A > 6B > 5C > 3C > Assault_A > 6B > 5C > 3C > Assault_A > 6B > 5C > 3C\n");
		CHECK(PastesAs(text, table, { "NmlAtkThrow", "StandDrive", "Assault_A", "NmlAtk6B", "NmlAtk5C", "NmlAtk3C",
			"Assault_A", "NmlAtk6B", "NmlAtk5C", "NmlAtk3C", "Assault_A", "NmlAtk6B", "NmlAtk5C", "NmlAtk3C" }));

		// Pasted in and done again, it tracks to the end.
		Sim again(table, text);
		PlayReportedCombo(again);
		CHECK(again.State() == ComboRunState::Completed);

		// Why the extra step: one step per credited move only would go off route on its own
		// combo, because naming Assault_A makes it a move of its own after StandDrive.
		Sim strict(table, "combo: x\nB+C > StandDrive > 6B > 5C > 3C > Assault_A > 6B > 5C > 3C > Assault_A > 6B > 5C > 3C\n");
		PlayReportedCombo(strict);
		CHECK(strict.State() == ComboRunState::OffRoute && strict.t.OffRouteMove() == "Assault_A");
	}
	{ // the same with 236A mapped in the moves file
		const MoveTable table = ReportTable("236A = Assault_A\n");
		Sim s(table, "combo: starter\n5A > 5B > 5C > 2C > 6A\n");
		PlayReportedCombo(s);
		const std::string text = ComboRecording::RecordedComboText(s.t.Hits(), table);
		std::printf("  report, 236A = Assault_A:\n%s", text.c_str());
		CHECK(text == "combo: Recorded\n"
			"B+C > StandDrive > 236A > 6B > 5C > 3C > 236A > 6B > 5C > 3C > 236A > 6B > 5C > 3C\n");
		Sim again(table, text);
		PlayReportedCombo(again);
		CHECK(again.State() == ComboRunState::Completed);
	}
	{ // no state list: a special is written with itself as the match key, still exact
		MoveTable empty;
		const std::vector<ComboHit> hits = { { "NmlAtk5C", "NmlAtk5C", true }, { "Assault_A", "Assault_A", true } };
		const std::string text = ComboRecording::RecordedComboText(hits, empty);
		CHECK(text == "combo: Recorded\n5C > Assault_A | Assault_A\n");
		CHECK(PastesAs(text, empty, { "NmlAtk5C", "Assault_A" }));
	}
	{ // a state named like a movement word would auto-pass; the match key keeps it exact
		MoveTable t;
		t.SetStates({ "CmnActStand", "NmlAtk5C", "Land" }, {});
		const std::vector<ComboHit> hits = { { "NmlAtk5C", "NmlAtk5C", true }, { "Land", "Land", true } };
		const std::string text = ComboRecording::RecordedComboText(hits, t);
		CHECK(text == "combo: Recorded\n5C > Land | Land\n");
		CHECK(PastesAs(text, t, { "NmlAtk5C", "Land" }));
	}
	{ // a move superseded before its first hit: the hit goes to the named state it moved into
		const std::vector<ComboHit> hits = {
			{ "Skill1", "Skill1", true },
			{ "NmlAtk5C", "Skill1", true }, // 5C, then Skill1 again before the hit
		};
		const std::vector<std::string> moves = ComboRecording::RecordedMoves(hits);
		CHECK(moves.size() == 2 && moves[0] == "Skill1" && moves[1] == "Skill1");
	}
	{ // nothing credited: nothing to copy
		const std::vector<ComboHit> hits = { { "CmnActStand", "CmnActStand", false } };
		CHECK(ComboRecording::RecordedComboText(hits, TestTable()).empty());
		CHECK(ComboRecording::RecordedComboText({}, TestTable()).empty());
	}
	{ // moves file lines: a completed combo pairs its unmapped steps with what ticked them
		const std::string text = "combo: a\n5C > 236D > jc > j.5C > 214D > 236D\n";
		Sim s(text);
		s.Move("NmlAtk5C"); s.Hit(); s.Move("Skill1"); s.Hit(); s.Move("CmnActJumpUpper");
		s.Move("NmlAtkAIR5C"); s.Hit(); s.Move("Skill2"); s.Hit(); s.Move("CmnActStand"); s.Move("Skill1"); s.Hit();
		CHECK(s.State() == ComboRunState::Completed);
		const Combo combo = Parse(text)[0];
		CHECK(ComboRecording::MovesFileLines(combo, s.steps, s.t.Hits(), s.State())
			== "236D = Skill1\n214D = Skill2\n");
	}
	{ // ...nothing when every step is already exact, or the attempt was not completed
		const std::string text = "combo: a\n5C > 236D\n";
		Sim done("combo: a\n5B > 5C\n");
		done.Move("NmlAtk5B"); done.Hit(); done.Move("NmlAtk5C"); done.Hit();
		CHECK(done.State() == ComboRunState::Completed);
		CHECK(ComboRecording::MovesFileLines(Parse("combo: a\n5B > 5C\n")[0], done.steps, done.t.Hits(), done.State()).empty());
		Sim dropped(text);
		dropped.Move("NmlAtk5C"); dropped.Hit(); dropped.Drop();
		CHECK(ComboRecording::MovesFileLines(Parse(text)[0], dropped.steps, dropped.t.Hits(), dropped.State()).empty());
	}
}

int main()
{
	ParserTests();
	MoveTableTests();
	TrackerTests();
	NotationTests();
	RecordingTests();
	if (g_failures == 0)
		std::printf("All combo list tests passed.\n");
	return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

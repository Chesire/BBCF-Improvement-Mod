// Tests for the combo list parser and hit tracker. No Windows or game needed:
//   g++ -std=c++14 -I src tools/tests/combo_list_tests.cpp src/Game/ComboList/ComboParser.cpp src/Game/ComboList/ComboTracker.cpp -o combo_list_tests
//   ./combo_list_tests
// (MSVC: cl /EHsc /I src tools\tests\combo_list_tests.cpp src\Game\ComboList\ComboParser.cpp src\Game\ComboList\ComboTracker.cpp)
#include "Game/ComboList/ComboParser.h"
#include "Game/ComboList/ComboTracker.h"

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

struct Sim
{
	ComboTracker t;
	Combo c;
	unsigned f = 100;
	int sc = 0;
	std::string act = "CmnActStand";
	int hits = 0;

	explicit Sim(const std::string& text) : c(Parse(text).at(0)) { t.Reset(&c); Tick(); }
	void Tick()
	{
		ComboTrackerFrame fr;
		fr.attackerAction = act;
		fr.attackerStateChanges = sc;
		fr.defenderHitCount = hits;
		fr.frameCount = f++;
		t.OnFrame(fr, c);
	}
	void Move(const std::string& a) { act = a; ++sc; Tick(); Tick(); }
	void Hit() { ++hits; Tick(); Tick(); }
	void Drop() { hits = 0; Tick(); }
};

static void TrackerTests()
{
	{ // keyed route; a multi-hit move counts once; A/B alternatives
		Sim s("combo: a\n5B|NmlAtk5B > 5C|NmlAtk5C > 236D|Skill1/Skill1Air\n");
		CHECK(s.t.RunState() == ComboRunState::Idle);
		s.Move("NmlAtk5B"); s.Hit();
		CHECK(s.t.RunState() == ComboRunState::InProgress && s.t.NextStep() == 1);
		s.Move("NmlAtk5C"); s.Hit(); s.Hit();
		CHECK(s.t.NextStep() == 2);
		s.Move("skill1air"); s.Hit();
		CHECK(s.t.RunState() == ComboRunState::Completed && s.t.LandedMoves().size() == 3);
		s.Drop();
		CHECK(s.t.RunState() == ComboRunState::Completed);
	}
	{ // wrong move lands
		Sim s("combo: a\n5B|NmlAtk5B > 5C|NmlAtk5C\n");
		s.Move("NmlAtk5B"); s.Hit(); s.Move("NmlAtk2C"); s.Hit();
		CHECK(s.t.RunState() == ComboRunState::OffRoute && s.t.StepState(1) == ComboStepState::Missed);
		CHECK(s.t.OffRouteAction() == "NmlAtk2C");
		s.Move("NmlAtk5C"); s.Hit();
		CHECK(s.t.RunState() == ComboRunState::OffRoute && s.t.LandedMoves().size() == 3);
	}
	{ // dropped, then the next combo starts a fresh attempt
		Sim s("combo: a\n5B > 5C > 2C\n");
		s.Move("A"); s.Hit(); s.Move("B"); s.Hit(); s.Drop();
		CHECK(s.t.RunState() == ComboRunState::Dropped && s.t.NextStep() == 2);
		s.Move("A"); s.Hit();
		CHECK(s.t.RunState() == ComboRunState::InProgress && s.t.NextStep() == 1 && s.t.LandedMoves().size() == 1);
	}
	{ // non-hit steps tick off with the next hit; a trailing one does not block completion
		Sim s("combo: a\nj.C|JmpAtkC > jc|- > j.C|JmpAtkC > dash|-\n");
		s.Move("JmpAtkC"); s.Hit(); s.Move("CmnActJumpUpper"); s.Move("JmpAtkC"); s.Hit();
		CHECK(s.t.RunState() == ComboRunState::Completed);
		CHECK(s.t.StepState(1) == ComboStepState::Landed && s.t.StepState(3) == ComboStepState::Landed);
	}
	{ // the same move done twice is two instances
		Sim s("combo: a\n5A|NmlAtk5A > 5A|NmlAtk5A\n");
		s.Move("NmlAtk5A"); s.Hit(); s.Move("NmlAtk5A"); s.Hit();
		CHECK(s.t.RunState() == ComboRunState::Completed);
	}
	{ // hit counter jumps N -> 1 for a new combo; frame counter going back is a reset
		Sim s("combo: a\n5B > 5C\n");
		s.Move("A"); s.Hit(); s.Move("B"); s.Hit();
		CHECK(s.t.RunState() == ComboRunState::Completed);
		s.Move("A"); s.hits = 1; s.Tick();
		CHECK(s.t.RunState() == ComboRunState::InProgress && s.t.NextStep() == 1);
		s.f = 5; s.Tick();
		CHECK(s.t.RunState() == ComboRunState::Idle);
	}
	{ // a repeated frame number (pause, frame step) is ignored
		Sim s("combo: a\n5B\n");
		ComboTrackerFrame fr;
		fr.attackerAction = "X";
		fr.attackerStateChanges = s.sc + 1;
		fr.defenderHitCount = 1;
		fr.frameCount = s.f - 1;
		s.t.OnFrame(fr, s.c);
		CHECK(s.t.RunState() == ComboRunState::Idle);
	}
	CHECK(ComboTracker::KeyMatches("a / B ", "b"));
	CHECK(!ComboTracker::KeyMatches("a/b", "c"));
	CHECK(ComboTracker::KeyMatches("", "anything"));
}

int main()
{
	ParserTests();
	TrackerTests();
	if (g_failures == 0)
		std::printf("All combo list tests passed.\n");
	return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

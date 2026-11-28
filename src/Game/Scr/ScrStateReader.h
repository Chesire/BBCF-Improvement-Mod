#pragma once

#include <vector>
#include <string>
#include <map>
#include "Game/Jonb/JonbDBReader.h"
#include "Game/Jonb/JonbDBEntry.h"
#include "ScrStateEntry.h"


std::vector<scrState*> parse_scr(char* bbcf_base_addr, int player_num);

// The moves the player's script registers with their inputs (registerMove blocks), for the
// combo list's notation. Found by scanning the script's code, since they live in a subroutine
// that parse_scr does not reach. Empty when the script is not loaded or does not look as
// expected (the FPAC header is checked before anything is read).
std::vector<scrMoveRegistration> parse_scr_move_registrations(char* bbcf_base_addr, int player_num);
int parse_state(char* addr, std::vector<scrState*>& states_parsed, std::map<std::string, JonbDBEntry>*, std::map<std::string, scrState*>* ea_state_map);
void override_state(char* addr, char* new_state);
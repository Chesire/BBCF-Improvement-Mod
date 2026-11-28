#pragma once
// Finds the moves a character's script registers (registerMove 14001 ... endRegisterMove 14002).
//
// They are in a subroutine (the character's init), and parse_scr only walks the functions in the
// script's state index, which lists states and not subroutines. So instead of walking functions,
// this scans the script's code for registerMove commands and walks each block from there.
//
// Plain C++ (no Win32, no game pointers) so it can be tested on its own: the caller hands over
// the code bytes and a function that gives each command's size.

#include "ScrStateEntry.h"

#include <cstddef>
#include <vector>

// Total size in bytes of a command, its 4-byte id included, or 0 when it is not known.
typedef unsigned int (*ScrCommandSizeFn)(unsigned int command);

// Every registration block in [code, code + size). Commands are 4-byte aligned.
// A block counts when registerMove has a plausible name (letters, digits, '_') and type, and the
// walk reaches endRegisterMove or stops on a command whose size is not known. In the second case
// the inputs read so far are kept; the move then gets no notation if its button was never read,
// rather than a wrong one.
std::vector<scrMoveRegistration> FindMoveRegistrations(const char* code, size_t size, ScrCommandSizeFn commandSize);

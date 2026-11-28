#include "ScrMoveRegistrations.h"

#include <cctype>
#include <cstring>

namespace
{
	const unsigned int kRegisterMove = 14001;    // (string[32] name, int type)
	const unsigned int kEndRegisterMove = 14002; // ()
	const unsigned int kMoveCondition = 14003;   // (int)
	const unsigned int kMoveInput = 14012;       // (int input)
	const unsigned int kMoveSubroutine = 14013;  // (string[32])

	// A registration block is a handful of commands; anything longer is not one.
	const int kMaxCommandsInBlock = 64;
	// Types are 2, 0x606, 0x607, 0x1xx..0x4xx for attacks and small numbers for movement.
	const unsigned int kMaxPlausibleType = 0x1000;

	bool ReadU32(const char* code, size_t size, size_t pos, unsigned int& out)
	{
		if (pos > size || size - pos < 4)
			return false;
		memcpy(&out, code + pos, 4);
		return true;
	}

	// A script name: 1..31 identifier characters, then a NUL, inside the 32-byte field.
	bool IsPlausibleName(const char* field)
	{
		size_t length = 0;
		while (length < 32 && field[length] != '\0')
		{
			const unsigned char c = static_cast<unsigned char>(field[length]);
			if (!std::isalnum(c) && c != '_')
				return false;
			++length;
		}
		return length > 0 && length < 32;
	}
}

std::vector<scrMoveRegistration> FindMoveRegistrations(const char* code, size_t size, ScrCommandSizeFn commandSize)
{
	std::vector<scrMoveRegistration> found;
	if (!code)
		return found;

	size_t pos = 0;
	while (pos + 40 <= size)
	{
		unsigned int command = 0;
		ReadU32(code, size, pos, command);
		if (command != kRegisterMove || !IsPlausibleName(code + pos + 4))
		{
			pos += 4;
			continue;
		}

		scrMoveRegistration reg;
		reg.name.assign(code + pos + 4, strnlen(code + pos + 4, 32));
		memcpy(&reg.type, code + pos + 36, 4);
		if (reg.type >= kMaxPlausibleType)
		{
			pos += 4;
			continue;
		}

		// Walk the block: inputs, conditions and a subroutine name until endRegisterMove.
		size_t at = pos + 40;
		bool keep = false;
		for (int step = 0; step < kMaxCommandsInBlock; ++step)
		{
			unsigned int next = 0;
			if (!ReadU32(code, size, at, next))
				break;
			if (next == kEndRegisterMove)
			{
				at += 4;
				keep = true;
				break;
			}
			if (next == kRegisterMove)
				break; // a new block without this one ending: not a block we understand
			if (next == kMoveInput || next == kMoveCondition)
			{
				unsigned int value = 0;
				if (!ReadU32(code, size, at + 4, value))
					break;
				(next == kMoveInput ? reg.inputs : reg.conditions).push_back(value);
				at += 8;
				continue;
			}
			if (next == kMoveSubroutine)
			{
				if (size - at < 36)
					break;
				reg.subroutine.assign(code + at + 4, strnlen(code + at + 4, 32));
				at += 36;
				continue;
			}
			const unsigned int length = commandSize ? commandSize(next) : 0;
			if (length < 4 || length % 4 != 0)
			{
				keep = true; // unknown command inside the block: keep what was read
				break;
			}
			at += length;
		}

		if (keep)
		{
			found.push_back(reg);
			pos = at > pos + 40 ? at : pos + 40;
		}
		else
		{
			pos += 4;
		}
	}
	return found;
}

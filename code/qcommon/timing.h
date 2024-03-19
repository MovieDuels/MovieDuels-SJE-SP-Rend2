/*
===========================================================================
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

#pragma once
#if defined(_WIN32) && defined(__GNUC__)
#include <x86intrin.h>  /* for __rdtsc() */
#endif
#include <cstdint>
#include <intrin.h>

class timing_c
{
	uint64_t start;
	uint64_t end;

	int reset;

public:
	timing_c() : start(0), end(0), reset(0)
	{
	}

	void Start()
	{
#ifdef _WIN32
		start = __rdtsc();
#else
		start = 0;
#endif
	}

	int End()
	{
		int time;

#ifdef _WIN32
		end = __rdtsc();
#else
		end = 0;
#endif

		time = end - start;
		if (time < 0)
		{
			time = 0;
		}
		return time;
	}
};

// end

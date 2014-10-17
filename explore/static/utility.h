#pragma once
#include <stdint.h>

#ifdef WIN32
typedef unsigned __int64 u64;
typedef unsigned __int32 u32;
typedef unsigned __int16 u16;
typedef unsigned __int8  u8;
typedef signed __int64 i64;
typedef signed __int32 i32;
typedef signed __int16 i16;
typedef signed __int8  i8;
#else
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;
#endif

typedef unsigned char    byte;

#include "prg.h"

class HashUtils
{
public:
	static u64 Compute_Id_Hash(const std::string& unique_id)
	{
		size_t ret = 0;
		const char *p = unique_id.c_str();
		while (*p != '\0')
		if (*p >= '0' && *p <= '9')
			ret = 10 * ret + *(p++) - '0';
		else
		{
			murmur_hash foo;
			return foo.uniform_hash(unique_id.c_str(), unique_id.size(), 0);
		}
		return ret;
	}
}
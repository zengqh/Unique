#include "Precompiled.h"
#include "StringUtils.h"


namespace Unique
{

	String FormatString(const char* formatString, ...)
	{
		String ret;
		va_list args;
		va_start(args, formatString);
		ret.AppendWithFormatArgs(formatString, args);
		va_end(args);
		return ret;
	}

	//-----------------------------------------------------------------------
	bool Match(const String& str, const String& pattern, bool caseSensitive)
	{
		String tmpStr;
		String tmpPattern;
		if (!caseSensitive)
		{
			tmpStr = str.ToLower();
			tmpPattern = pattern.ToLower();
		}
		else
		{
			tmpStr = str;
			tmpPattern = pattern;
		}

		String::const_iterator strIt = tmpStr.Begin();
		String::const_iterator patIt = tmpPattern.Begin();
		String::const_iterator lastWildCardIt = tmpPattern.End();
		while (strIt != tmpStr.End() && patIt != tmpPattern.End())
		{
			if (*patIt == '*')
			{
				lastWildCardIt = patIt;
				// Skip over looking for next character
				++patIt;
				if (patIt == tmpPattern.End())
				{
					// Skip right to the end since * matches the entire rest of the string
					strIt = tmpStr.End();
				}
				else
				{
					// scan until we find next pattern character
					while (strIt != tmpStr.End() && *strIt != *patIt)
						++strIt;
				}
			}
			else
			{
				if (*patIt != *strIt)
				{
					if (lastWildCardIt != tmpPattern.End())
					{
						// The last wildcard can match this incorrect sequence
						// rewind pattern to wildcard and keep searching
						patIt = lastWildCardIt;
						lastWildCardIt = tmpPattern.End();
					}
					else
					{
						// no wildwards left
						return false;
					}
				}
				else
				{
					++patIt;
					++strIt;
				}
			}

		}
		// If we reached the end of both the pattern and the string, we succeeded
		if ((patIt == tmpPattern.End() || (*patIt == '*' && patIt + 1 == tmpPattern.End())) && strIt == tmpStr.End())
		{
			return true;
		}
		else
		{
			return false;
		}

	}

	ByteArray ToBase64(ByteArray& v)
	{
		const char alphabet[] = "ABCDEFGH" "IJKLMNOP" "QRSTUVWX" "YZabcdef"
			"ghijklmn" "opqrstuv" "wxyz0123" "456789+/";
		const char padchar = '=';
		int padlen = 0;

		ByteArray tmp;
		tmp.resize((v.size() * 4) / 3 + 3);

		int i = 0;
		char *out = tmp.data();
		while (i < v.size()) {
			int chunk = 0;
			chunk |= int(byte(v[i++])) << 16;
			if (i == v.size()) {
				padlen = 2;
			}
			else {
				chunk |= int(byte(v[i++])) << 8;
				if (i == v.size()) padlen = 1;
				else chunk |= int(byte(v[i++]));
			}

			int j = (chunk & 0x00fc0000) >> 18;
			int k = (chunk & 0x0003f000) >> 12;
			int l = (chunk & 0x00000fc0) >> 6;
			int m = (chunk & 0x0000003f);
			*out++ = alphabet[j];
			*out++ = alphabet[k];
			if (padlen > 1) *out++ = padchar;
			else *out++ = alphabet[l];
			if (padlen > 0) *out++ = padchar;
			else *out++ = alphabet[m];
		}

		tmp.resize(out - tmp.data());
		return tmp;
	}

	ByteArray FromBase64(const char* base64, size_t size)
	{
		unsigned int buf = 0;
		int nbits = 0;
		ByteArray tmp;
		tmp.reserve((size * 3) / 4);

		int offset = 0;
		for (int i = 0; i < size; ++i) {
			int ch = base64[i];
			int d;

			if (ch >= 'A' && ch <= 'Z')
				d = ch - 'A';
			else if (ch >= 'a' && ch <= 'z')
				d = ch - 'a' + 26;
			else if (ch >= '0' && ch <= '9')
				d = ch - '0' + 52;
			else if (ch == '+')
				d = 62;
			else if (ch == '/')
				d = 63;
			else
				d = -1;

			if (d != -1) {
				buf = (buf << 6) | d;
				nbits += 6;
				if (nbits >= 8) {
					nbits -= 8;
					tmp.push_back(buf >> nbits);///*[offset++]*/ = buf >> nbits;
					buf &= (1 << nbits) - 1;
				}
			}
		}

		tmp.shrink_to_fit();
		return tmp;
	}

}
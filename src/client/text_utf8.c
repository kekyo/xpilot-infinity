/* UTF-8 validation shared by native and browser text providers. */
#include "native_text.h"

static XpTextStatus Utf8_prefix(const char *utf8, size_t byte_length,
				int allow_incomplete, size_t *prefix_length)
{
    size_t offset = 0;

    if ((utf8 == NULL && byte_length != 0) || prefix_length == NULL)
	return XP_TEXT_STATUS_INVALID_ARGUMENT;
    while (offset < byte_length) {
	const unsigned char lead = (unsigned char)utf8[offset];
	size_t sequence_length;
	uint32_t codepoint;
	size_t continuation;

	if (lead <= 0x7f) {
	    offset++;
	    continue;
	}
	if (lead >= 0xc2 && lead <= 0xdf) {
	    sequence_length = 2;
	    codepoint = lead & 0x1f;
	} else if (lead >= 0xe0 && lead <= 0xef) {
	    sequence_length = 3;
	    codepoint = lead & 0x0f;
	} else if (lead >= 0xf0 && lead <= 0xf4) {
	    sequence_length = 4;
	    codepoint = lead & 0x07;
	} else {
	    return XP_TEXT_STATUS_INVALID_UTF8;
	}
	if (sequence_length > byte_length - offset) {
	    if (allow_incomplete) {
		*prefix_length = offset;
		return XP_TEXT_STATUS_OK;
	    }
	    return XP_TEXT_STATUS_INVALID_UTF8;
	}
	for (continuation = 1; continuation < sequence_length;
	     continuation++) {
	    const unsigned char byte =
		(unsigned char)utf8[offset + continuation];

	    if ((byte & 0xc0) != 0x80)
		return XP_TEXT_STATUS_INVALID_UTF8;
	    codepoint = (codepoint << 6) | (byte & 0x3f);
	}
	if ((sequence_length == 3 && codepoint < 0x800)
	    || (sequence_length == 4 && codepoint < 0x10000)
	    || codepoint > 0x10ffff
	    || (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
	    return XP_TEXT_STATUS_INVALID_UTF8;
	}
	offset += sequence_length;
    }
    *prefix_length = offset;
    return XP_TEXT_STATUS_OK;
}

XpTextStatus Xp_text_complete_utf8_prefix(const char *utf8,
					  size_t byte_length,
					  size_t *prefix_length)
{
    return Utf8_prefix(utf8, byte_length, 1, prefix_length);
}


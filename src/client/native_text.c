/* Backend-neutral native text layout and rasterization. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "native_text.h"
#include "native_text_internal.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WINDOWS
#include <windows.h>
#else
#include <fontconfig/fontconfig.h>
#endif

#define XP_TEXT_MAX_FACES 24

typedef struct XpTextFace {
    TTF_Font *ttf;
    char *family;
    unsigned char *memory;
    size_t memory_size;
} XpTextFace;

typedef struct XpTextSource {
    char *family;
    char *path;
    long face_index;
    unsigned char *memory;
    size_t memory_size;
} XpTextSource;

typedef struct XpTextSourceList {
    XpTextSource items[XP_TEXT_MAX_FACES];
    size_t count;
} XpTextSourceList;

typedef struct XpTextFamilyList {
    char **items;
    size_t count;
} XpTextFamilyList;

struct XpTextSystem {
    TTF_TextEngine *surface_engine;
#ifdef _WINDOWS
    wchar_t **private_font_paths;
    size_t private_font_count;
#else
    FcConfig *font_config;
#endif
};

struct XpTextFont {
    XpTextSystem *system;
    XpTextFace faces[XP_TEXT_MAX_FACES];
    size_t face_count;
    int ascent;
    int line_spacing;
};

struct XpTextLayout {
    XpTextFont *font;
    TTF_Text *text;
    XpTextDirection direction;
    char *language;
};

static char *Text_duplicate_range(const char *text, size_t length)
{
    char *copy;

    if (length == SIZE_MAX)
	return NULL;
    copy = malloc(length + 1);
    if (copy == NULL)
	return NULL;
    if (length > 0)
	memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

static char *Text_duplicate(const char *text)
{
    if (text == NULL)
	return NULL;
    return Text_duplicate_range(text, strlen(text));
}

static void Family_list_destroy(XpTextFamilyList *list)
{
    size_t index;

    if (list == NULL)
	return;
    for (index = 0; index < list->count; index++)
	free(list->items[index]);
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static int Family_list_contains(const XpTextFamilyList *list,
				const char *family)
{
    size_t index;

    for (index = 0; index < list->count; index++) {
        if (strcmp(list->items[index], family) == 0)
	    return 1;
    }
    return 0;
}

static XpTextStatus Family_list_append(XpTextFamilyList *list,
				       const char *family, size_t length)
{
    char **items;
    char *copy;
    size_t begin = 0;

    while (begin < length && isspace((unsigned char)family[begin]))
	begin++;
    while (length > begin && isspace((unsigned char)family[length - 1]))
	length--;
    if (length == begin)
	return XP_TEXT_STATUS_OK;
    copy = Text_duplicate_range(family + begin, length - begin);
    if (copy == NULL)
	return XP_TEXT_STATUS_OUT_OF_MEMORY;
    if (Family_list_contains(list, copy)) {
	free(copy);
	return XP_TEXT_STATUS_OK;
    }
    if (list->count == SIZE_MAX / sizeof(*items)) {
	free(copy);
	return XP_TEXT_STATUS_OUT_OF_MEMORY;
    }
    items = realloc(list->items, (list->count + 1) * sizeof(*items));
    if (items == NULL) {
	free(copy);
	return XP_TEXT_STATUS_OUT_OF_MEMORY;
    }
    list->items = items;
    list->items[list->count++] = copy;
    return XP_TEXT_STATUS_OK;
}

static XpTextStatus Family_list_append_csv(XpTextFamilyList *list,
					   const char *families)
{
    const char *start;
    const char *cursor;
    XpTextStatus status;

    if (families == NULL)
	return XP_TEXT_STATUS_INVALID_ARGUMENT;
    start = families;
    cursor = families;
    for (;;) {
	if (*cursor == ',' || *cursor == '\0') {
	    status = Family_list_append(
		list, start, (size_t)(cursor - start));
	    if (status != XP_TEXT_STATUS_OK)
		return status;
	    if (*cursor == '\0')
		break;
	    start = cursor + 1;
	}
	cursor++;
    }
    return XP_TEXT_STATUS_OK;
}

static XpTextStatus Family_list_build(const XpTextFontRequest *request,
				      XpTextFamilyList *list)
{
    static const char *fallbacks[] = {
	"Noto Sans Mono CJK JP",
	"Noto Sans Mono"
    };
    XpTextStatus status;
    size_t index;

    status = Family_list_append_csv(list, request->family_list);
    if (status != XP_TEXT_STATUS_OK)
	return status;
    for (index = 0; index < sizeof(fallbacks) / sizeof(fallbacks[0]);
	 index++) {
	status = Family_list_append(
	    list, fallbacks[index], strlen(fallbacks[index]));
	if (status != XP_TEXT_STATUS_OK)
	    return status;
    }
    if (request->spacing == XP_TEXT_SPACING_MONOSPACE) {
	return Family_list_append(list, "monospace",
				  sizeof("monospace") - 1);
    }
    return Family_list_append(list, "sans-serif",
			      sizeof("sans-serif") - 1);
}

static void Source_destroy(XpTextSource *source)
{
    if (source == NULL)
	return;
    free(source->family);
    free(source->path);
    free(source->memory);
    memset(source, 0, sizeof(*source));
}

static void Source_list_destroy(XpTextSourceList *sources)
{
    size_t index;

    if (sources == NULL)
	return;
    for (index = 0; index < sources->count; index++)
	Source_destroy(&sources->items[index]);
    sources->count = 0;
}

static int Source_list_contains(const XpTextSourceList *sources,
				const char *path, long face_index,
				const char *family)
{
    size_t index;

    for (index = 0; index < sources->count; index++) {
	const XpTextSource *source = &sources->items[index];

	if (path != NULL && source->path != NULL
	    && source->face_index == face_index
	    && strcmp(source->path, path) == 0) {
	    return 1;
	}
	if (path == NULL && source->path == NULL && family != NULL
	    && source->family != NULL
	    && strcmp(source->family, family) == 0) {
	    return 1;
	}
    }
    return 0;
}

#ifndef _WINDOWS

static int Fontconfig_family_is_generic(const char *family)
{
    return strcmp(family, "monospace") == 0
	|| strcmp(family, "sans-serif") == 0;
}

static int Fontconfig_pattern_has_family(const FcPattern *pattern,
					 const char *family)
{
    FcChar8 *candidate = NULL;
    int index;

    for (index = 0;
	 FcPatternGetString(pattern, FC_FAMILY, index, &candidate)
	     == FcResultMatch;
	 index++) {
	if (candidate != NULL
	    && FcStrCmpIgnoreCase(candidate, (const FcChar8 *)family) == 0) {
	    return 1;
	}
    }
    return 0;
}

static XpTextStatus Resolve_sources(XpTextSystem *system,
				    const XpTextFontRequest *request,
				    const XpTextFamilyList *families,
				    XpTextSourceList *sources)
{
    FcPattern *pattern = NULL;
    FcFontSet *matches = NULL;
    FcResult result = FcResultNoMatch;
    XpTextStatus status = XP_TEXT_STATUS_FONT_UNAVAILABLE;
    size_t family_index;
    int match_index;

    /* Resolve named families independently so style scoring cannot displace
     * a later multilingual fallback beyond the bounded face chain. The final
     * generic family still contributes the wider system fallback set. */
    for (family_index = 0; family_index < families->count; family_index++) {
	const char *requested_family = families->items[family_index];
	int generic = Fontconfig_family_is_generic(requested_family);

	pattern = FcPatternCreate();
	if (pattern == NULL) {
	    status = XP_TEXT_STATUS_OUT_OF_MEMORY;
	    goto cleanup;
	}
	if (!FcPatternAddString(pattern, FC_FAMILY,
		(const FcChar8 *)requested_family)
	    || !FcPatternAddInteger(pattern, FC_WEIGHT,
		request->weight == XP_TEXT_WEIGHT_BOLD ? FC_WEIGHT_BOLD
						 : FC_WEIGHT_REGULAR)
	    || !FcPatternAddInteger(pattern, FC_SLANT,
		request->slant == XP_TEXT_SLANT_ITALIC ? FC_SLANT_ITALIC
						 : FC_SLANT_ROMAN)
	    || !FcPatternAddBool(pattern, FC_SCALABLE, FcTrue)) {
	    status = XP_TEXT_STATUS_OUT_OF_MEMORY;
	    goto cleanup;
	}
	if (!FcConfigSubstitute(
		system->font_config, pattern, FcMatchPattern)) {
	    status = XP_TEXT_STATUS_BACKEND_ERROR;
	    goto cleanup;
	}
	FcDefaultSubstitute(pattern);
	matches = FcFontSort(
	    system->font_config, pattern, FcTrue, NULL, &result);
	if (matches == NULL) {
	    status = result == FcResultOutOfMemory
		? XP_TEXT_STATUS_OUT_OF_MEMORY
		: XP_TEXT_STATUS_FONT_UNAVAILABLE;
	    goto cleanup;
	}
	for (match_index = 0;
	     match_index < matches->nfont
		 && sources->count < XP_TEXT_MAX_FACES;
	     match_index++) {
	    FcPattern *match = matches->fonts[match_index];
	    FcChar8 *file = NULL;
	    FcChar8 *family = NULL;
	    int face_index = 0;
	    XpTextSource *source;

	    if (!generic
		&& !Fontconfig_pattern_has_family(match, requested_family)) {
		continue;
	    }
	    if (FcPatternGetString(match, FC_FILE, 0, &file)
		    != FcResultMatch
		|| file == NULL) {
		continue;
	    }
	    (void)FcPatternGetInteger(match, FC_INDEX, 0, &face_index);
	    (void)FcPatternGetString(match, FC_FAMILY, 0, &family);
	    if (Source_list_contains(sources, (const char *)file,
				     face_index, NULL)) {
		if (!generic)
		    break;
		continue;
	    }
	    source = &sources->items[sources->count];
	    source->path = Text_duplicate((const char *)file);
	    source->family = Text_duplicate(
		family != NULL ? (const char *)family : "unknown");
	    if (source->path == NULL || source->family == NULL) {
		Source_destroy(source);
		status = XP_TEXT_STATUS_OUT_OF_MEMORY;
		goto cleanup;
	    }
	    source->face_index = face_index;
	    sources->count++;
	    if (!generic)
		break;
	}
	FcFontSetDestroy(matches);
	matches = NULL;
	FcPatternDestroy(pattern);
	pattern = NULL;
	if (sources->count == XP_TEXT_MAX_FACES)
	    break;
    }
    status = sources->count > 0 ? XP_TEXT_STATUS_OK
				: XP_TEXT_STATUS_FONT_UNAVAILABLE;

cleanup:
    if (matches != NULL)
	FcFontSetDestroy(matches);
    if (pattern != NULL)
	FcPatternDestroy(pattern);
    return status;
}

#else

static wchar_t *Utf8_to_wide(const char *utf8)
{
    wchar_t *wide;
    int length;

    length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
				 utf8, -1, NULL, 0);
    if (length <= 0)
	return NULL;
    wide = malloc((size_t)length * sizeof(*wide));
    if (wide == NULL)
	return NULL;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
			    utf8, -1, wide, length) != length) {
	free(wide);
	return NULL;
    }
    return wide;
}

static char *Wide_to_utf8(const wchar_t *wide)
{
    char *utf8;
    int length;

    length = WideCharToMultiByte(CP_UTF8, 0, wide, -1,
				 NULL, 0, NULL, NULL);
    if (length <= 0)
	return NULL;
    utf8 = malloc((size_t)length);
    if (utf8 == NULL)
	return NULL;
    if (WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, length,
			    NULL, NULL) != length) {
	free(utf8);
	return NULL;
    }
    return utf8;
}

static int Wide_extension_is_font(const wchar_t *name)
{
    const wchar_t *extension = wcsrchr(name, L'.');

    return extension != NULL
	&& (_wcsicmp(extension, L".ttf") == 0
	    || _wcsicmp(extension, L".ttc") == 0
	    || _wcsicmp(extension, L".otf") == 0);
}

static void Windows_unregister_private_fonts(XpTextSystem *system)
{
    size_t index;

    for (index = 0; index < system->private_font_count; index++) {
	RemoveFontResourceExW(system->private_font_paths[index], FR_PRIVATE,
			      NULL);
	free(system->private_font_paths[index]);
    }
    free(system->private_font_paths);
    system->private_font_paths = NULL;
    system->private_font_count = 0;
}

static void Windows_register_private_fonts(XpTextSystem *system,
					   const char *directory)
{
    WIN32_FIND_DATAW data;
    HANDLE search = INVALID_HANDLE_VALUE;
    wchar_t *wide_directory = NULL;
    wchar_t *pattern = NULL;
    size_t directory_length;

    if (directory == NULL || directory[0] == '\0')
	return;
    wide_directory = Utf8_to_wide(directory);
    if (wide_directory == NULL)
	return;
    directory_length = wcslen(wide_directory);
    pattern = malloc((directory_length + 3) * sizeof(*pattern));
    if (pattern == NULL)
	goto cleanup;
    wcscpy(pattern, wide_directory);
    if (directory_length > 0
	&& wide_directory[directory_length - 1] != L'\\'
	&& wide_directory[directory_length - 1] != L'/') {
	pattern[directory_length++] = L'\\';
    }
    pattern[directory_length++] = L'*';
    pattern[directory_length] = L'\0';
    search = FindFirstFileW(pattern, &data);
    if (search == INVALID_HANDLE_VALUE)
	goto cleanup;
    do {
	wchar_t *path;
	wchar_t **paths;
	size_t name_length;

	if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0
	    || !Wide_extension_is_font(data.cFileName)) {
	    continue;
	}
	name_length = wcslen(data.cFileName);
	path = malloc((directory_length + name_length + 1) * sizeof(*path));
	if (path == NULL)
	    continue;
	wmemcpy(path, pattern, directory_length - 1);
	wcscpy(path + directory_length - 1, data.cFileName);
	if (AddFontResourceExW(path, FR_PRIVATE, NULL) == 0) {
	    free(path);
	    continue;
	}
	paths = realloc(system->private_font_paths,
			(system->private_font_count + 1) * sizeof(*paths));
	if (paths == NULL) {
	    RemoveFontResourceExW(path, FR_PRIVATE, NULL);
	    free(path);
	    continue;
	}
	system->private_font_paths = paths;
	system->private_font_paths[system->private_font_count++] = path;
    } while (FindNextFileW(search, &data));

cleanup:
    if (search != INVALID_HANDLE_VALUE)
	FindClose(search);
    free(pattern);
    free(wide_directory);
}

static XpTextStatus Windows_load_family(const char *family,
					const XpTextFontRequest *request,
					XpTextSource *source)
{
    LOGFONTW descriptor;
    wchar_t actual_family[LF_FACESIZE];
    wchar_t *requested_family = NULL;
    HDC device = NULL;
    HFONT font = NULL;
    HGDIOBJ previous = NULL;
    DWORD size;
    XpTextStatus status = XP_TEXT_STATUS_FONT_UNAVAILABLE;

    requested_family = Utf8_to_wide(family);
    if (requested_family == NULL)
	return XP_TEXT_STATUS_OUT_OF_MEMORY;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.lfHeight = -(LONG)ceilf(request->pixel_height);
    descriptor.lfWeight = request->weight == XP_TEXT_WEIGHT_BOLD
	? FW_BOLD : FW_NORMAL;
    descriptor.lfItalic = request->slant == XP_TEXT_SLANT_ITALIC;
    descriptor.lfCharSet = DEFAULT_CHARSET;
    descriptor.lfPitchAndFamily = request->spacing == XP_TEXT_SPACING_MONOSPACE
	? FIXED_PITCH | FF_DONTCARE : VARIABLE_PITCH | FF_DONTCARE;
    wcsncpy(descriptor.lfFaceName, requested_family, LF_FACESIZE - 1);
    font = CreateFontIndirectW(&descriptor);
    device = CreateCompatibleDC(NULL);
    if (font == NULL || device == NULL)
	goto cleanup;
    previous = SelectObject(device, font);
    if (previous == NULL || previous == HGDI_ERROR)
	goto cleanup;
    if (GetTextFaceW(device, LF_FACESIZE, actual_family) <= 0)
	goto cleanup;
    if (_wcsicmp(actual_family, requested_family) != 0)
	goto cleanup;
    size = GetFontData(device, 0, 0, NULL, 0);
    if (size == GDI_ERROR || size == 0)
	goto cleanup;
    source->memory = malloc(size);
    if (source->memory == NULL) {
	status = XP_TEXT_STATUS_OUT_OF_MEMORY;
	goto cleanup;
    }
    if (GetFontData(device, 0, 0, source->memory, size) != size)
	goto cleanup;
    source->family = Wide_to_utf8(actual_family);
    if (source->family == NULL) {
	status = XP_TEXT_STATUS_OUT_OF_MEMORY;
	goto cleanup;
    }
    source->memory_size = size;
    status = XP_TEXT_STATUS_OK;

cleanup:
    if (status != XP_TEXT_STATUS_OK)
	Source_destroy(source);
    if (previous != NULL && previous != HGDI_ERROR)
	SelectObject(device, previous);
    if (font != NULL)
	DeleteObject(font);
    if (device != NULL)
	DeleteDC(device);
    free(requested_family);
    return status;
}

static XpTextStatus Resolve_sources(XpTextSystem *system,
				    const XpTextFontRequest *request,
				    const XpTextFamilyList *families,
				    XpTextSourceList *sources)
{
    static const char *mono_generics[] = {
	"Consolas", "Courier New", "Lucida Console"
    };
    static const char *sans_generics[] = {
	"Segoe UI", "Arial", "Tahoma"
    };
    size_t family_index;

    (void)system;
    for (family_index = 0;
	 family_index < families->count
	     && sources->count < XP_TEXT_MAX_FACES;
	 family_index++) {
	const char *family = families->items[family_index];
	const char *single_family[1];
	const char **generic_list = NULL;
	size_t generic_count = 0;
	size_t generic_index;

	if (strcmp(family, "monospace") == 0) {
	    generic_list = mono_generics;
	    generic_count = sizeof(mono_generics) / sizeof(mono_generics[0]);
	} else if (strcmp(family, "sans-serif") == 0) {
	    generic_list = sans_generics;
	    generic_count = sizeof(sans_generics) / sizeof(sans_generics[0]);
	}
	if (generic_list == NULL) {
	    single_family[0] = family;
	    generic_list = single_family;
	    generic_count = 1;
	}
	for (generic_index = 0;
	     generic_index < generic_count
		 && sources->count < XP_TEXT_MAX_FACES;
	     generic_index++) {
	    XpTextSource candidate;
	    XpTextStatus status;

	    memset(&candidate, 0, sizeof(candidate));
	    status = Windows_load_family(
		generic_list[generic_index], request, &candidate);
	    if (status == XP_TEXT_STATUS_OUT_OF_MEMORY) {
		Source_destroy(&candidate);
		return status;
	    }
	    if (status != XP_TEXT_STATUS_OK)
		continue;
	    if (Source_list_contains(
		    sources, NULL, 0, candidate.family)) {
		Source_destroy(&candidate);
		continue;
	    }
	    sources->items[sources->count++] = candidate;
	}
    }
    return sources->count > 0 ? XP_TEXT_STATUS_OK
			      : XP_TEXT_STATUS_FONT_UNAVAILABLE;
}

#endif

static TTF_Font *Open_source_font(const XpTextSource *source,
				  float pixel_height)
{
    if (source->path != NULL) {
	SDL_PropertiesID properties = SDL_CreateProperties();
	TTF_Font *font = NULL;

	if (properties == 0)
	    return NULL;
	if (SDL_SetStringProperty(properties,
		TTF_PROP_FONT_CREATE_FILENAME_STRING, source->path)
	    && SDL_SetFloatProperty(properties,
		TTF_PROP_FONT_CREATE_SIZE_FLOAT, pixel_height)
	    && SDL_SetNumberProperty(properties,
		TTF_PROP_FONT_CREATE_FACE_NUMBER, source->face_index)) {
	    font = TTF_OpenFontWithProperties(properties);
	}
	SDL_DestroyProperties(properties);
	return font;
    }
    if (source->memory != NULL && source->memory_size > 0) {
	SDL_IOStream *stream = SDL_IOFromConstMem(
	    source->memory, source->memory_size);

	if (stream == NULL)
	    return NULL;
	return TTF_OpenFontIO(stream, true, pixel_height);
    }
    return NULL;
}

XpTextStatus Xp_text_system_create(const char *application_font_directory,
				   XpTextSystem **system)
{
    XpTextSystem *candidate;

    if (system == NULL || *system != NULL)
	return XP_TEXT_STATUS_INVALID_ARGUMENT;
    candidate = calloc(1, sizeof(*candidate));
    if (candidate == NULL)
	return XP_TEXT_STATUS_OUT_OF_MEMORY;
    if (!TTF_Init()) {
	free(candidate);
	return XP_TEXT_STATUS_BACKEND_ERROR;
    }
    candidate->surface_engine = TTF_CreateSurfaceTextEngine();
    if (candidate->surface_engine == NULL) {
	TTF_Quit();
	free(candidate);
	return XP_TEXT_STATUS_BACKEND_ERROR;
    }
#ifdef _WINDOWS
    Windows_register_private_fonts(candidate, application_font_directory);
#else
    candidate->font_config = FcInitLoadConfigAndFonts();
    if (candidate->font_config == NULL) {
	TTF_DestroySurfaceTextEngine(candidate->surface_engine);
	TTF_Quit();
	free(candidate);
	return XP_TEXT_STATUS_BACKEND_ERROR;
    }
    if (application_font_directory != NULL
	&& application_font_directory[0] != '\0') {
	(void)FcConfigAppFontAddDir(
	    candidate->font_config,
	    (const FcChar8 *)application_font_directory);
    }
#endif
    *system = candidate;
    return XP_TEXT_STATUS_OK;
}

void Xp_text_system_destroy(XpTextSystem **system)
{
    if (system == NULL || *system == NULL)
	return;
#ifdef _WINDOWS
    Windows_unregister_private_fonts(*system);
#else
    FcConfigDestroy((*system)->font_config);
#endif
    TTF_DestroySurfaceTextEngine((*system)->surface_engine);
    TTF_Quit();
    free(*system);
    *system = NULL;
}

static int Font_request_valid(const XpTextFontRequest *request)
{
    return request != NULL && request->family_list != NULL
	&& request->family_list[0] != '\0'
	&& isfinite(request->pixel_height) && request->pixel_height > 0.0f
	&& (request->weight == XP_TEXT_WEIGHT_NORMAL
	    || request->weight == XP_TEXT_WEIGHT_BOLD)
	&& (request->slant == XP_TEXT_SLANT_NORMAL
	    || request->slant == XP_TEXT_SLANT_ITALIC)
	&& (request->spacing == XP_TEXT_SPACING_PROPORTIONAL
	    || request->spacing == XP_TEXT_SPACING_MONOSPACE);
}

XpTextStatus Xp_text_font_open(XpTextSystem *system,
			       const XpTextFontRequest *request,
			       XpTextFont **font)
{
    XpTextFamilyList families;
    XpTextSourceList sources;
    XpTextFont *candidate = NULL;
    XpTextStatus status;
    size_t source_index;

    if (system == NULL || !Font_request_valid(request)
	|| font == NULL || *font != NULL) {
	return XP_TEXT_STATUS_INVALID_ARGUMENT;
    }
    memset(&families, 0, sizeof(families));
    memset(&sources, 0, sizeof(sources));
    status = Family_list_build(request, &families);
    if (status != XP_TEXT_STATUS_OK)
	goto cleanup;
    status = Resolve_sources(system, request, &families, &sources);
    if (status != XP_TEXT_STATUS_OK)
	goto cleanup;
    candidate = calloc(1, sizeof(*candidate));
    if (candidate == NULL) {
	status = XP_TEXT_STATUS_OUT_OF_MEMORY;
	goto cleanup;
    }
    candidate->system = system;
    for (source_index = 0; source_index < sources.count; source_index++) {
	XpTextSource *source = &sources.items[source_index];
	TTF_Font *ttf = Open_source_font(source, request->pixel_height);
	XpTextFace *face;

	if (ttf == NULL)
	    continue;
	face = &candidate->faces[candidate->face_count];
	face->ttf = ttf;
	face->family = source->family;
	source->family = NULL;
	face->memory = source->memory;
	face->memory_size = source->memory_size;
	source->memory = NULL;
	source->memory_size = 0;
	if (candidate->face_count > 0
	    && !TTF_AddFallbackFont(candidate->faces[0].ttf, ttf)) {
	    TTF_CloseFont(ttf);
	    free(face->family);
	    free(face->memory);
	    memset(face, 0, sizeof(*face));
	    continue;
	}
	candidate->face_count++;
    }
    if (candidate->face_count == 0) {
	status = XP_TEXT_STATUS_FONT_UNAVAILABLE;
	goto cleanup;
    }
    candidate->ascent = TTF_GetFontAscent(candidate->faces[0].ttf);
    candidate->line_spacing = TTF_GetFontLineSkip(candidate->faces[0].ttf);
    if (candidate->ascent <= 0 || candidate->line_spacing <= 0) {
	status = XP_TEXT_STATUS_BACKEND_ERROR;
	goto cleanup;
    }
    *font = candidate;
    candidate = NULL;
    status = XP_TEXT_STATUS_OK;

cleanup:
    if (candidate != NULL)
	Xp_text_font_close(&candidate);
    Source_list_destroy(&sources);
    Family_list_destroy(&families);
    return status;
}

void Xp_text_font_close(XpTextFont **font)
{
    size_t index;

    if (font == NULL || *font == NULL)
	return;
    if ((*font)->face_count > 0)
	TTF_ClearFallbackFonts((*font)->faces[0].ttf);
    for (index = (*font)->face_count; index > 0; index--) {
	XpTextFace *face = &(*font)->faces[index - 1];

	TTF_CloseFont(face->ttf);
	free(face->family);
	free(face->memory);
    }
    free(*font);
    *font = NULL;
}

size_t Xp_text_font_resolved_count(const XpTextFont *font)
{
    return font != NULL ? font->face_count : 0;
}

const char *Xp_text_font_resolved_family(const XpTextFont *font,
					 size_t index)
{
    if (font == NULL || index >= font->face_count)
	return NULL;
    return font->faces[index].family;
}

int Xp_text_font_has_glyph(const XpTextFont *font, uint32_t codepoint)
{
    size_t index;

    if (font == NULL || codepoint > 0x10ffff
	|| (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
	return 0;
    }
    for (index = 0; index < font->face_count; index++) {
	if (TTF_FontHasGlyph(font->faces[index].ttf, codepoint))
	    return 1;
    }
    return 0;
}

TTF_Font *Xp_text_font_primary_ttf(XpTextFont *font)
{
    if (font == NULL || font->face_count == 0)
	return NULL;
    return font->faces[0].ttf;
}

XpTextStatus Xp_text_layout_create(XpTextFont *font,
				   const XpTextLayoutRequest *request,
				   XpTextLayout **layout)
{
    static const char empty_text[] = "";
    XpTextLayout *candidate;
    size_t valid_length = 0;
    XpTextStatus status;

    if (font == NULL || request == NULL || layout == NULL || *layout != NULL
	|| (request->utf8 == NULL && request->byte_length != 0)
	|| (request->direction != XP_TEXT_DIRECTION_AUTO
	    && request->direction != XP_TEXT_DIRECTION_LTR
	    && request->direction != XP_TEXT_DIRECTION_RTL)) {
	return XP_TEXT_STATUS_INVALID_ARGUMENT;
    }
    if (request->direction == XP_TEXT_DIRECTION_RTL)
	return XP_TEXT_STATUS_UNSUPPORTED_DIRECTION;
    status = Xp_text_complete_utf8_prefix(
	request->utf8, request->byte_length, &valid_length);
    if (status != XP_TEXT_STATUS_OK)
	return status;
    if (valid_length != request->byte_length)
	return XP_TEXT_STATUS_INVALID_UTF8;
    candidate = calloc(1, sizeof(*candidate));
    if (candidate == NULL)
	return XP_TEXT_STATUS_OUT_OF_MEMORY;
    candidate->language = Text_duplicate(
	request->language_bcp47 != NULL ? request->language_bcp47 : "");
    if (candidate->language == NULL) {
	free(candidate);
	return XP_TEXT_STATUS_OUT_OF_MEMORY;
    }
    candidate->text = TTF_CreateText(
	font->system->surface_engine, font->faces[0].ttf,
	request->utf8 != NULL ? request->utf8 : empty_text,
	request->byte_length);
    if (candidate->text == NULL) {
	free(candidate->language);
	free(candidate);
	return XP_TEXT_STATUS_BACKEND_ERROR;
    }
    if ((request->direction == XP_TEXT_DIRECTION_LTR
	 && !TTF_SetTextDirection(candidate->text, TTF_DIRECTION_LTR))
	|| !TTF_SetTextColor(candidate->text, 255, 255, 255, 255)) {
	TTF_DestroyText(candidate->text);
	free(candidate->language);
	free(candidate);
	return XP_TEXT_STATUS_BACKEND_ERROR;
    }
    candidate->font = font;
    candidate->direction = request->direction;
    *layout = candidate;
    return XP_TEXT_STATUS_OK;
}

void Xp_text_layout_destroy(XpTextLayout **layout)
{
    if (layout == NULL || *layout == NULL)
	return;
    TTF_DestroyText((*layout)->text);
    free((*layout)->language);
    free(*layout);
    *layout = NULL;
}

XpTextDirection Xp_text_layout_direction(const XpTextLayout *layout)
{
    return layout != NULL ? layout->direction : XP_TEXT_DIRECTION_AUTO;
}

const char *Xp_text_layout_language(const XpTextLayout *layout)
{
    return layout != NULL ? layout->language : "";
}

XpTextStatus Xp_text_layout_measure(const XpTextLayout *layout,
				    XpTextMetrics *metrics)
{
    XpTextMetrics candidate;

    if (layout == NULL || metrics == NULL)
	return XP_TEXT_STATUS_INVALID_ARGUMENT;
    if (!TTF_GetTextSize(layout->text, &candidate.width, &candidate.height))
	return XP_TEXT_STATUS_BACKEND_ERROR;
    if (candidate.height == 0)
	candidate.height = TTF_GetFontHeight(layout->font->faces[0].ttf);
    candidate.ascent = layout->font->ascent;
    candidate.line_spacing = layout->font->line_spacing;
    if (candidate.width < 0 || candidate.height <= 0
	|| candidate.ascent <= 0 || candidate.line_spacing <= 0) {
	return XP_TEXT_STATUS_BACKEND_ERROR;
    }
    *metrics = candidate;
    return XP_TEXT_STATUS_OK;
}

XpTextStatus Xp_text_layout_rasterize(const XpTextLayout *layout,
				      XpTextBitmap *bitmap)
{
    XpTextMetrics metrics;
    XpTextBitmap candidate;
    SDL_Surface *surface = NULL;
    int locked = 0;
    int row;
    XpTextStatus status;

    if (layout == NULL || bitmap == NULL || bitmap->pixels != NULL
	|| bitmap->width != 0 || bitmap->height != 0 || bitmap->pitch != 0) {
	return XP_TEXT_STATUS_INVALID_ARGUMENT;
    }
    status = Xp_text_layout_measure(layout, &metrics);
    if (status != XP_TEXT_STATUS_OK)
	return status;
    memset(&candidate, 0, sizeof(candidate));
    candidate.width = metrics.width;
    candidate.height = metrics.height;
    if (metrics.width == 0) {
	*bitmap = candidate;
	return XP_TEXT_STATUS_OK;
    }
    if ((size_t)metrics.width > SIZE_MAX / 4
	|| (size_t)metrics.height > SIZE_MAX / ((size_t)metrics.width * 4)) {
	return XP_TEXT_STATUS_OUT_OF_MEMORY;
    }
    surface = SDL_CreateSurface(
	metrics.width, metrics.height, SDL_PIXELFORMAT_RGBA32);
    if (surface == NULL || !SDL_FillSurfaceRect(surface, NULL, 0)
	|| !TTF_DrawSurfaceText(layout->text, 0, 0, surface)) {
	status = XP_TEXT_STATUS_BACKEND_ERROR;
	goto cleanup;
    }
    candidate.scale = 1.0f;
    candidate.pitch = (size_t)metrics.width * 4;
    candidate.pixels = malloc(candidate.pitch * (size_t)metrics.height);
    if (candidate.pixels == NULL) {
	status = XP_TEXT_STATUS_OUT_OF_MEMORY;
	goto cleanup;
    }
    if (SDL_MUSTLOCK(surface)) {
	if (!SDL_LockSurface(surface)) {
	    status = XP_TEXT_STATUS_BACKEND_ERROR;
	    goto cleanup;
	}
	locked = 1;
    }
    for (row = 0; row < metrics.height; row++) {
	memcpy(candidate.pixels + (size_t)row * candidate.pitch,
	       (const unsigned char *)surface->pixels
		   + (size_t)row * (size_t)surface->pitch,
	       candidate.pitch);
    }
    status = XP_TEXT_STATUS_OK;

cleanup:
    if (locked)
	SDL_UnlockSurface(surface);
    if (surface != NULL)
	SDL_DestroySurface(surface);
    if (status != XP_TEXT_STATUS_OK) {
	free(candidate.pixels);
	return status;
    }
    *bitmap = candidate;
    return XP_TEXT_STATUS_OK;
}

void Xp_text_bitmap_destroy(XpTextBitmap *bitmap)
{
    if (bitmap == NULL)
	return;
    free(bitmap->pixels);
    memset(bitmap, 0, sizeof(*bitmap));
}

const char *Xp_text_status_string(XpTextStatus status)
{
    switch (status) {
    case XP_TEXT_STATUS_OK:
	return "ok";
    case XP_TEXT_STATUS_INVALID_ARGUMENT:
	return "invalid argument";
    case XP_TEXT_STATUS_INVALID_UTF8:
	return "invalid UTF-8";
    case XP_TEXT_STATUS_UNSUPPORTED_DIRECTION:
	return "unsupported text direction";
    case XP_TEXT_STATUS_OUT_OF_MEMORY:
	return "out of memory";
    case XP_TEXT_STATUS_FONT_UNAVAILABLE:
	return "font unavailable";
    case XP_TEXT_STATUS_BACKEND_ERROR:
	return "text backend error";
    default:
	return "unknown text status";
    }
}

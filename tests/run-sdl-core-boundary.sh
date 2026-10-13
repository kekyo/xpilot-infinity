#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
production_source="$source_root/src/client/sdl"
object_dir=${XPILOT_SDL_OBJECT_DIR:?
XPILOT_SDL_OBJECT_DIR is required}
production_binary=${XPILOT_SDL_BINARY:?
XPILOT_SDL_BINARY is required}
production_makefile="$object_dir/Makefile"
symbols_file=$(mktemp "${TMPDIR:-/tmp}/xpilot-sdl-core-boundary.XXXXXX")

cleanup()
{
    rm -f -- "$symbols_file"
}

fail()
{
    echo "SDL core boundary failure: $*" >&2
    exit 1
}

trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

for required_path in "$production_makefile" "$production_binary"; do
    if test ! -r "$required_path"; then
        fail "compiled production input is missing: $required_path"
    fi
done
if test ! -x "$production_binary"; then
    fail "compiled SDL client is not executable: $production_binary"
fi
if ! command -v nm >/dev/null 2>&1; then
    fail "nm is required to inspect production objects"
fi

for removed_file in renderer_gl_legacy.c renderer_gl_legacy.h; do
    if test -e "$production_source/$removed_file"; then
        fail "retired production backend remains: $removed_file"
    fi
done

if grep -En \
    'renderer_gl_legacy|Sdl_renderer_(prepare_legacy|flush_preserving_legacy)' \
    "$production_source"/*.c "$production_source"/*.h \
    "$production_source"/Makefile.am "$production_source"/Makefile.in \
    >"$symbols_file"; then
    sed -n '1,80p' "$symbols_file" >&2
    fail "retired legacy renderer API remains in the production boundary"
fi

if grep -En \
    '(^|[^[:alnum:]_])(gl(Begin|End|Vertex[234]|Color[34]|TexCoord[234]|MatrixMode|PushMatrix|PopMatrix|LoadIdentity|LoadMatrix[fd]?|MultMatrix[fd]?|Ortho|Frustum|Translate[fd]|Rotate[fd]|Scale[fd]|PushAttrib|PopAttrib|ShadeModel|AlphaFunc|TexEnv|EnableClientState|DisableClientState|VertexPointer|ColorPointer|TexCoordPointer|LineStipple|PolygonStipple|RasterPos|Bitmap|DrawPixels|CopyPixels|GenLists|NewList|EndList|CallList|CallLists|DeleteLists|ListBase)|glu[A-Za-z0-9_]*)[[:space:]]*\(' \
    "$production_source"/*.c "$production_source"/*.h \
    >"$symbols_file"; then
    sed -n '1,80p' "$symbols_file" >&2
    fail "fixed-function, display-list, or GLU call remains in production"
fi

if grep -En 'GLU_CFLAGS|GLU_LIBS|<GL/glu\.h>' \
    "$production_source"/*.c "$production_source"/*.h \
    "$production_source"/Makefile.am "$production_source"/Makefile.in \
    >"$symbols_file"; then
    sed -n '1,80p' "$symbols_file" >&2
    fail "SDL production still declares a GLU dependency"
fi
if grep -En 'PKG_CHECK_MODULES\(\[?GLU' \
    "$source_root/configure.ac" >"$symbols_file"; then
    sed -n '1,80p' "$symbols_file" >&2
    fail "the repository still requires GLU at configure time"
fi

object_extension=$(awk '
    $1 == "OBJEXT" && $2 == "=" { print $3; exit }
' "$production_makefile")
if test -z "$object_extension"; then
    fail "could not determine the production object extension"
fi
production_objects=$(awk -v extension="$object_extension" '
    /^am_xpilot_infinity_sdl_OBJECTS[[:space:]]*=/ {
        collecting = 1
        sub(/^[^=]*=[[:space:]]*/, "")
    }
    collecting {
        continued = $0 ~ /\\[[:space:]]*$/
        gsub(/\\/, " ")
        for (field_number = 1; field_number <= NF; field_number++) {
            if ($field_number ~ /\.\$\(OBJEXT\)$/) {
                sub(/\.\$\(OBJEXT\)$/, "." extension, $field_number)
                print $field_number
            }
        }
        if (!continued)
            exit
    }
' "$production_makefile")
if test -z "$production_objects"; then
    fail "could not enumerate the production SDL objects"
fi

for object_name in $production_objects; do
    object_file="$object_dir/$object_name"
    case "${object_name##*/}" in
        renderer_gl_core.*|gl_diagnostics.*)
            continue
            ;;
    esac
    if test ! -f "$object_file"; then
        fail "production object is missing: $object_file"
    fi
    if ! nm -u "$object_file" >"$symbols_file"; then
        fail "could not inspect production object: $object_file"
    fi
    for symbol in $(awk '{
        symbol = $NF
        sub(/^_/, "", symbol)
        sub(/@.*/, "", symbol)
        print symbol
    }' "$symbols_file"); do
        case "$symbol" in
            gl[A-Z]*|glu[A-Z]*)
                fail "$object_name bypasses the semantic renderer: $symbol"
                ;;
        esac
    done
done

if ! nm -g "$production_binary" >"$symbols_file"; then
    fail "could not inspect compiled SDL client symbols"
fi
if ! awk '{ print $NF }' "$symbols_file" \
    | sed 's/^_//; s/@.*$//' \
    | grep -qx 'Renderer_gl_core_create'; then
    fail "compiled SDL client does not contain the core backend factory"
fi
if awk '{ print $NF }' "$symbols_file" \
    | sed 's/^_//; s/@.*$//' \
    | grep -Eq '^(Renderer_gl_legacy_create|Sdl_renderer_prepare_legacy|Sdl_renderer_flush_preserving_legacy)$'; then
    fail "compiled SDL client contains a retired legacy renderer symbol"
fi

if command -v readelf >/dev/null 2>&1; then
    if readelf -d "$production_binary" 2>/dev/null \
        | grep -q 'NEEDED.*libGLU'; then
        fail "compiled SDL client still links libGLU"
    fi
elif command -v objdump >/dev/null 2>&1; then
    if objdump -p "$production_binary" 2>/dev/null \
        | grep -q 'NEEDED.*libGLU'; then
        fail "compiled SDL client still links libGLU"
    fi
else
    fail "readelf or objdump is required to inspect linked dependencies"
fi

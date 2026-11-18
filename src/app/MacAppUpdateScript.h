#pragma once

// The installer runs outside the application so it can wait for shutdown and replace the whole
// signed bundle. Paths and translated diagnostics are passed as arguments, never as shell source.
inline constexpr char kMacAppUpdateScript[] = R"SONGBIRD(
set -e
umask 077

pid=$1
image=$2
app=$3
data=$4
work=$5
title=$6
failureMessage=$7
configArgument=$8
oldConfigArgument=$9
shift 9

exec >>"$work/install.log" 2>&1
mount="$work/image"
lock="$data/updates/.install-lock"
stage=
mounted=0
locked=0
installed=0

cleanup() {
    status=$?
    trap - EXIT
    set +e
    if [ "$mounted" -eq 1 ]; then
        /usr/bin/hdiutil detach "$mount" || /usr/bin/hdiutil detach -force "$mount" || true
    fi
    if [ "$status" -ne 0 ]; then
        if [ -n "$stage" ] && [ -d "$stage/previous.app" ]; then
            if [ "$installed" -eq 1 ]; then
                /bin/mv "$app" "$stage/failed.app" || true
            fi
            if [ ! -e "$app" ]; then
                /bin/mv "$stage/previous.app" "$app" || echo "Previous bundle: $stage/previous.app"
            fi
        fi
        if ! /bin/kill -0 "$pid" 2>/dev/null && [ -d "$app" ]; then
            /usr/bin/open "$app" --args "$oldConfigArgument" "$@" || true
        fi
    fi
    if [ -n "$stage" ] && { [ "$status" -eq 0 ] || [ ! -d "$stage/previous.app" ]; }; then
        /bin/rm -rf "$stage"
    fi
    if [ "$locked" -eq 1 ]; then
        /bin/rmdir "$lock"
    fi
    if [ "$status" -eq 0 ] && [ "$mounted" -eq 0 ]; then
        /bin/rm -rf "$work"
    elif [ "$status" -ne 0 ]; then
        /usr/bin/osascript - "$title" "$failureMessage" <<'APPLESCRIPT' || true
on run argv
    display alert (item 1 of argv) message (item 2 of argv) as warning
end run
APPLESCRIPT
    fi
}

trap 'cleanup "$@"' EXIT
trap 'exit 1' HUP INT TERM

/bin/mkdir "$lock"
locked=1
for ((attempt = 0; attempt < 120; ++attempt)); do
    /bin/kill -0 "$pid" 2>/dev/null || break
    /bin/sleep 1
done
if /bin/kill -0 "$pid" 2>/dev/null; then
    echo "SongBird did not exit within 120 seconds."
    exit 1
fi

/bin/mkdir "$mount"
/usr/bin/hdiutil attach -readonly -nobrowse -mountpoint "$mount" "$image"
mounted=1

stage=$(/usr/bin/mktemp -d "${app%/*}/.songbird-update.XXXXXXXX")
candidate="$stage/SongBird.app"
/usr/bin/ditto "$mount/SongBird.app" "$candidate"
/usr/bin/hdiutil detach "$mount"
mounted=0

/usr/bin/codesign --verify --deep --strict "$candidate"
bundleValue() {
    /usr/libexec/PlistBuddy -c "Print :$2" "$1/Contents/Info.plist"
}
if [ "$(bundleValue "$candidate" CFBundleIdentifier)" != "$(bundleValue "$app" CFBundleIdentifier)" ] ||
   [ "$(bundleValue "$candidate" CFBundleExecutable)" != "$(bundleValue "$app" CFBundleExecutable)" ]; then
    echo "The downloaded application does not match the installed bundle."
    exit 1
fi

# Older versions stored configuration, state and downloaded cores alongside the executable.
# Copy missing files only. The current default configuration is copied last so it can only
# become the default after the rest of the legacy data is available.
/bin/mkdir -p "$data"
legacy="$app/Contents/MacOS"
executable=$(bundleValue "$app" CFBundleExecutable)
shopt -s nullglob dotglob
for source in "$legacy"/*; do
    name=${source##*/}
    case "$name" in
        "$executable"|songbird.json|updates|runtime) continue ;;
    esac
    if [ ! -e "$candidate/Contents/MacOS/$name" ] &&
       [ ! -e "$data/$name" ] && [ ! -L "$data/$name" ]; then
        /bin/cp -pPR "$source" "$work/legacy-entry"
        /bin/mv "$work/legacy-entry" "$data/$name"
    fi
done
if [ -f "$legacy/songbird.json" ] && [ ! -e "$data/songbird.json" ] && [ ! -L "$data/songbird.json" ]; then
    /bin/cp -pP "$legacy/songbird.json" "$work/songbird.json"
    /bin/mv -n "$work/songbird.json" "$data/songbird.json"
fi

oldConfig=${oldConfigArgument#--config=}
newConfig=${configArgument#--config=}
if [ "$oldConfig" != "$newConfig" ] && [ -f "$oldConfig" ] &&
   ! /usr/bin/cmp -s "$oldConfig" "$newConfig"; then
    echo "The active configuration could not be migrated without overwriting an existing file."
    exit 1
fi

echo "Previous bundle backup: $stage/previous.app"
/bin/mv "$app" "$stage/previous.app"
/bin/mv "$candidate" "$app"
installed=1
/usr/bin/open -n "$app" --args "$configArgument" "$@"
)SONGBIRD";

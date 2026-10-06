---
name: songbird-i18n-string
description: Add or change a user-visible (translatable) string in the SongBird repository at D:\sss\v2rayq without breaking the translation checks. Use when adding a new tr()/translate() message, editing an existing one, or when ctest reports localization-coverage / translations-fresh failing.
---

# Adding a user-visible string to SongBird

Use this skill only for the SongBird repository at `D:\sss\v2rayq`.

The `.ts` is **hand-maintained** (there is no lupdate build target) and is **CRLF**, while
`.cpp`/`.ps1` are LF. Getting either wrong is silent: the build stays green and the UI keeps
showing English. Follow all five steps; skipping the last two is how a string ships untranslated.

## 1. Write the string

- Use `QCoreApplication::translate("<Context>", "...")` in a free function / helper, or `tr("...")`
  in a `QObject` member function. The context is the class name for `tr()`, or whatever you pass to
  `translate()`.
- **House style for a message with a reason: `context + " " + reason`.** Append the reason rather
  than substituting it into a `%1` slot, so the translator owns a complete sentence.
- One complete sentence per message. Do **not** glue a subject fragment and a verb fragment into a
  shared `"%1 %2"` template -- Chinese word order cannot reorder a template's slots.
- **`src/auto/` is an English surface.** Bare `QStringLiteral` only; `tr()`/`translate()`/
  `QT_TR_*` there is a Check E violation. `src/services/`, `src/appcore/`, `src/ui/` keep
  `translate()`.
- There is no `%n` plural form anywhere in this repo. If the count can be 1, rephrase so the plural
  reads correctly, or make the branch that emits it impossible with a count of 1.

## 2. Hand-add the entry to the .ts

The file is CRLF; edit it with Python so the line endings are exact:

```bash
C:/Users/atom.DELL/.workbuddy-ai/binaries/python/versions/3.13.12/python.exe -c "
p='translations/SongBird_zh_CN.ts'
d=open(p,'rb').read().decode('utf-8')
anchor='    </message>\r\n</context>\r\n'   # pick a UNIQUE anchor
assert d.count(anchor)==1
addition=('    <message>\r\n'
          '        <source>Your new string.</source>\r\n'
          '        <translation>你的新字符串。</translation>\r\n'
          '    </message>\r\n')
d=d.replace(anchor, addition+anchor, 1)
open(p,'wb').write(d.encode('utf-8'))
"
```

Then confirm the file is still valid and still fully CRLF:

```bash
python -c "
import xml.etree.ElementTree as ET
d=open('translations/SongBird_zh_CN.ts','rb').read().decode('utf-8')
print('lone LF:', d.count(chr(10))-d.count(chr(13)+chr(10)))   # must be 0
ET.fromstring(d); print('XML OK')"
```

The `<source>` text must match the code **byte for byte**, including punctuation. Chinese entries
in this repo use full-width punctuation (`：` `，` `（）`).

## 3. Refresh the .qm and its hash

`check-translations-fresh.ps1` does not accept `-SourceRoot`, so
`.workbuddy-ai/tools/run-check-in-runspace.ps1` cannot run it. Reproduce `-Update` in Bash instead:

```bash
QTB=/d/local/Qt5/5.15.2/msvc2019_64/bin
"$QTB/lrelease.exe" translations/SongBird_zh_CN.ts -qm translations/compiled/SongBird_zh_CN.qm
# prove the committed .qm really came from this .ts
"$QTB/lrelease.exe" translations/SongBird_zh_CN.ts -qm /tmp/v.qm
cmp /tmp/v.qm translations/compiled/SongBird_zh_CN.qm && echo BYTE-EXACT
# manifest = lowercase hex + CRLF (matches Set-Content -Encoding ascii)
printf '%s\r\n' "$(sha256sum translations/SongBird_zh_CN.ts | cut -d' ' -f1)" \
    > translations/compiled/SongBird_zh_CN.qm.ts.sha256
```

If you **remove** a string from the code, delete its `.ts` entry the same way and redo this step.
Leaving it behind is not a check failure (Check B is one-directional) but it is dead weight.

## 4. Prove the message is translated, not just present

Add a `TaggingTranslator` case to the test target that covers the file. The pattern lives in
`tests/ProxyCrashRestartPolicyTests.cpp` and `tests/ProxyHealthWatchPolicyTests.cpp`:

```cpp
class TaggingTranslator : public QTranslator {
public:
    QString translate(const char* context, const char* sourceText, const char*, int) const override
    {
        if (qstrcmp(context, "YourContext") != 0) { return {}; }
        return QStringLiteral("[zh]%1").arg(QString::fromUtf8(sourceText));
    }
};
```

Install it, call the code path, and assert the marker count. This is the only thing that catches a
`translate()` call being replaced by `QStringLiteral` -- which compiles and reads fine and is
exactly how this repo previously shipped 65 English-only strings.

## 5. Run the checks

```bash
# both in a contained runspace; the scripts `exit 1`, which takes the host session with it
# when invoked with `&`. See .workbuddy-ai/tools/run-check-in-runspace.ps1 for the wrapper.
scripts/check-localization-coverage.ps1 -SourceRoot src -TranslationFile translations/SongBird_zh_CN.ts
scripts/check-translations-fresh.ps1 -TranslationFile translations/SongBird_zh_CN.ts -CompiledFile translations/compiled/SongBird_zh_CN.qm
```

Read the **scan counts** in the output, not just "passed". Check B's `compared N lupdate-extracted
string(s)` must have moved by exactly the number of strings you added, otherwise lupdate did not
extract yours (usually because it is behind a helper that takes the literal as a parameter -- add
that helper to `$localizingHelpers` in the coverage script).

## Trap list

- Never use an editor that rewrites line endings on the `.ts`.
- Never trust `lupdate`'s "0 unfinished" as coverage -- it cannot see strings that were never
  wrapped, never extracted, or are behind a parameterised helper.
- `Write` prepends a UTF-8 BOM to **new** files; strip it with
  `sed -i '1s/^\xEF\xBB\xBF//' <file>` (a BOM in a `.ts` makes lupdate miss the first entry).
- `.ts` edits do not take effect in a running app until the `.qm` is regenerated **and** the
  executable is rebuilt (the `.qm` is embedded at build time).

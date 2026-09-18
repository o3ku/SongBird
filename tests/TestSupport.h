#pragma once

#include <QString>
#include <QtGlobal>

namespace TestSupport {

// Emoji mark used by startup-checklist status rendering (and its tests).
inline QString emojiMark(ushort codePoint)
{
    return QString(QChar(codePoint));
}

} // namespace TestSupport

// Note: server fixture helpers (makeServer / baseServer / serverOfType /
// createServer) intentionally stay per test file: their field and signature
// shapes differ per test domain (persistence round-trips, service CRUD,
// config writing, protocol compat), and a shared shape would couple unrelated
// tests.

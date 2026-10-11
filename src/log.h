// Logging to the system log (through NickelHook) and to a small file in the
// mod's folder, so problems can be read from the drive without a terminal.
#ifndef KOSYNC_LOG_H
#define KOSYNC_LOG_H

#include <QString>

namespace kosync {

void setVerboseLogging(bool verbose);
bool verboseLogging();
void log(const QString &message);
// Only written when verbose logging is on.
void debug(const QString &message);

}

#endif

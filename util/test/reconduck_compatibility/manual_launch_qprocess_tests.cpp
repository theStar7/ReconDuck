#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <windows.h>
#include "../../../qreconduck/Code/qprocessinfo.h"

static bool CheckProcess(uint32_t pid, uint32_t parent, const QString &path)
{
  for(const QProcessInfo &process : QProcessInfo::enumerate(false))
    if(process.pid() == pid)
      return process.startTime() != 0 && process.parentPid() == parent &&
             QFileInfo(process.executablePath()).canonicalFilePath().compare(
                 QFileInfo(path).canonicalFilePath(), Qt::CaseInsensitive) == 0;
  return false;
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  if(argc > 1)
  {
    Sleep(10000);
    return 0;
  }
  QProcess child;
  child.start(app.applicationFilePath(), QStringList() << QStringLiteral("--child"));
  if(!child.waitForStarted(2000))
    return 1;
  bool found = CheckProcess((uint32_t)child.processId(), GetCurrentProcessId(),
                             app.applicationFilePath());
  child.kill();
  child.waitForFinished(2000);
  return found ? 0 : 2;
}

// Exercise the actual worker with real Qt shared state and isolated capture/registry boundaries.
#include <QAtomicInt>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QSharedPointer>
#include <QString>
#include <QThread>
#include <QVector>
#include <cstdio>
#include "../../../qreconduck/Code/manual_launch_process.h"

struct CaptureDialog
{
  static QString tr(const char *text) { return QCoreApplication::translate("CaptureDialog", text); }
};

// These boundaries are deliberately inert: no target process, DLL, socket or registry is touched.
struct QProcessInfo
{
  static QList<QProcessInfo> enumerate(bool) { return {}; }
  uint32_t pid() const { return 0; }
  uint32_t parentPid() const { return 0; }
  uint64_t startTime() const { return 0; }
  QString executablePath() const { return {}; }
};
using QProcessList = QList<QProcessInfo>;
using rdcstr = QString;
struct ResultDetails
{
  bool success;
  bool OK() const { return success; }
  QString Message() const { return QStringLiteral("Injected startup failure"); }
};
struct CaptureSettings
{
  int options = 0;
  int environment = 0;
};
struct ExecuteResult
{
  ResultDetails result{false};
  uint32_t ident = 0;
};
enum class TargetControlMessageType { RegisterAPI };
struct TargetControlMessage
{
  TargetControlMessageType type = TargetControlMessageType::RegisterAPI;
  struct { bool supported = false, presenting = false; } apiUse;
};
struct ITargetControl
{
  uint32_t GetPID() { return 0; }
  bool Connected() { return false; }
  TargetControlMessage ReceiveMessage() { return {}; }
  void Shutdown() {}
};

struct ManualLaunchState;
static ManualLaunchState *cancelDuringStart = NULL;
static bool active = false, startSuccess = false, retainFailedStart = false, stopSuccess = true;
static unsigned starts = 0, stops = 0;
static bool RECONDUCK_IsGlobalHookActive() { return active; }
static ResultDetails RECONDUCK_StartGlobalHook(const QString &, const QString &, int);
static void RECONDUCK_StopGlobalHook()
{
  stops++;
  if(stopSuccess)
    active = false;
}
static uint32_t RECONDUCK_EnumerateRemoteTargets(const char *, uint32_t) { return 0; }
static ITargetControl *RECONDUCK_CreateTargetControl(const char *, uint32_t, const rdcstr &, bool)
{
  return NULL;
}
static ExecuteResult RECONDUCK_InjectIntoProcess(uint32_t, int, const QString &, int, bool)
{
  return {};
}

// Generated at test time from CaptureDialog.cpp, not a duplicate implementation.
#include "manual_launch_worker.inc"

static ResultDetails RECONDUCK_StartGlobalHook(const QString &, const QString &, int)
{
  starts++;
  active = startSuccess || retainFailedStart;
  if(cancelDuringStart)
    cancelDuringStart->cancelled.storeRelease(1);
  return {startSuccess};
}

static bool Run(const char *name, bool preexisting, bool success, bool residual,
                bool cleanupSuccess, bool cancelledBefore, bool cancelledDuring,
                unsigned expectedStarts, unsigned expectedStops, bool expectedActive,
                bool expectedCleanupFailure, bool expectedError)
{
  active = preexisting;
  startSuccess = success;
  retainFailedStart = residual;
  stopSuccess = cleanupSuccess;
  starts = stops = 0;
  QSharedPointer<ManualLaunchState> state(new ManualLaunchState);
  if(cancelledBefore)
    state->cancelled.storeRelease(1);
  cancelDuringStart = cancelledDuring ? state.data() : NULL;
  RunManualLaunch(state, QStringLiteral("c:/target/game.exe"), QStringLiteral("c:/capture.rdc"),
                  CaptureSettings(), QStringLiteral("tester"));
  bool passed = state->done.loadAcquire() && starts == expectedStarts && stops == expectedStops &&
                active == expectedActive && state->cleanupFailed == expectedCleanupFailure &&
                !state->error.isEmpty() == expectedError;
  std::printf("%s %s (start=%u, stop=%u, active=%d, cleanupFailed=%d)\n",
              passed ? "PASS" : "FAIL", name, starts, stops, int(active), int(state->cleanupFailed));
  return passed;
}

int main(int argc, char **argv)
{
  QCoreApplication application(argc, argv);
  bool passed = true;
  passed &= Run("preexisting-hook-preserved", true, false, false, true, false, false,
                0, 0, true, false, true);
  passed &= Run("cancel-before-start", false, false, false, true, true, false,
                0, 0, false, false, false);
  passed &= Run("failed-start-fully-rolled-back", false, false, false, true, false, false,
                1, 0, false, false, true);
  passed &= Run("failed-start-retries-owned-recovery", false, false, true, true, false, false,
                1, 1, false, false, true);
  passed &= Run("failed-start-retains-cleanup-failure", false, false, true, false, false, false,
                1, 1, true, true, true);
  passed &= Run("cancelled-failed-start-still-cleans", false, false, true, true, false, true,
                1, 1, false, false, true);
  passed &= Run("cancelled-failed-start-shows-cleanup-failure", false, false, true, false, false, true,
                1, 1, true, true, true);
  passed &= Run("cancelled-successful-start-cleans", false, true, false, true, false, true,
                1, 1, false, false, false);
  passed &= Run("cancelled-successful-start-retains-cleanup-failure", false, true, false, false,
                false, true, 1, 1, true, true, true);
  return passed ? 0 : 1;
}

// Run the actual window-close methods without creating visible widgets.
#include <QAtomicInt>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSharedPointer>
#include <QString>
#include <cstdio>

static bool active = false;
static unsigned messages = 0;
static bool RECONDUCK_IsGlobalHookActive() { return active; }
struct RDDialog
{
  static void critical(QObject *, const QString &, const QString &) { messages++; }
};
struct ManualLaunchState
{
  QAtomicInt cancelled = 0;
};
class CaptureDialog : public QObject
{
  Q_OBJECT
public:
  QSharedPointer<ManualLaunchState> m_ManualLaunch;
  QObject *Widget() { return this; }
  bool checkAllowClose();
};
class LiveCapture : public QObject
{
public:
  int unsavedCaptureCount() const { return 0; }
  bool checkAllowClose(int, bool &) { return true; }
  void cleanItems() {}
};
struct IPythonShell
{
  bool CheckUnsavedChanges() { return true; }
};
struct Context
{
  CaptureDialog *capture;
  bool HasCaptureDialog() { return capture != NULL; }
  CaptureDialog *GetCaptureDialog() { return capture; }
  bool HasPythonShell() { return false; }
  IPythonShell *GetPythonShell() { return NULL; }
};
class MainWindow : public QObject
{
public:
  explicit MainWindow(CaptureDialog *capture) : m_Ctx{capture} {}
  Context m_Ctx;
  QList<QPointer<LiveCapture>> m_LiveCaptures;
  unsigned prompted = 0, saved = 0;
  bool PromptCloseCapture() { prompted++; return true; }
  void SaveLayout(int) { saved++; }
  void closeEvent(QCloseEvent *event);
};

#include "manual_launch_close.inc"
#include "manual_launch_close.moc"

int main(int argc, char **argv)
{
  QCoreApplication application(argc, argv);
  CaptureDialog capture;
  MainWindow main(&capture);
  capture.m_ManualLaunch.reset(new ManualLaunchState);
  // StartGlobalHook has not yet published an active session; the worker still owns startup.
  QCloseEvent duringStartup;
  main.closeEvent(&duringStartup);
  bool startupBlocked = !duringStartup.isAccepted() &&
                        capture.m_ManualLaunch->cancelled.loadAcquire() &&
                        main.prompted == 0 && main.saved == 0;
  std::printf("%s application-close-blocked-during-unpublished-startup\n",
              startupBlocked ? "PASS" : "FAIL");

  capture.m_ManualLaunch.clear();
  active = true;
  main.prompted = main.saved = 0;
  QCloseEvent duringRecovery;
  main.closeEvent(&duringRecovery);
  bool recoveryBlocked = !duringRecovery.isAccepted() && messages != 0 && main.saved == 0;
  std::printf("%s application-close-blocked-during-owned-recovery\n",
              recoveryBlocked ? "PASS" : "FAIL");

  active = false;
  main.prompted = main.saved = 0;
  QCloseEvent afterCleanup;
  main.closeEvent(&afterCleanup);
  bool completed = afterCleanup.isAccepted() && main.prompted == 1 && main.saved == 1;
  std::printf("%s application-close-allowed-after-cleanup\n", completed ? "PASS" : "FAIL");
  return startupBlocked && recoveryBlocked && completed ? 0 : 1;
}

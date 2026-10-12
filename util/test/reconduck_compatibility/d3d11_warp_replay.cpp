#include <stdio.h>
#include "api/replay/reconduck_replay.h"

REPLAY_PROGRAM_MARKER();

static void Check(bool ok, const char *message)
{
  if(!ok) { printf("FAIL: %s\n", message); exit(1); }
}

int main(int argc, char **argv)
{
  Check(argc == 2, "expected capture file");
  RECONDUCK_InitialiseReplay(GlobalEnvironment(), {});
  ICaptureFile *file = RECONDUCK_OpenCaptureFile();
  ResultDetails opened = file->OpenFile(argv[1], "rdc", NULL);
  Check(opened.OK(), "open real WARP capture file");
  auto replay = file->OpenCapture(ReplayOptions(), NULL);
  Check(replay.first.OK() && replay.second, "create replay controller");
  IReplayController *controller = replay.second;
  file->Shutdown();
  Check(controller->GetAPIProperties().pipelineType == GraphicsAPI::D3D11, "D3D11 capture API");
  const auto &actions = controller->GetRootActions();
  Check(!actions.empty(), "replayed frame must have actions");
  controller->SetFrameEvent(actions.back().eventId, true);
  bool verified = false;
  for(const auto &texture : controller->GetTextures())
  {
    if(texture.width != 64 || texture.height != 64)
      continue;
    bytebuf data = controller->GetTextureData(texture.resourceId, Subresource());
    if(data.size() < 64 * 64 * 4)
      continue;
    bool colourMatches = true;
    for(size_t i = 0; i < 64 * 64; ++i)
      colourMatches &= data[i * 4] >= 63 && data[i * 4] <= 65 &&
                       data[i * 4 + 1] >= 127 && data[i * 4 + 1] <= 129 &&
                       data[i * 4 + 2] >= 190 && data[i * 4 + 2] <= 192 && data[i * 4 + 3] == 255;
    verified |= colourMatches;
  }
  Check(verified, "replayed backbuffer pixels must match captured clear colour");
  controller->Shutdown();
  RECONDUCK_ShutdownReplay();
  printf("PASS: real D3D11/WARP replay actions and all 4096 backbuffer pixels\n");
  return 0;
}

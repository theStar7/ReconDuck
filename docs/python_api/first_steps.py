if not pyreconduck.IsCaptureLoaded():
    filename = pyreconduck.Extensions().OpenFileName("Choose a capture", "", "*.rdc")

    pyreconduck.LoadCapture(filename, reconduck.ReplayOptions(), filename, False, True)

eid = pyreconduck.CurEvent()

name = pyreconduck.GetEventBrowser().GetEventName(eid)

print(f"Currently we are at EID {eid} named: '{name}'")
if eid > 1:
    prevname = pyreconduck.GetEventBrowser().GetEventName(eid - 1)
    print(f"    > the previous event {eid-1} is named: '{prevname}'")

pipe = pyreconduck.CurPipelineState()

outputs = pipe.GetOutputTargets()

for idx, out in enumerate(outputs):
    if out.resource != reconduck.ResourceId.Null():
        name = pyreconduck.GetResourceName(out.resource)
        print(f"Output {idx} is: {name}")

depth = pipe.GetDepthTarget()

name = pyreconduck.GetResourceName(depth.resource)
print(f"Depth is: {name}")
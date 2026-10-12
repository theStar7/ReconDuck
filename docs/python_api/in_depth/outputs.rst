Replay Outputs
==============

Some of ReconDuck's functionality for analysis can't be easily presented through pure text or data, and is far better to be represented visually. For example the texture overlays or 3D mesh previews.

This is handled through ReconDuck's replay output system.

Creating an output
------------------

ReconDuck creates replay outputs onto a native window or widget, with a 1:1 relationship. Some APIs like D3D12 will take exclusive access to a native window and make it impossible to re-use afterwards, so it is recommended to recreate any widget after you are finished using for a replay output. If you use :meth:`~qreconduck.MiniQtHelper.CreateOutputRenderingWidget` this is automatically handled for you and you don't have to worry about it.

Once you have a window or widget you want to display to, it is necessary to retrieve the :class:`~reconduck.WindowingData` that ReconDuck can use internally to refer to it. if you have used :meth:`~qreconduck.MiniQtHelper.CreateOutputRenderingWidget` then you can use :meth:`~qreconduck.MiniQtHelper.GetWidgetWindowingData` to fetch it directly. Otherwise you will need to use a platform specific function like :func:`~reconduck.CreateWin32WindowingData` or :func:`~reconduck.CreateXCBWindowingData` to create windowing data for a native window.

It is also possible to render to a fixed off-screen fake window. You can use :func:`~reconduck.CreateHeadlessWindowingData` to create a fixed-size window with an internal buffer that can then be queried later using :meth:`~reconduck.ReplayOutput.ReadbackOutputTexture`. This could be used for example to save results to an image file on disk.

With the windowing data and a :class:`~reconduck.ReplayController` you can call :meth:`~reconduck.ReplayController.CreateOutput` to create a :class:`~reconduck.ReplayOutput` of a given type - either texture or mesh rendering. Once created you should manage the :doc:`lifetime <lifetimes>` of the output and only use it on the :ref:`same thread <pythreading>` as the :class:`~reconduck.ReplayController`, and call :meth:`~reconduck.ReplayOutput.Shutdown` when you are finished.

Configuring an output
---------------------

Once created, an output is configured using :meth:`~reconduck.ReplayOutput.SetTextureDisplay` or :meth:`~reconduck.ReplayOutput.SetMeshDisplay` depending on its type. These take a configuration struct which specifies everything needed to display the relevant resources. For a mesh display you will need a camera which can be initialised with :func:`~reconduck.InitCamera` - two camera types are available, flycam (which can double as a look-at camera) and arcball. These either have position + direction, or position + angle + distance respectively.

Rendering an output
-------------------

Outputs are rendered and refreshed by calling :meth:`~reconduck.ReplayOutput.Display`. You should call this function when needed to re-draw - either after changing the configuration or if the native window needs to be redrawn for platform specific reasons. If you are using a widget created with :meth:`~qreconduck.MiniQtHelper.CreateOutputRenderingWidget` this updating automatically handled for you and you only have to manually call :meth:`~reconduck.ReplayOutput.Display` after changing the configuration.

Sub-windows
-----------

For texture outputs, it is common to want to also display small thumbnails and ReconDuck's replay outputs have a system for handling child thumbnails with low overhead. You can call :meth:`~reconduck.ReplayOutput.AddThumbnail` and pass it the :class:`~reconduck.WindowingData` of the window to render onto. This window is then owned until the replay output is shut down. You can manually release all current thumbnails with :meth:`~reconduck.ReplayOutput.ClearThumbnails` without shutting down the main replay output itself.

You can also render a thumbnail and return the raw bytes for quick previews using :meth:`~reconduck.ReplayOutput.DrawThumbnail` which returns the bytes directly.

Another similar helper is the pixel context, which allows you to render a fixed highly-zoomed view of the current texture and location to another window. As with thumbnails you can pass a native window using :meth:`~reconduck.ReplayOutput.SetPixelContext` and this window will then be owned by the replay output until it is shut down. It will be automatically rendered when you call :meth:`~reconduck.ReplayOutput.Display` on the main output, and the location displayed can be updated with :meth:`~reconduck.ReplayOutput.SetPixelContextLocation`.

Extra helpers
-------------

For some replay output types there are extra helpers that are available.

On texture displaying outputs, if available you can query the :ref:`IDs <resourceids>` for internal textures used for displaying the current texture overlay (:meth:`~reconduck.ReplayOutput.GetDebugOverlayTexID`) or output from a custom display shader (:meth:`~reconduck.ReplayOutput.GetCustomShaderTexID`).

These IDs are of internal resources and so should not be cached for long as the texture may be destroyed the next time the configuration or current event is changed, but can be used to obtain the direct contents of the output of those processes.
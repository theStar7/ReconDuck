Example: Show and save a texture
================================

This example demonstrates how to enumerate textures, show one in the texture viewer, and save the texture to disk.

Fetching Texture Metadata
-------------------------

First we iterate through the list of textures (:meth:`~qreconduck.CaptureContext.GetTextures`) and print their dimensions as we go. We keep track of which texture has the largest area.

.. highlight:: python
.. code:: python

    highestArea = 0
    largest = None
    for tex in pyreconduck.GetTextures():
        name = pyreconduck.GetResourceName(tex.resourceId)
        print(f"{name} is {tex.width} x {tex.height}")
        area = tex.width * tex.height
        if area > highestArea:
            highestArea = area
            largest = tex

Opening in Texture Viewer
-------------------------

Once we've found the largest texture, we print its information again as a summary and then show (:meth:`~qreconduck.CaptureContext.ShowTextureViewer`) and ask the :class:`~qreconduck.TextureViewer` to display it as a new locked tab (:meth:`~qreconduck.TextureViewer.ViewTexture`).

.. highlight:: python
.. code:: python

    if largest is not None:
        name = pyreconduck.GetResourceName(largest.resourceId)
        print(f"\n+++ Largest texture is {name}")

        # open largest texture (by area) in texture viewer, and focus
        pyreconduck.ShowTextureViewer()
        pyreconduck.GetTextureViewer().ViewTexture(largest.resourceId,
                                                reconduck.CompType.Typeless,
                                                True)

.. figure:: ../../imgs/Screenshots/CurrentVsLockedTab.png

	An example locked tab that has been opened from the python script.

To go further we will now save this texture to disk in a couple of different formats.

Saving Texture to Disk
----------------------

We will need to obtain the :class:`~reconduck.ReplayController` which controls ReconDuck's underlying analysis.

.. tip::
    Although not shown in this example, with the texture ID you can use :meth:`~reconduck.ReplayController.GetTextureData` to fetch the raw bytes for a given subresource in a texture, for arbitrary processing.

For convenience we will fetch a blocking version (:meth:`~qreconduck.CaptureContext.GetBlockingController`) that stalls the python script and executes the given command. If this code ran in a UI extension that could cause the UI to become unresponsive while the texture is processed and written to disk so this work could be done on a thread instead - see :ref:`pythreading`.

.. highlight:: python
.. code:: python

    controller = pyreconduck.GetBlockingController()

Next so that we know where to save the file, we prompt the user to browse to a filename (:meth:`qreconduck.ExtensionManager.SaveFileName`). We'll replace the extension so trim off any ``.jpg`` we get.

.. highlight:: python
.. code:: python

    filename = pyreconduck.Extensions().SaveFileName(
        "Choose where to save JPG/PNG/DDS texture files", "", "*.jpg"
    )

    filename = filename.replace(".jpg", "")

Saving textures to disk can require a few different configuration options, which is contained in the :class:`~reconduck.TextureSave` configuration structure.

Not all textures map cleanly to normal texture formats and some textures may have multiple mips or array slices. To start with we will specify that when writing a texture format without an alpha channel ReconDuck should blend to a checkerboard pattern (:data:`~reconduck.AlphaMapping.BlendToCheckerboard`). We also choose to save mip 0 if there are multiple mips, and if there are multiple slices save only slice 0. Other options are possible to e.g. lay out all slices in a grid atlas.

.. highlight:: python
.. code:: python

    texsave = reconduck.TextureSave()
    texsave.resourceId = largest.resourceId

    # Blend alpha to a checkerboard pattern for formats without alpha support
    texsave.alpha = reconduck.AlphaMapping.BlendToCheckerboard

    # Most formats can only display a single image per file, so we select the
    # first mip and first slice
    texsave.mip = 0
    texsave.slice.sliceIndex = 0

With that done we can save the texture in both :data:`~reconduck.FileType.JPG` and :data:`~reconduck.FileType.PNG` formats with a call to :meth:`~reconduck.ReplayController.SaveTexture`.

.. highlight:: python
.. code:: python

    texsave.destType = reconduck.FileType.JPG
    controller.SaveTexture(texsave, filename + ".jpg")

    # For formats with an alpha channel, preserve it
    texsave.alpha = reconduck.AlphaMapping.Preserve

    texsave.destType = reconduck.FileType.PNG
    controller.SaveTexture(texsave, filename + ".png")

Finally we will save to :data:`~reconduck.FileType.DDS`, and in this case we now have a texture format that can support mips and array slices. We'll change the configuration to ensure that all mips and all array slices are written to the same file.

.. code:: python
    
    # DDS textures can save multiple mips and array slices, so instead
    # of the default behaviour of saving mip 0 and slice 0, we set -1
    # which saves *all* mips and slices
    texsave.mip = -1
    texsave.slice.sliceIndex = -1

    texsave.destType = reconduck.FileType.DDS
    controller.SaveTexture(texsave, filename + ".dds")

Example Source
--------------

This example can be found under the name "Show and save a texture" in the python scripting window.

.. only:: html and not htmlhelp

    :download:`Download the example script <show_texture.py>`.

.. literalinclude:: show_texture.py

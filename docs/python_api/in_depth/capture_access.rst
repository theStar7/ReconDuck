Capture File Access
===================

ReconDuck captures are stored in ``.rdc`` files, created by ReconDuck when active in an application and a capture is triggered. This file contains all of the necessary data to replay the captured frame, as well as metadata and additional information.

``.rdc`` files are a fairly simple container and can also be managed by scripts to attach additional custom information that is not normally used by the ReconDuck UI.

This is also how replay and analysis is started when not using the ReconDuck UI but driving the replay API from script :doc:`entirely standalone <../python_module>`.

Accessing capture files
-----------------------

Capture files are managed through two main interfaces, :class:`~reconduck.CaptureAccess` as a limited subset but is available over a network connection without access to the file on local disk, and :class:`~reconduck.CaptureFile` which offers more functionality but must be initialised for a locally-accessible file.

.. note::
    For this document we will assume you are accessing a :class:`~reconduck.CaptureFile` but will note which functionality is remotely available through :class:`~reconduck.CaptureAccess`. For more information about ReconDuck's network replay functionality see :doc:`remote_replay`.

To begin with, you can create a :class:`~reconduck.CaptureFile` with :func:`~reconduck.OpenCaptureFile`. This handle is owned by python and must be destroyed when you are finished with :meth:`~reconduck.CaptureFile.Shutdown`.

From there you can open a capture with :meth:`~reconduck.CaptureFile.OpenFile` with a filename. This function accepts an optional progress callback which will be called intermittently during opening.

.. tip::
    A script running in the UI can access the :class:`~reconduck.CaptureAccess` for the currently loaded capture with :meth:`~qreconduck.ReplayManager.GetCaptureAccess`, and if the file is open locally you can access the :class:`~reconduck.CaptureFile` with :meth:`~qreconduck.ReplayManager.GetCaptureFile`, however note that a capture open remotely will return ``None`` in this latter case.

    Both of these interfaces **must** be only accessed on the :ref:`replay thread <pythreading>`.

Opening capture for replay
--------------------------

With a :class:`~reconduck.CaptureFile` you can begin replaying using :meth:`~reconduck.CaptureFile.OpenCapture`, which will attempt to open and replay the capture and return the :class:`~reconduck.ReplayController` for it if successful.

Note that the :class:`~reconduck.CaptureFile` must still stay open until you are finished with the :class:`~reconduck.ReplayController` so you should ensure that you shut down the controller first when done with analysis before closing the capture file.

Capture file formats
--------------------

In the vast majority of cases, files opened this way will be normal ``.rdc`` files and so when :meth:`~reconduck.CaptureFile.OpenFile` is called the filetype should be ``rdc`` or an empty string.

ReconDuck does have support for other formats, although the support for importing is very limited as this requires a format that contains all of the information needed for a ReconDuck capture.

Support for other formats is more useful for exporting with :meth:`~reconduck.CaptureFile.Convert`, which may contain a limited subset of the data. The list of supported formats can be queried via :meth:`~reconduck.CaptureFile.GetCaptureFileFormats` which gives details about which formats are supported, whether they can be imported or exported, etc.

Capture data
------------

Opening a file is a fairly lightweight operation, as it only decodes the container and loads capture metadata. This will not perform any graphics API calls or begin to replay the capture, and it will not load large amounts of data into memory.

From here it is possible to query metadata about which graphics API - referred to as a driver here - is used in the capture, whether it is supported locally for replay, and the thumbnail (:meth:`~reconduck.CaptureFile.GetThumbnail`) for the capture. You can also ask for a machine ident string which will give you information about the platform where the capture was recorded - e.g. x86 or Android, 32-bit or 64-bit. This can be useful in displaying messages to the user in case there is incompatibility.

You can request the :doc:`structured_data` (:class:`~reconduck.SDFile`) for the capture - this is distinct from replaying as ReconDuck will decode the serialised data within the capture, but will not make any graphics API calls. This can be done on any build of ReconDuck that supports the target API. For example on linux it would not be possible to load the structured data for a D3D capture as D3D support is not available at all, but a windows machine could load the structured data for an Android vulkan capture even if it could not replay it.

Requesting structured data will be a more heavyweight operation as this requires reading and decoding the entirety of the capture. The requested :class:`~reconduck.SDFile` will contain buffers as well, and so is compatible with any export format that has :data:`~reconduck.CaptureFileFormat.requiresBuffers` for :meth:`~reconduck.CaptureFile.Convert`.

Capture sections
----------------

The ``rdc`` container file contains a small header and then an arbitrary number of sections. By default this will contain at minimum the section for the frame capture itself, and commonly will also include an extended lossless thumbnail. If callstacks have been captured, there will be a platform-specific section with information about the loaded modules for later resolving.

The known official sections are detailed in :class:`~reconduck.SectionType` with both an enum value and a string path e.g. ``reconduck/internal/framecapture`` for the frame capture as well. Sections can be enumerated and accessed with both :class:`~reconduck.CaptureAccess` or :class:`~reconduck.CaptureFile`, and sections can also be added or written to via these APIs.

Through these APIs you can read and write your own custom sections to add extra data into a ReconDuck capture for your own processing or tracking. Sections can be added or overwritten by calling :meth:`~reconduck.CaptureAccess.WriteSection`, and retrieved with :meth:`~reconduck.CaptureAccess.GetSectionContents`. Sections are described with :class:`~reconduck.SectionProperties` both when being written and when being enumerated for reading.

.. note::
    You should avoid the ``reconduck/`` prefix for any of your custom section names - these can be arbitrary strings so you should use your own namespacing.

ASCII sections
--------------

To aid in better access for raw scripts, it is possible to add sections to ReconDuck captures by concatenating a text file to the end of the ``.rdc``. This then means it is possible to add section data in a very limited fashion without needing to use the ReconDuck python API directly.

.. warning::
    This is an *advanced* feature and care should be taken when doing this. Any errors in the formatting can render the capture file impossible to open! Unless you definitely need to do this you should investigate safer options.

The format of an ASCII section in a ReconDuck capture file is as follows. These lines are commented in this example, but the real format *must not* include extra white space or comments.

.. sourcecode:: text

    A                      # A literal 'A' character, denoting an ASCII section
    119                    # The length in bytes, as a decimal number, of the contents
    4                      # The numeric value of the `SectionType` - usually 0 for custom sections
    1                      # The version of this section, used for backwards compatibility
    reconduck/ui/notes     # The name of the section
                           # There must be a newline after the name of the section

This header can be crafted in a text editor or via any script as it is plain text. As mentioned above, the comments are only for explanation and the actual header *must* only have the data with no trailing white space. Note that the second line with the length in bytes should usually be generated by the script instead of updated manually.

After this header you can put the contents of the section, whatever you would like them to be. In this case we are setting the UI notes section so that we can demonstrate this and show how you could add comments to a capture that can be viewed in the UI. The notes section is formatted as JSON and we want the ``"comments"`` key to be a string to display:

.. sourcecode:: text

    {
            "comments": "These are some notes!

    there isn't really much to put here, except to demonstrate an ASCII section."
    }

If you take the header and remove the comments and white space, and concatenate the header then body onto the end of an existing ``.rdc`` file you will find that the UI can display the comments from the body we have provided here.
API Reference: qreconduck Main Interfaces
=========================================

This is the API reference for the functions, classes, and enums in the ``qreconduck`` module which represents the UI-specific interface for integrating with the UI and writing UI extensions. For more high-level information and instructions on using the python API, see :doc:`../index` and :doc:`../ui_extensions`.

.. contents:: Sections
   :local:

.. module:: qreconduck

Context
-------

.. autoclass:: qreconduck.CaptureContext
  :members:

Replay Manager
--------------

.. autoclass:: qreconduck.ReplayManager
  :members:

RGP Interop Control
-------------------

.. autoclass:: qreconduck.RGPInterop
  :members:


CaptureViewer Interface
------------------------

.. autoclass:: qreconduck.CaptureViewer
  :members:

Helpers
-------

.. autoclass:: qreconduck.BufferInterpreter
  :members:

.. autoclass:: qreconduck.PackingRules
  :members:

.. autoclass:: qreconduck.ParsedBufferFormat
  :members:

.. autoclass:: qreconduck.ParseError
  :members:

Utilities
---------

.. autoclass:: qreconduck.DockReference
  :members:
  :exclude-members: enum_constants__, 

.. autoclass:: qreconduck.CaptureModifications
  :members:
  :exclude-members: enum_constants__, 

.. autoclass:: qreconduck.CaptureSettings
  :members:

.. autoclass:: qreconduck.EventBookmark
  :members:


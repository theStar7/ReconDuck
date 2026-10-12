API Reference: Utilities
========================

This is the API reference for the functions, classes, and enums in the ``reconduck`` module which represents the underlying interface that the UI is built on top of. For more high-level information and instructions on using the python API, see :doc:`../index`.

.. contents:: Sections
   :local:

.. currentmodule:: reconduck

Maths
-----

.. autoclass:: FloatVector
  :members:

.. autofunction:: reconduck.HalfToFloat
.. autofunction:: reconduck.FloatToHalf

Logging & Versioning
--------------------

.. autofunction:: reconduck.LogMessage
.. autofunction:: reconduck.SetDebugLogFile
.. autofunction:: reconduck.GetLogFile
.. autofunction:: reconduck.GetCurrentProcessMemoryUsage
.. autofunction:: reconduck.DumpObject

.. autoclass:: LogType
  :members:


Versioning
----------

.. autofunction:: reconduck.GetVersionString
.. autofunction:: reconduck.GetCommitHash
.. autofunction:: reconduck.IsReleaseBuild

Settings
--------

.. autofunction:: reconduck.GetConfigSetting
.. autofunction:: reconduck.SetConfigSetting
.. autofunction:: reconduck.SaveConfigSettings

Self-hosted captures
--------------------

.. autofunction:: reconduck.CanSelfHostedCapture
.. autofunction:: reconduck.StartSelfHostCapture
.. autofunction:: reconduck.EndSelfHostCapture

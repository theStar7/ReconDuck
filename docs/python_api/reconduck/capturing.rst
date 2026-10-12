API Reference: Capturing
========================

This is the API reference for the functions, classes, and enums in the ``reconduck`` module which represents the underlying interface that the UI is built on top of. For more high-level information and instructions on using the python API, see :doc:`../index`.

.. contents:: Sections
   :local:

.. currentmodule:: reconduck

Execution & Injection
---------------------

.. autofunction:: reconduck.ExecuteAndInject
.. autofunction:: reconduck.InjectIntoProcess

.. autoclass:: reconduck.CaptureOptions
  :members:
  
.. autoclass:: reconduck.EnvironmentModification
  :members:

.. autoclass:: reconduck.EnvMod
  :members:

.. autoclass:: reconduck.EnvSep
  :members:

.. autoclass:: reconduck.ExecuteResult
  :members:

Global Hooking
--------------

.. autofunction:: reconduck.StartGlobalHook
.. autofunction:: reconduck.StopGlobalHook
.. autofunction:: reconduck.IsGlobalHookActive
.. autofunction:: reconduck.CanGlobalHook

Target Control
--------------

.. autofunction:: reconduck.EnumerateRemoteTargets
.. autofunction:: reconduck.CreateTargetControl

.. autoclass:: reconduck.TargetControl
  :members:

.. autoclass:: reconduck.TargetControlMessage
  :members:

.. autoclass:: reconduck.TargetControlMessageType
  :members:

.. autoclass:: reconduck.NewCaptureData
  :members:

.. autoclass:: reconduck.APIUseData
  :members:

.. autoclass:: reconduck.BusyData
  :members:

.. autoclass:: reconduck.NewChildData
  :members:


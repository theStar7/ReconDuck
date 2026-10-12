API Reference: Replay Control
=============================

This is the API reference for the functions, classes, and enums in the ``reconduck`` module which represents the underlying interface that the UI is built on top of. For more high-level information and instructions on using the python API, see :doc:`../index`.

.. contents:: Sections
   :local:

.. currentmodule:: reconduck

Initialisation and Shutdown
---------------------------

.. autofunction:: reconduck.InitialiseReplay
.. autofunction:: reconduck.ShutdownReplay

.. autoclass:: reconduck.GlobalEnvironment
  :members:

.. autoclass:: reconduck.ResultCode
  :members:

.. autoclass:: reconduck.ResultDetails
  :members:

Capture File Access
-------------------

.. autofunction:: reconduck.OpenCaptureFile

.. autoclass:: reconduck.CaptureAccess
  :members:

.. autoclass:: reconduck.CaptureFile
  :members:

.. autoclass:: reconduck.ReplaySupport
  :members:

.. autoclass:: reconduck.CaptureFileFormat
  :members:

.. autoclass:: reconduck.SectionProperties
  :members:

.. autoclass:: reconduck.SectionType
  :members:

.. autoclass:: reconduck.SectionFlags
  :members:

.. autoclass:: reconduck.Thumbnail
  :members:

GPU Enumeration
---------------

.. autoclass:: reconduck.GPUDevice
  :members:

.. autoclass:: reconduck.GPUVendor
  :members:

.. autofunction:: reconduck.GPUVendorFromPCIVendor

.. autoclass:: reconduck.GraphicsAPI
  :members:

.. autofunction:: reconduck.IsD3D

.. autofunction:: reconduck.GetDriverInformation

.. autoclass:: reconduck.DriverInformation
  :members:

Replay Controller
-----------------

.. autoclass:: reconduck.ReplayController
  :members:

.. autoclass:: reconduck.ReplayOptions
  :members:

.. autoclass:: reconduck.ReplayOptimisationLevel
  :members:

.. autoclass:: reconduck.APIProperties
  :members:

Device Protocols
----------------

.. autoclass:: reconduck.DeviceProtocolController
  :members:

.. autofunction:: reconduck.GetSupportedDeviceProtocols
.. autofunction:: reconduck.GetDeviceProtocolController

Remote Servers
--------------

.. autoclass:: reconduck.RemoteServer
  :members:

.. autofunction:: reconduck.CreateRemoteServerConnection
.. autofunction:: reconduck.CheckRemoteServerConnection
.. autofunction:: reconduck.BecomeRemoteServer

.. autoclass:: reconduck.PathEntry
  :members:

.. autoclass:: reconduck.PathProperty
  :members:

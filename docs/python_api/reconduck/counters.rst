API Reference: Performance Counters
===================================

This is the API reference for the functions, classes, and enums in the ``reconduck`` module which represents the underlying interface that the UI is built on top of. For more high-level information and instructions on using the python API, see :doc:`../index`.

.. contents:: Sections
   :local:

.. currentmodule:: reconduck

Counters
--------

.. autoclass:: reconduck.CounterDescription
  :members:

.. autoclass:: reconduck.CounterUnit
  :members:

.. autoclass:: reconduck.Uuid
  :members:

Counter Types
-------------

.. autoclass:: reconduck.GPUCounter
  :members:

.. autofunction:: reconduck.IsAMDCounter
.. autofunction:: reconduck.IsARMCounter
.. autofunction:: reconduck.IsGenericCounter
.. autofunction:: reconduck.IsIntelCounter
.. autofunction:: reconduck.IsNvidiaCounter
.. autofunction:: reconduck.IsVulkanExtendedCounter

Results
-------

.. autoclass:: reconduck.CounterResult
  :members:

.. autoclass:: reconduck.CounterValue
  :members:

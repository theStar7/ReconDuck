API Reference: Structured Data
==============================

This is the API reference for the functions, classes, and enums in the ``reconduck`` module which represents the underlying interface that the UI is built on top of. For more high-level information and instructions on using the python API, see :doc:`../index`.

.. contents:: Sections
   :local:

.. currentmodule:: reconduck

Type information
----------------

.. autoclass:: SDType
  :members:

.. autoclass:: SDBasic
  :members:

.. autoclass:: SDTypeFlags
  :members:

Objects
-------

.. autoclass:: SDObject
  :members:

.. autoclass:: SDObjectData
  :members:

.. autoclass:: SDObjectPODData
  :members:

Chunks
------

.. autoclass:: SDChunk
  :members:

.. autoclass:: SDChunkMetaData
  :members:

.. autoclass:: SDChunkFlags
  :members:

Structured File
---------------

.. autoclass:: SDFile
  :members:

Creation Helper Functions
-------------------------

.. autofunction:: reconduck.makeSDArray
.. autofunction:: reconduck.makeSDBool
.. autofunction:: reconduck.makeSDEnum
.. autofunction:: reconduck.makeSDFloat
.. autofunction:: reconduck.makeSDInt32
.. autofunction:: reconduck.makeSDInt64
.. autofunction:: reconduck.makeSDResourceId
.. autofunction:: reconduck.makeSDString
.. autofunction:: reconduck.makeSDStruct
.. autofunction:: reconduck.makeSDUInt32
.. autofunction:: reconduck.makeSDUInt64

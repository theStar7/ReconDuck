API Reference: Common Pipeline State Abstraction
================================================

This is the API reference for the functions, classes, and enums in the ``reconduck`` module which represents the underlying interface that the UI is built on top of. For more high-level information and instructions on using the python API, see :doc:`../../index`.

.. contents:: Sections
   :local:

.. currentmodule:: reconduck

.. autoclass:: PipeState
  :members:

General
-------

.. autoclass:: reconduck.Offset
  :members:

Vertex Inputs
-------------

.. autoclass:: reconduck.BoundVBuffer
  :members:

.. autoclass:: reconduck.VertexInputAttribute
  :members:

.. autoclass:: reconduck.Topology
  :members:
  
.. autofunction:: reconduck.NumVerticesPerPrimitive
.. autofunction:: reconduck.VertexOffset
.. autofunction:: reconduck.PatchList_Count
.. autofunction:: reconduck.PatchList_Topology
.. autofunction:: reconduck.IsStrip

Shader Resource Bindings
------------------------

.. autoclass:: reconduck.UsedDescriptor
  :members:

.. autoclass:: reconduck.BindType
  :members:

.. autoclass:: reconduck.TextureSwizzle
  :members:

.. autoclass:: reconduck.TextureSwizzle4
  :members:

Samplers
--------

.. autoclass:: reconduck.AddressMode
  :members:

.. autoclass:: reconduck.TextureFilter
  :members:

.. autoclass:: reconduck.FilterMode
  :members:
  
.. autoclass:: reconduck.FilterFunction
  :members:

.. autoclass:: reconduck.ChromaSampleLocation
  :members:

.. autoclass:: reconduck.YcbcrConversion
  :members:

.. autoclass:: reconduck.YcbcrRange
  :members:

Viewport and Scissor
--------------------

.. autoclass:: reconduck.Viewport
  :members:

.. autoclass:: reconduck.Scissor
  :members:

Rasterizer
----------

.. autoclass:: reconduck.CullMode
  :members:

.. autoclass:: reconduck.DepthBiasMode
  :members:

.. autoclass:: reconduck.FillMode
  :members:

.. autoclass:: reconduck.ConservativeRaster
  :members:

.. autoclass:: reconduck.LineRaster
  :members:

.. autoclass:: reconduck.ShadingRateCombiner
  :members:

.. autoclass:: reconduck.RasterState
  :members:


Depth and Stencil
-----------------

.. autoclass:: reconduck.DepthTestState
  :members:

.. autoclass:: reconduck.StencilFace
  :members:

.. autoclass:: reconduck.StencilOperation
  :members:

.. autoclass:: reconduck.CompareFunction
  :members:

Blending
--------

.. autoclass:: reconduck.ColorBlend
  :members:

.. autoclass:: reconduck.BlendEquation
  :members:

.. autoclass:: reconduck.BlendMultiplier
  :members:

.. autoclass:: reconduck.BlendOperation
  :members:

.. autoclass:: reconduck.LogicOperation
  :members:

Shader Messages
---------------

.. autoclass:: reconduck.ShaderMessage
  :members:

.. autoclass:: reconduck.ShaderMessageLocation
  :members:

.. autoclass:: reconduck.ShaderMeshMessageLocation
  :members:

.. autoclass:: reconduck.ShaderVertexMessageLocation
  :members:

.. autoclass:: reconduck.ShaderPixelMessageLocation
  :members:

.. autoclass:: reconduck.ShaderGeometryMessageLocation
  :members:

.. autoclass:: reconduck.ShaderComputeMessageLocation
  :members:

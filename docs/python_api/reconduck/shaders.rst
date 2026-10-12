API Reference: Shaders
======================

This is the API reference for the functions, classes, and enums in the ``reconduck`` module which represents the underlying interface that the UI is built on top of. For more high-level information and instructions on using the python API, see :doc:`../index`.

.. contents:: Sections
   :local:

.. currentmodule:: reconduck

Descriptors
-----------

.. autoclass:: reconduck.Descriptor
  :members:

.. autoclass:: reconduck.SamplerDescriptor
  :members:

.. autoclass:: reconduck.DescriptorFlags
  :members:
   
.. autoclass:: reconduck.DescriptorCategory
  :members:
   
.. autoclass:: reconduck.DescriptorType
  :members:
   
.. autofunction:: reconduck.CategoryForDescriptorType
.. autofunction:: reconduck.IsConstantBlockDescriptor
.. autofunction:: reconduck.IsReadOnlyDescriptor
.. autofunction:: reconduck.IsReadWriteDescriptor
.. autofunction:: reconduck.IsSamplerDescriptor

.. autoclass:: reconduck.DescriptorLogicalLocation
  :members:

.. autoclass:: reconduck.DescriptorRange
  :members:

.. autoclass:: reconduck.DescriptorAccess
  :members:

Reflection
----------

.. autoclass:: reconduck.ShaderReflection
  :members:

.. autoclass:: reconduck.ShaderStage
  :members:

.. autoclass:: reconduck.ShaderStageMask
  :members:

.. autofunction:: reconduck.MaskForStage
.. autofunction:: reconduck.FirstStageForMask

.. autoclass:: reconduck.SigParameter
  :members:

.. autoclass:: reconduck.ShaderBuiltin
  :members:

.. autoclass:: reconduck.ConstantBlock
  :members:

.. autoclass:: reconduck.ShaderSampler
  :members:

.. autoclass:: reconduck.ShaderResource
  :members:

Debug Info
----------

.. autoclass:: reconduck.ShaderDebugInfo
  :members:

.. autoclass:: reconduck.ShaderEncoding
  :members:
  
.. autoclass:: reconduck.KnownShaderTool
  :members:
  
.. autofunction:: reconduck.ToolExecutable
.. autofunction:: reconduck.ToolInput
.. autofunction:: reconduck.ToolOutput

.. autofunction:: reconduck.IsTextRepresentation

.. autoclass:: reconduck.ShaderEntryPoint
  :members:

.. autoclass:: reconduck.ShaderSourceFile
  :members:

.. autoclass:: reconduck.ShaderCompileFlags
  :members:

.. autoclass:: reconduck.ShaderCompileFlag
  :members:

.. autoclass:: reconduck.ShaderSourcePrefix
  :members:

Shader Constants
----------------

.. autoclass:: reconduck.ShaderConstant
  :members:

.. autoclass:: reconduck.ShaderConstantType
  :members:

.. autoclass:: reconduck.ShaderVariableFlags
  :members:

.. autoclass:: reconduck.VarType
  :members:

.. autofunction:: reconduck.VarTypeByteSize
.. autofunction:: reconduck.VarTypeCompType

Shader Debugging
----------------

.. autoclass:: reconduck.ShaderDebugTrace
  :members:

.. autoclass:: reconduck.ShaderDebugger
  :members:

.. autoclass:: reconduck.SourceVariableMapping
  :members:

.. autoclass:: reconduck.DebugVariableReference
  :members:

.. autoclass:: reconduck.DebugVariableType
  :members:

.. autoclass:: reconduck.LineColumnInfo
  :members:

.. autoclass:: reconduck.InstructionSourceInfo
  :members:

.. autoclass:: reconduck.ShaderDebugState
  :members:

.. autoclass:: reconduck.ShaderEvents
  :members:

.. autoclass:: reconduck.ShaderVariableChange
  :members:

Shader Variables
----------------
  
.. autoclass:: reconduck.ShaderVariable
  :members:

.. autoclass:: reconduck.ShaderValue
  :members:

.. autoclass:: reconduck.PointerVal
  :members:

.. autoclass:: reconduck.ShaderBindIndex
  :members:

.. autoclass:: reconduck.ShaderDirectAccess
  :members:

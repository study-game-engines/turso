// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "../../Math/Color.h"
#include "../../Math/Matrix3.h"
#include "../../Math/Matrix3x4.h"
#include "GLConstantBuffer.h"
#include "GLGraphics.h"
#include "GLVertexBuffer.h"

#include <flextGL.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

ConstantBuffer::ConstantBuffer() :
    byteSize(0),
    usage(USAGE_DEFAULT),
    dirty(false)
{
}

ConstantBuffer::~ConstantBuffer()
{
    Release();
}

void ConstantBuffer::Release()
{
    if (graphics)
    {
        for (size_t i = 0; i < MAX_SHADER_STAGES; ++i)
        {
            for (size_t j = 0; j < MAX_CONSTANT_BUFFERS; ++j)
            {
                if (graphics->GetConstantBuffer((ShaderStage)i, j) == this)
                    graphics->SetConstantBuffer((ShaderStage)i, j, 0);
            }
        }
    }

    /// \todo Destroy OpenGL buffer

    shadowData.Reset();
}

bool ConstantBuffer::Define(ResourceUsage usage_, const Vector<Constant>& srcConstants)
{
    return Define(usage_, srcConstants.Size(), srcConstants.Size() ? &srcConstants[0] : nullptr);
}

bool ConstantBuffer::Define(ResourceUsage usage_, size_t numConstants, const Constant* srcConstants)
{
    PROFILE(DefineConstantBuffer);

    Release();

    if (!numConstants)
    {
        LOGERROR("Can not define constant buffer with no constants");
        return false;
    }
    if (usage_ == USAGE_RENDERTARGET)
    {
        LOGERROR("Rendertarget usage is illegal for constant buffers");
        return false;
    }

    constants.Clear();
    byteSize = 0;
    usage = usage_;

    while (numConstants--)
    {
        Constant newConstant;
        newConstant.type = srcConstants->type;
        newConstant.name = srcConstants->name;
        newConstant.numElements = srcConstants->numElements;
        newConstant.elementSize = VertexBuffer::elementSize[newConstant.type];
        // If element crosses 16 byte boundary or is larger than 16 bytes, align to next 16 bytes
        if ((newConstant.elementSize <= 16 && ((byteSize + newConstant.elementSize - 1) >> 4) != (byteSize >> 4)) ||
            (newConstant.elementSize > 16 && (byteSize & 15)))
            byteSize += 16 - (byteSize & 15);
        newConstant.offset = byteSize;
        constants.Push(newConstant);

        byteSize += newConstant.elementSize * newConstant.numElements;
        ++srcConstants;
    }

    // Align the final buffer size to a multiple of 16 bytes
    if (byteSize & 15)
        byteSize += 16 - (byteSize & 15);

    shadowData = new unsigned char[byteSize];

    if (usage != USAGE_IMMUTABLE)
        return Create();
    else
        return true;
}

bool ConstantBuffer::SetConstant(size_t index, void* data, size_t numElements)
{
    if (index >= constants.Size())
        return false;

    const Constant& constant = constants[index];

    if (!numElements || numElements > constant.numElements)
        numElements = constant.numElements;

    memcpy(shadowData.Get() + constant.offset, data, numElements * constant.elementSize);
    dirty = true;
    return true;
}

bool ConstantBuffer::SetConstant(const String& name, void* data, size_t numElements)
{
    return SetConstant(name.CString(), data, numElements);
}

bool ConstantBuffer::SetConstant(const char* name, void* data, size_t numElements)
{
    for (size_t i = 0; i < constants.Size(); ++i)
    {
        if (constants[i].name == name)
            return SetConstant(i, data, numElements);
    }

    return false;
}

bool ConstantBuffer::Apply()
{
    if (usage == USAGE_IMMUTABLE)
    {
        /*
        {
            LOGERROR("Apply can only be called once on an immutable constant buffer");
            return false;
        }
        */
    }

    if (!dirty)
        return true;

    /// \todo Apply to OpenGL buffer

    dirty = false;
    return true;
}

size_t ConstantBuffer::FindConstantIndex(const String& name)
{
    return FindConstantIndex(name.CString());
}

size_t ConstantBuffer::FindConstantIndex(const char* name)
{
    for (size_t i = 0; i < constants.Size(); ++i)
    {
        if (constants[i].name == name)
            return i;
    }

    return NPOS;
}

bool ConstantBuffer::Create(const void* data)
{
    dirty = false;

    if (graphics && graphics->IsInitialized())
    {
        /// \todo Create OpenGL buffer
    }

    return true;
}

}
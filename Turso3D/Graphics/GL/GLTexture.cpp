// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "GLGraphics.h"
#include "GLTexture.h"

#include <flextGL.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

unsigned Texture::glTarget[] = 
{
    GL_TEXTURE_1D,
    GL_TEXTURE_2D,
    GL_TEXTURE_3D,
    GL_TEXTURE_CUBE_MAP
};

unsigned Texture::glInternalFormat[] =
{
    0,
    GL_R8,
    GL_RG8,
    GL_RGBA8,
    GL_ALPHA,
    GL_R16,
    GL_RG16,
    GL_RGBA16,
    GL_R16F,
    GL_RG16F,
    GL_RGBA16F,
    GL_R32F,
    GL_RG32F,
    GL_RGB32F,
    GL_RGBA32F,
    GL_DEPTH_COMPONENT16,
    GL_DEPTH_COMPONENT32,
    GL_DEPTH24_STENCIL8,
    GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
    GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
    GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
    0,
    0,
    0,
    0,
    0
};

unsigned Texture::glFormat[] =
{
    0,
    GL_RED,
    GL_RG,
    GL_RGBA,
    GL_ALPHA,
    GL_RED,
    GL_RG,
    GL_RGBA,
    GL_RED,
    GL_RG,
    GL_RGBA,
    GL_RED,
    GL_RG,
    GL_RGB,
    GL_RGBA,
    GL_DEPTH_COMPONENT,
    GL_DEPTH_COMPONENT,
    GL_DEPTH_STENCIL,
    GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
    GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
    GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
    0,
    0,
    0,
    0,
    0
};

unsigned Texture::glDataType[] = 
{
    0,
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_SHORT,
    GL_UNSIGNED_SHORT,
    GL_UNSIGNED_SHORT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_UNSIGNED_SHORT,
    GL_UNSIGNED_INT,
    GL_UNSIGNED_INT_24_8,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};

Texture::Texture() :
    texture(0),
    type(TEX_2D),
    usage(USAGE_DEFAULT),
    width(0),
    height(0),
    format(FMT_NONE)
{
}

Texture::~Texture()
{
    Release();
}

void Texture::Release()
{
    if (graphics)
    {
        for (size_t i = 0; i < MAX_TEXTURE_UNITS; ++i)
        {
            if (graphics->GetTexture(i) == this)
                graphics->SetTexture(i, 0);
        }

        if (usage == USAGE_RENDERTARGET)
        {
            bool clear = false;

            for (size_t i = 0; i < MAX_RENDERTARGETS; ++i)
            {
                if (graphics->RenderTarget(i) == this)
                {
                    clear = true;
                    break;
                }
            }

            if (!clear && graphics->DepthStencil() == this)
                clear = true;

            if (clear)
                graphics->ResetRenderTargets();
        }
    }

    if (texture)
    {
        glDeleteTextures(1, &texture);
        texture = 0;
    }
}

bool Texture::Define(TextureType type_, ResourceUsage usage_, int width_, int height_, ImageFormat format_, size_t numLevels_, const ImageLevel* initialData)
{
    PROFILE(DefineTexture);

    Release();

    if (type_ != TEX_2D)
    {
        LOGERROR("Only 2D textures supported for now");
        return false;
    }
    if (format_ > FMT_DXT5)
    {
        LOGERROR("ETC1 and PVRTC formats are unsupported");
        return false;
    }

    if (numLevels_ < 1)
        numLevels_ = 1;

    type = type_;
    usage = usage_;

    if (graphics && graphics->IsInitialized())
    {
        glGenTextures(1, &texture);
        if (!texture)
        {
            width = 0;
            height = 0;
            format = FMT_NONE;
            numLevels = 0;

            LOGERROR("Failed to create texture");
            return false;
        }

        // Ensure the texture is bound for creation
        graphics->SetTexture(0, this);

        width = width_;
        height = height_;
        format = format_;
        numLevels = numLevels_;

        // If not compressed and no initial data, create the initial level 0 texture with null data
        if (!IsCompressed() && !initialData)
            glTexImage2D(glTarget[type], 0, glInternalFormat[format], width, height, 0, glFormat[format], glDataType[format], 0);

        if (initialData)
        {
            // Hack for allowing immutable texture to set initial data
            usage = USAGE_DEFAULT;
            for (size_t i = 0; i < numLevels; ++i)
                SetData(i, IntRect(0, 0, Max(width >> i, 1), Max(height >> i, 1)), initialData[i]);
            usage = usage_;
        }

        glTexParameteri(glTarget[type], GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(glTarget[type], GL_TEXTURE_MAX_LEVEL, numLevels - 1);

        LOGDEBUGF("Created texture width %d height %d format %d numLevels %d", width, height, (int)format, numLevels);
    }

    return true;
}

bool Texture::DefineSampler(TextureFilterMode filter, TextureAddressMode u, TextureAddressMode v, TextureAddressMode w, unsigned maxAnisotropy, float minLod, float maxLod, const Color& borderColor)
{
    PROFILE(DefineTextureSampler);

    /// \todo Apply to OpenGL texture

    return true;
}

bool Texture::SetData(size_t level, const IntRect rect, const ImageLevel& data)
{
    PROFILE(UpdateTextureLevel);

    if (texture)
    {
        if (usage == USAGE_IMMUTABLE)
        {
            LOGERROR("Can not update immutable texture");
            return false;
        }
        if (level >= numLevels)
        {
            LOGERROR("Mipmap level to update out of bounds");
            return false;
        }
        
        IntRect levelRect(0, 0, Max(width >> level, 1), Max(height >> level, 1));
        if (levelRect.IsInside(rect) != INSIDE)
        {
            LOGERRORF("Texture update region %s is outside level %s", rect.ToString().CString(), levelRect.ToString().CString());
            return false;
        }

        bool wholeLevel = rect == levelRect;
        if (!IsCompressed())
        {
            if (wholeLevel)
            {
                glTexImage2D(glTarget[type], level, glInternalFormat[format], rect.Width(), rect.Height(), 0, glFormat[format],
                    glDataType[format], data.data);
            }
            else
            {
                glTexSubImage2D(glTarget[type], level, rect.left, rect.top, rect.Width(), rect.Height(), glFormat[format],
                    glDataType[format], data.data);
            }
        }
        else
        {
            if (wholeLevel)
            {
                glCompressedTexImage2D(glTarget[type], level, glInternalFormat[format], rect.Width(), rect.Height(), 0,
                    Image::CalculateDataSize(rect.Width(), rect.Height(), format), data.data);
            }
            else
            {
                glCompressedTexSubImage2D(glTarget[type], level, rect.left, rect.top, rect.Width(), rect.Height(), glFormat[format],
                    Image::CalculateDataSize(rect.Width(), rect.Height(), format), data.data);
            }
        }
    }

    return true;
}

}

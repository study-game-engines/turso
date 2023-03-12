#include "Graphics/FrameBuffer.h"
#include "Graphics/Graphics.h"
#include "Graphics/Texture.h"
#include "Input/Input.h"
#include "IO/Arguments.h"
#include "IO/FileSystem.h"
#include "IO/Log.h"
#include "IO/StringUtils.h"
#include "Math/Math.h"
#include "Math/Random.h"
#include "Renderer/AnimatedModel.h"
#include "Renderer/Animation.h"
#include "Renderer/AnimationState.h"
#include "Renderer/Camera.h"
#include "Renderer/Light.h"
#include "Renderer/Material.h"
#include "Renderer/Model.h"
#include "Renderer/Octree.h"
#include "Renderer/Renderer.h"
#include "Resource/ResourceCache.h"
#include "Renderer/StaticModel.h"
#include "Scene/Scene.h"
#include "Time/Timer.h"
#include "Time/Profiler.h"
#include "Thread/ThreadUtils.h"

#include <SDL.h>
#include <glew.h>
#include <tracy/Tracy.hpp>

std::vector<StaticModel*> rotatingObjects;
std::vector<AnimatedModel*> animatingObjects;

void CreateScene(Scene* scene, int preset)
{
    rotatingObjects.clear();
    animatingObjects.clear();

    ResourceCache* cache = Object::Subsystem<ResourceCache>();

    scene->Clear();
    scene->CreateChild<Octree>();

    SetRandomSeed(1);

    if (preset == 0)
    {
        for (int y = -55; y <= 55; ++y)
        {
            for (int x = -55; x <= 55; ++x)
            {
                StaticModel* object = scene->CreateChild<StaticModel>();
                object->SetStatic(true);
                object->SetPosition(Vector3(10.5f * x, -0.05f, 10.5f * y));
                object->SetScale(Vector3(10.0f, 0.1f, 10.0f));
                object->SetModel(cache->LoadResource<Model>("Box.mdl"));
                object->SetMaterial(cache->LoadResource<Material>("Stone.json"));
            }
        }

        for (unsigned i = 0; i < 10000; ++i)
        {
            StaticModel* object = scene->CreateChild<StaticModel>();
            object->SetStatic(true);
            object->SetPosition(Vector3(Random() * 1000.0f - 500.0f, 0.0f, Random() * 1000.0f - 500.0f));
            object->SetScale(1.5f);
            object->SetModel(cache->LoadResource<Model>("Mushroom.mdl"));
            object->SetMaterial(cache->LoadResource<Material>("Mushroom.json"));
            object->SetCastShadows(true);
            object->SetLodBias(2.0f);
            object->SetMaxDistance(600.0f);
        }

        for (unsigned i = 0; i < 100; ++i)
        {
            Light* light = scene->CreateChild<Light>();
            light->SetStatic(true);
            light->SetLightType(LIGHT_POINT);
            light->SetCastShadows(true);
            Vector3 colorVec = 2.0f * Vector3(Random(), Random(), Random()).Normalized();
            light->SetColor(Color(colorVec.x, colorVec.y, colorVec.z));
            light->SetRange(40.0f);
            light->SetPosition(Vector3(Random() * 1000.0f - 500.0f, 7.0f, Random() * 1000.0f - 500.0f));
            light->SetShadowMapSize(256);
            light->SetShadowMaxDistance(200.0f);
            light->SetMaxDistance(900.0f);
        }
    }
    else if (preset == 1)
    {
        for (int y = -125; y <= 125; ++y)
        {
            for (int x = -125; x <= 125; ++x)
            {
                StaticModel* object = scene->CreateChild<StaticModel>();
                //object->SetStatic(true);
                object->SetPosition(Vector3(x * 0.3f, 0.0f, y * 0.3f));
                object->SetScale(0.25f);
                object->SetModel(cache->LoadResource<Model>("Box.mdl"));
                rotatingObjects.push_back(object);
            }
        }

        Light* light = scene->CreateChild<Light>();
        light->SetLightType(LIGHT_DIRECTIONAL);
        light->SetCastShadows(false);
        light->SetColor(Color(1.0f, 1.0f, 1.0f, 0.5f));
        light->SetRotation(Quaternion(45.0f, 45.0f, 0.0f));
        light->SetShadowMapSize(1024);
        light->SetShadowMaxDistance(100.0f);
    }
    else if (preset == 2)
    {
        {
            StaticModel* object = scene->CreateChild<StaticModel>();
            object->SetStatic(true);
            object->SetPosition(Vector3(0, -0.05f, 0));
            object->SetScale(Vector3(100.0f, 0.1f, 100.0f));
            object->SetModel(cache->LoadResource<Model>("Box.mdl"));
            object->SetMaterial(cache->LoadResource<Material>("Stone.json"));
        }

        for (int i = 0; i < 500; ++i)
        {
            AnimatedModel* object = scene->CreateChild<AnimatedModel>();
            object->SetStatic(true);
            object->SetPosition(Vector3(Random() * 90.0f - 45.0f, 0.0f, Random() * 90.0f - 45.0f));
            object->SetRotation(Quaternion(Random(360.0f), Vector3::UP));
            object->SetModel(cache->LoadResource<Model>("Jack.mdl"));
            object->SetCastShadows(true);
            object->SetMaxDistance(600.0f);
            AnimationState* state = object->AddAnimationState(cache->LoadResource<Animation>("Jack_Walk.ani"));
            state->SetWeight(1.0f);
            state->SetLooped(true);
            animatingObjects.push_back(object);
        }

        Light* light = scene->CreateChild<Light>();
        light->SetLightType(LIGHT_DIRECTIONAL);
        light->SetCastShadows(true);
        light->SetColor(Color(1.0f, 1.0f, 1.0f, 0.5f));
        light->SetRotation(Quaternion(45.0f, 45.0f, 0.0f));
        light->SetShadowMapSize(2048);
        light->SetShadowMaxDistance(100.0f);
    }
}

int ApplicationMain(const std::vector<std::string>& arguments)
{
    bool useThreads = true;

    if (arguments.size() > 1 && arguments[1].find("nothreads") != std::string::npos)
        useThreads = false;

    AutoPtr<WorkQueue> workQueue = new WorkQueue(useThreads ? 0 : 1);

    AutoPtr<Profiler> profiler = new Profiler();
    AutoPtr<Log> log = new Log();
    AutoPtr<ResourceCache> cache = new ResourceCache();
    cache->AddResourceDir(ExecutableDir() + "Data");

    AutoPtr<Graphics> graphics = new Graphics("Turso3D renderer test", IntVector2(1920, 1080));
    if (!graphics->Initialize())
        return 1;

    AutoPtr<Input> input = new Input(graphics->Window());

    AutoPtr<Renderer> renderer = new Renderer();
    renderer->SetupShadowMaps(1024, 2048, FMT_D16);

    AutoPtr<FrameBuffer> viewFbo = new FrameBuffer();
    AutoPtr<FrameBuffer> viewMRTFbo = new FrameBuffer();
    AutoPtr<FrameBuffer> ssaoFbo = new FrameBuffer();
    AutoPtr<Texture> colorBuffer = new Texture();
    AutoPtr<Texture> normalBuffer = new Texture();
    AutoPtr<Texture> depthStencilBuffer = new Texture();
    AutoPtr<Texture> ssaoTexture = new Texture();

    unsigned char noiseData[4 * 4 * 4];
    for (int i = 0; i < 4 * 4; ++i)
    {
        Vector3 noiseVec(Random() * 2.0f - 1.0f, Random() * 2.0f - 1.0f, Random() * 2.0f - 1.0f);
        noiseVec.Normalize();

        noiseData[i * 4 + 0] = (unsigned char)(noiseVec.x * 127.0f + 128.0f);
        noiseData[i * 4 + 1] = (unsigned char)(noiseVec.y * 127.0f + 128.0f);
        noiseData[i * 4 + 2] = (unsigned char)(noiseVec.z * 127.0f + 128.0f);
        noiseData[i * 4 + 3] = 0;
    }

    ImageLevel noiseDataLevel(IntVector2(4, 4), FMT_RGBA8, &noiseData[0]);
    AutoPtr<Texture> noiseTexture = new Texture();
    noiseTexture->Define(TEX_2D, IntVector2(4, 4), FMT_RGBA8, 1, 1, &noiseDataLevel);
    noiseTexture->DefineSampler(FILTER_POINT);

    AutoPtr<Scene> scene = new Scene();
    CreateScene(scene, 0);

    AutoPtr<Camera> camera = new Camera();
    camera->SetPosition(Vector3(0.0f, 20.0f, -75.0f));

    float yaw = 0.0f, pitch = 20.0f;
    HiresTimer frameTimer;
    Timer profilerTimer;
    float dt = 0.0f;
    float angle = 0.0f;
    int shadowMode = 1;
    bool drawSSAO = false;
    bool animate = true;

    std::string profilerOutput;

    while (!input->ShouldExit() && !input->KeyPressed(27))
    {
        ZoneScoped;
        frameTimer.Reset();

        if (profilerTimer.ElapsedMSec() >= 1000)
        {
            profilerOutput = profiler->OutputResults();
            profiler->BeginInterval();
            profilerTimer.Reset();
        }

        profiler->BeginFrame();

        input->Update();

        if (input->KeyPressed(SDLK_1))
        {
            ++shadowMode;
            if (shadowMode > 2)
                shadowMode = 0;
            
            float biasMul = shadowMode > 1 ? 1.25f : 1.0f;
            Material::SetGlobalShaderDefines("", shadowMode > 1 ? "HQSHADOW" : "");
            renderer->SetShadowDepthBiasMul(biasMul, biasMul);
        }
        
        if (input->KeyPressed(SDLK_2))
            drawSSAO = !drawSSAO;
        if (input->KeyPressed(SDLK_3))
            CreateScene(scene, 0);
        if (input->KeyPressed(SDLK_4))
            CreateScene(scene, 1);
        if (input->KeyPressed(SDLK_5))
            CreateScene(scene, 2);
        if (input->KeyPressed(SDLK_f))
            graphics->SetFullscreen(!graphics->IsFullscreen());
        
        IntVector2 mouseMove = input->MouseMove();
        yaw += mouseMove.x * 0.1f;
        pitch += mouseMove.y * 0.1f;
        pitch = Clamp(pitch, -90.0f, 90.0f);
        camera->SetRotation(Quaternion(pitch, yaw, 0.0f));

        float moveSpeed = (input->KeyDown(SDLK_LSHIFT) || input->KeyDown(SDLK_RSHIFT)) ? 50.0f : 5.0f;

        if (input->KeyDown(SDLK_w))
            camera->Translate(Vector3::FORWARD * dt * moveSpeed);
        if (input->KeyDown(SDLK_s))
            camera->Translate(Vector3::BACK * dt * moveSpeed);
        if (input->KeyDown(SDLK_a))
            camera->Translate(Vector3::LEFT * dt * moveSpeed);
        if (input->KeyDown(SDLK_d))
            camera->Translate(Vector3::RIGHT * dt * moveSpeed);

        if (input->KeyPressed(SDLK_SPACE))
            animate = !animate;

        if (animate)
        {
            ZoneScopedN("MoveObjects");

            PROFILE(MoveObjects);
        
            if (rotatingObjects.size())
            {
                angle += 100.0f * dt;
                Quaternion rotQuat(angle, Vector3::ONE);
                for (auto it = rotatingObjects.begin(); it != rotatingObjects.end(); ++it)
                    (*it)->SetRotation(rotQuat);
            }
            else if (animatingObjects.size())
            {
                for (auto it = animatingObjects.begin(); it != animatingObjects.end(); ++it)
                {
                    AnimatedModel* object = *it;
                    AnimationState* state = object->AnimationStates()[0];
                    state->AddTime(dt);
                    object->Translate(Vector3::FORWARD * 2.0f * dt);

                    // Rotate to avoid going outside the plane
                    Vector3 pos = object->Position();
                    if (pos.x < -45.0f || pos.x > 45.0f || pos.z < -45.0f || pos.z > 45.0f)
                        object->Yaw(45.0f * dt);
                }
            }
        }

        int width = graphics->RenderWidth();
        int height = graphics->RenderHeight();

        if (colorBuffer->Width() != width || colorBuffer->Height() != height)
        {
            colorBuffer->Define(TEX_2D, IntVector2(width, height), FMT_RGBA8);
            colorBuffer->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
            depthStencilBuffer->Define(TEX_2D, IntVector2(width, height), FMT_D32);
            depthStencilBuffer->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
            normalBuffer->Define(TEX_2D, IntVector2(width, height), FMT_RGBA8);
            normalBuffer->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
            viewFbo->Define(colorBuffer, depthStencilBuffer);

            std::vector<Texture*> mrt;
            mrt.push_back(colorBuffer.Get());
            mrt.push_back(normalBuffer.Get());
            viewMRTFbo->Define(mrt, depthStencilBuffer);
        }

        if (drawSSAO && (ssaoTexture->Width() != colorBuffer->Width() / 2 || ssaoTexture->Height() != colorBuffer->Height() / 2))
        {
            ssaoTexture->Define(TEX_2D, IntVector2(colorBuffer->Width() / 2, colorBuffer->Height() / 2), FMT_R32F, 1, 1);
            ssaoTexture->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
            ssaoFbo->Define(ssaoTexture, nullptr);
        }

        camera->SetAspectRatio((float)width / (float)height);

        {
            PROFILE(PrepareView);
            renderer->PrepareView(scene, camera, shadowMode > 0);
        }
        
        {
            PROFILE(RenderView);

            renderer->RenderShadowMaps();

            if (drawSSAO)
                viewMRTFbo->Bind();
            else
                viewFbo->Bind();

            graphics->SetViewport(IntRect(0, 0, width, height));
            graphics->Clear(true, true, IntRect::ZERO, Color::BLACK);
            renderer->RenderOpaque();

            if (drawSSAO)
            {
                float farClip = camera->FarClip();
                float nearClip = camera->NearClip();
                Vector3 nearVec, farVec;
                camera->FrustumSize(nearVec, farVec);

                ssaoFbo->Bind();
                graphics->SetViewport(IntRect(0, 0, ssaoTexture->Width(), ssaoTexture->Height()));
                ShaderProgram* program = graphics->SetProgram("Shaders/SSAO.glsl");
                graphics->SetUniform(program, "noiseInvSize", Vector2(ssaoTexture->Width() / 4.0f, ssaoTexture->Height() / 4.0f));
                graphics->SetUniform(program, "screenInvSize", Vector2(1.0f / colorBuffer->Width(), 1.0f / colorBuffer->Height()));
                graphics->SetUniform(program, "frustumSize", Vector4(farVec, (float)height / (float)width));
                graphics->SetUniform(program, "aoParameters", Vector4(0.15f, 1.0f, 0.015f, 0.15f));
                graphics->SetUniform(program, "depthReconstruct", Vector2(farClip / (farClip - nearClip), -nearClip / (farClip - nearClip)));
                depthStencilBuffer->Bind(0);
                normalBuffer->Bind(1);
                noiseTexture->Bind(2);
                graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_ALWAYS, true, false);
                graphics->DrawQuad();
                Texture::Unbind(1);
                Texture::Unbind(2);

                viewFbo->Bind();
                graphics->SetViewport(IntRect(0, 0, width, height));
                program = graphics->SetProgram("Shaders/SSAOBlur.glsl");
                graphics->SetUniform(program, "blurInvSize", Vector2(1.0f / ssaoTexture->Width(), 1.0f / ssaoTexture->Height()));
                ssaoTexture->Bind(0);
                graphics->SetRenderState(BLEND_SUBTRACT, CULL_NONE, CMP_ALWAYS, true, false);
                graphics->DrawQuad();
                Texture::Unbind(0);
            }

            viewFbo->Bind();
            graphics->SetViewport(IntRect(0, 0, width, height));
            renderer->RenderAlpha();

            // Optional debug render of shadowmap
            /*
            {
                PROFILE(RenderShadowDebug);

                Matrix4 quadMatrix = Matrix4::IDENTITY;
                quadMatrix.m00 = 0.5f;
                quadMatrix.m11 = 0.5f;
                quadMatrix.m03 = -0.5f;
                quadMatrix.m13 = -0.5f;

                ShaderProgram* program = graphics->SetProgram("Shaders/Debug.glsl");
                graphics->SetUniform(program, "worldViewProjMatrix", quadMatrix);
                renderer->ShadowMapTexture(0)->Bind(0);
                graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_ALWAYS, true, false);
                graphics->DrawQuad();
                Texture::Unbind(0);
            }
            */

            FrameBuffer::Blit(nullptr, IntRect(0, 0, width, height), viewFbo, IntRect(0, 0, width, height), true, false, FILTER_POINT);
        }

        {
            PROFILE(Present);
            graphics->Present();
        }

        profiler->EndFrame();
        dt = frameTimer.ElapsedUSec() * 0.000001f;

        FrameMark;
    }

    printf("%s", profilerOutput.c_str());

    return 0;
}

int main(int argc, char** argv)
{
    return ApplicationMain(ParseArguments(argc, argv));
}
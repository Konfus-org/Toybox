#include "Tbx/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/Graphics/Rendering.h"
#include "Tbx/Graphics/IRenderer.h"
#include "Tbx/Graphics/Mesh.h"
#include "Tbx/Graphics/RenderProcessor.h"
#include "Tbx/Graphics/Material.h"
#include "Tbx/Events/EventCoordinator.h"
#include "Tbx/Events/RenderEvents.h"
#include "Tbx/TBS/World.h"
#include "Tbx/Plugin API/PluginServer.h"
#include <iostream>

namespace Tbx
{
    std::shared_ptr<IRendererFactoryPlugin> Rendering::_renderFactory = nullptr;
    std::map<UID, std::shared_ptr<IRenderer>> Rendering::_renderers = {};
    UID Rendering::_onWindowCreatedEventId = -1;
    UID Rendering::_onWindowClosedEventId = -1;

    void Rendering::Initialize()
    {
        _onWindowCreatedEventId = EventCoordinator::Subscribe<WindowOpenedEvent>(TBX_BIND_STATIC_FN(Rendering::OnWindowOpened));
        _onWindowClosedEventId = EventCoordinator::Subscribe<WindowClosedEvent>(TBX_BIND_STATIC_FN(Rendering::OnWindowClosed));

        _renderFactory = PluginServer::GetPlugin<IRendererFactoryPlugin>();
    }

    void Rendering::Shutdown()
    {
        EventCoordinator::Unsubscribe<WindowOpenedEvent>(_onWindowCreatedEventId);
        EventCoordinator::Unsubscribe<WindowClosedEvent>(_onWindowClosedEventId);

        _renderFactory = nullptr;
    }

    void Rendering::DrawFrame()
    {
        // TODO: This is testing code!!! Need actual implementation
        FrameBuffer buffer;

        // HACK: Where to get this from?
        char currentDirectory[MAX_PATH];
        DWORD result = GetCurrentDirectoryA(MAX_PATH, currentDirectory);
        TBX_ASSERT(result != 0, "Failed in GetCurrentDirectoryA");
        std::string workingDirectory(currentDirectory);

        std::string texturePath = workingDirectory + "/../Examples/Simple App/Assets/Checkerboard.bmp";
        std::string vertexShaderPath = workingDirectory + "/../Examples/Simple App/Assets/vertex.spv";
        std::string fragmentShaderPath = workingDirectory + "/../Examples/Simple App/Assets/fragment.spv";

        Tbx::Texture testTexture(texturePath);
        Tbx::Shader testShader(vertexShaderPath, fragmentShaderPath);
        Tbx::Material testMaterial;

        testMaterial.SetShader(testShader);
        testMaterial.SetTexture(0, testTexture);

        Tbx::Vector3 cameraPosition(0, 0, -2.5f);
        Tbx::Quaternion cameraRotation = Tbx::Quaternion::FromEuler(0, 0, 0);
        Tbx::Camera camera;
        float fov = 90;
        float aspect = 1;//_viewport.Size.GetAspectRatio();
        float zNear = 0.01f;
        float zFar = 500.0f;
        camera.SetPerspective(fov, aspect, zNear, zFar);
        Tbx::Mat4x4 viewProjectionMatrix = camera.CalculateViewProjectionMatrix(cameraPosition, cameraRotation, camera.GetProjectionMatrix());

        struct VertexUniformBlock {
            Tbx::Mat4x4 viewProjectionMatrix;
        };

        struct FragmentUniformBlock {
            float time;
        };

        // render quad mesh
        {
            auto testMesh = Primitives::Quad;

            buffer.Add({ DrawCommandType::SetMaterial, testMaterial });

#if USE_SHADER_UNIFORM_HACKERY
            Tbx::Mat4x4 worldMatrix = Tbx::Mat4x4::FromPosition(Tbx::Vector3(-0.5f, -0.5f, 0.0f));
            Tbx::Mat4x4 worldViewProjectionMatrix = worldMatrix * viewProjectionMatrix;
            static VertexUniformBlock vertexUniformBlock = {};
            vertexUniformBlock.viewProjectionMatrix = worldViewProjectionMatrix;
            ShaderData vertexShaderData(false, 0, &vertexUniformBlock, sizeof(VertexUniformBlock));
            buffer.Add({ DrawCommandType::UploadMaterialData, vertexShaderData });

            // this isn't being used at the moment...it's just here to show how we would set a fragment uniform block
            static FragmentUniformBlock fragmentUniformBlock = {};
            fragmentUniformBlock.time = 0;
            ShaderData fragmentShaderData(true, 0, &fragmentUniformBlock, sizeof(FragmentUniformBlock));
            buffer.Add({ DrawCommandType::UploadMaterialData, fragmentShaderData });
 #endif

            buffer.Add({ DrawCommandType::DrawMesh, testMesh });
        }

        // render triangle mesh
        {
            auto testMesh = Primitives::Triangle;

            buffer.Add({ DrawCommandType::SetMaterial, testMaterial });

#if USE_SHADER_UNIFORM_HACKERY
            Tbx::Mat4x4 worldMatrix = Tbx::Mat4x4::FromPosition(Tbx::Vector3(0.5f, 0.5f, 0.0f));
            Tbx::Mat4x4 worldViewProjectionMatrix = worldMatrix * viewProjectionMatrix;
            static VertexUniformBlock vertexUniformBlock = {};
            vertexUniformBlock.viewProjectionMatrix = worldViewProjectionMatrix;
            ShaderData vertexShaderData(false, 0, &vertexUniformBlock, sizeof(VertexUniformBlock));
            buffer.Add({ DrawCommandType::UploadMaterialData, vertexShaderData });
 #endif

            buffer.Add({ DrawCommandType::DrawMesh, testMesh });
        }

        auto windows = App::GetInstance()->GetWindows();
        for (const auto& window : windows)
        {
            auto winId = window->GetId();
            if (!_renderers.contains(winId)) continue;

            _renderers[winId]->Flush();
            _renderers[winId]->Draw(buffer);
        }
    }

    std::shared_ptr<IRenderer> Rendering::GetRenderer(UID window)
    {
        if (!_renderers.contains(window))
        {
            TBX_ASSERT(false, "No renderer found for window ID: {0}", window.Value);
            return nullptr;
        }

        return _renderers[window];
    }

    void Rendering::OnWindowOpened(const WindowOpenedEvent& e)
    {
        auto newWinId = e.GetWindowId();
        auto newWindow = App::GetInstance()->GetWindow(newWinId);

        auto newRenderer = _renderFactory->Create(newWindow);

        _renderers[newWinId] = newRenderer;
    }

    void Rendering::OnWindowClosed(const WindowClosedEvent& e)
    {
        auto windowId = e.GetWindowId();
        if (_renderers.contains(windowId))
        {
            // Log or assert the state before erasing
            TBX_ASSERT(_renderers[windowId] != nullptr, "Renderer should not be null");
            _renderers.erase(windowId);
        }
    }

    //void Rendering::OnOpenPlayspacesRequest(OpenPlayspacesRequest& r)
    //{
    //    auto playspaceToOpen = std::vector<std::shared_ptr<Playspace>>();
    //    for (const auto& playspaceId : r.GetBoxesToOpen())
    //    {
    //        playspaceToOpen.push_back(World::GetPlayspace(playspaceId));
    //    }

    //    auto buffer = RenderProcessor::PreProcess(playspaceToOpen);
    //    // TODO: implement
    //    //TBX_ASSERT(request.IsHandled, "Render frame request was not handled. Is a renderer created and listening?");

    //    r.IsHandled = true;
    //}
}
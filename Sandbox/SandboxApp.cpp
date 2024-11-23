#include <Toybox.h>
#include <gl/GL.h>

class SandboxApp : public Toybox::Application::App
{
public:
    SandboxApp() : App("Sandbox") { }
    ~SandboxApp() override = default;

protected:
    void OnOpen() override
    {
        TBX_TRACE("OnOpen called!");
    }

    void OnUpdate() override
    {
        _frameCount++;
        TBX_INFO("Update called!");
        TBX_WARN("Frame: {0}", _frameCount);
        ChangeWindowColorTest();
    }

    void OnClose() override
    {
        TBX_TRACE("OnClose called!");
    }

    void ChangeWindowColorTest()
    {
        glClearColor(_red, _green, _blue, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (_red > 1)
            _red = 0;
        if (_red < 0)
            _red = 1;

        if (_green > 1)
            _green = 0;
        if (_green < 0)
            _green = 1;

        if (_blue > 1)
            _blue = 0;
        if (_blue < 0)
            _blue = 1;

        _red += 0.001f;
        _green += 0.001f;
        _blue += 0.001f;
    }

private:
    int _frameCount = 0;
    float _red = 1;
    float _green = 0.5f;
    float _blue = 0;
};

Toybox::Application::App* Toybox::Application::CreateApp()
{
    return new SandboxApp();
}

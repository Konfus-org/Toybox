#include "Interop.h"
#include "InteropApp.h"
#include <gl/GL.h>

namespace Toybox::Interop
{
	// =============
	// TESTING CODE
	// =============
	float _red = 1;
	float _green = 0.5f;
	float _blue = 0;
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
	// =============
	// TESTING CODE
	// =============

    void InteropApp::OnOpen()
    {
        TBX_TRACE("OnOpen called!");
    }

    void InteropApp::OnUpdate()
    {
		ChangeWindowColorTest();
    }

    void InteropApp::OnClose()
    {
        TBX_TRACE("OnClose called!");
    }
}
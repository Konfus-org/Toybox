#pragma once
#include "Tbx/Math/Size.h"
#include <string>

namespace Tbx
{
    // TODO: create a plugin for this!
    class IImmediateGui
    {
    public:
        virtual ~IImmediateGui() = default;

        /// Starts a new ImGui window. Call EndWindow() after rendering elements.
        /// @param title The title of the window.
        /// @param open Optional pointer to a bool that controls window visibility.
        /// @returns true if the window is open and should be rendered.
        virtual bool BeginWindow(const std::string& title, bool* open = nullptr) = 0;

        /// Ends the current ImGui window. Must be called after BeginWindow().
        virtual void EndWindow() = 0;

        /// Displays static text in the UI.
        /// @param text The string to display.
        virtual void Text(const std::string& text) = 0;

        /// Renders a button.
        /// @param label The text on the button.
        /// @param size The size of the button. Default is ImGui's automatic size.
        /// @returns true if the button was clicked.
        virtual bool Button(const std::string& label, const Size& size = { 0, 0 }) = 0;

        /// Renders a checkbox that toggles a boolean value.
        /// @param label The label displayed next to the checkbox.
        /// @param value Pointer to the boolean value to toggle.
        /// @returns true if the value was changed.
        virtual bool Checkbox(const std::string& label, bool* value) = 0;

        /// Renders a slider to control a float value.
        /// @param label The label for the slider.
        /// @param value Pointer to the float to modify.
        /// @param min Minimum value.
        /// @param max Maximum value.
        /// @returns true if the value was changed.
        virtual bool SliderFloat(const std::string& label, float* value, float min, float max) = 0;

        /// Renders a single-line text input field.
        /// @param label The label for the input field.
        /// @param buffer Character buffer to hold input text.
        /// @param bufferSize Size of the buffer.
        /// @returns true if the text was edited.
        virtual bool InputText(const std::string& label, char* buffer, size_t bufferSize) = 0;

        /// Forces the next widget to render on the same horizontal line.
        virtual void SameLine() = 0;

        /// Draws a horizontal separator line.
        virtual void Separator() = 0;
    };

    using DebugGui = IImmediateGui;
    //using RuntimeGui = IRetainedGui;
}
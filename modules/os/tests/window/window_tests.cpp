#include "pch.h"

#include "tbx/os/window.h"

#include "tbx/messages/commands/window_commands.h"

#include "tbx/tsl/casting.h"



namespace

{

    class FakeWindowDispatcher : public tbx::IMessageDispatcher

    {

      public:

        mutable bool open_called = false;

        mutable bool close_called = false;

        mutable bool apply_called = false;

        mutable tbx::WindowDescription last_applied = {};

        mutable int implementation_storage = 0;



        tbx::Result send(tbx::Message& msg) const override

        {

            tbx::Result result;



            if (auto* open_command = tbx::as<tbx::OpenWindowCommand>(&msg))

            {

                open_called = true;

                open_command->state = tbx::MessageState::Handled;

                tbx::Window::WindowImpl impl = &implementation_storage;

                open_command->payload = impl;

                result.flag_success();

                return result;

            }



            if (auto* apply_command = tbx::as<tbx::ApplyWindowDescriptionCommand>(&msg))

            {

                apply_called = true;

                last_applied = apply_command->description;

                apply_command->state = tbx::MessageState::Handled;

                apply_command->payload = last_applied;

                result.flag_success();

                return result;

            }



            if (auto* close_command = tbx::as<tbx::CloseWindowCommand>(&msg))

            {

                close_called = true;

                close_command->state = tbx::MessageState::Handled;

                result.flag_success();

                return result;

            }



            msg.state = tbx::MessageState::Handled;

            result.flag_success();

            return result;

        }



        tbx::Result post(const tbx::Message&) override

        {

            tbx::Result result;

            result.flag_success();

            return result;

        }

    };

}



namespace tbx::tests::os

{

    TEST(WindowTests, OpensAndClosesThroughDispatcher)

    {

        FakeWindowDispatcher dispatcher;

        WindowDescription description;

        Window window(dispatcher, description, false);



        EXPECT_FALSE(window.is_open());



        window.open();

        EXPECT_TRUE(dispatcher.open_called);

        EXPECT_TRUE(window.is_open());



        window.close();

        EXPECT_TRUE(dispatcher.close_called);

        EXPECT_FALSE(window.is_open());

    }



    TEST(WindowTests, AppliesDescriptionUpdatesFromDispatcher)

    {

        FakeWindowDispatcher dispatcher;

        WindowDescription description;

        description.title = "Original";

        Window window(dispatcher, description, true);



        WindowDescription updated = description;

        updated.title = "Updated Title";

        updated.size.width = 800;



        window.set_description(updated);



        EXPECT_TRUE(dispatcher.apply_called);

        const auto& resolved = window.get_description();

        EXPECT_EQ(resolved.title, updated.title);

        EXPECT_EQ(resolved.size.width, updated.size.width);

    }

}


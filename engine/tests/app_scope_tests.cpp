#include "pch.h"

#include "tbx/application_context.h"
#include "tbx/messages/coordinator.h"
#include "tbx/messages/dispatcher_context.h"

namespace tbx::tests
{
    TEST(app_scope_tracks_global_application_context, sets_and_restores_values)
    {
        EXPECT_EQ(current_application_context(), nullptr);

        MessageCoordinator coordinator;
        ApplicationContext context = {};
        context.dispatcher = &coordinator;

        {
            AppScope scope(context);
            const auto* current = current_application_context();
            EXPECT_EQ(current, &context);
            EXPECT_EQ(current->dispatcher, &coordinator);
        }

        EXPECT_EQ(current_application_context(), nullptr);
    }
}

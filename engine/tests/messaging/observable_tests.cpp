#include "tbx/systems/messaging/message_coordinator.h"
#include "tbx/systems/messaging/observable.h"

namespace tbx::tests::messaging
{
    struct ChildObservableTestSettings
    {
        ChildObservableTestSettings(std::weak_ptr<IMessageDispatcher> dispatcher)
            : value(dispatcher, *this, &ChildObservableTestSettings::value, 1)
        {
        }

        ChildObservableTestSettings()
            : value(*this, &ChildObservableTestSettings::value, 1)
        {
        }

        ChildObservableTestSettings(int initial_value)
            : value(*this, &ChildObservableTestSettings::value, initial_value)
        {
        }

        template <typename TOwner>
        ChildObservableTestSettings(
            std::weak_ptr<IMessageDispatcher> dispatcher,
            Observable<TOwner, ChildObservableTestSettings>& parent_property,
            int initial_value = 1)
            : value(
                  dispatcher,
                  parent_property,
                  *this,
                  &ChildObservableTestSettings::value,
                  initial_value)
        {
        }

        Observable<ChildObservableTestSettings, int> value;
    };

    struct ParentObservableTestSettings
    {
        ParentObservableTestSettings(std::weak_ptr<IMessageDispatcher> dispatcher)
            : child(dispatcher, *this, &ParentObservableTestSettings::child, std::in_place)
        {
        }

        Observable<ParentObservableTestSettings, ChildObservableTestSettings> child;
    };

    TEST(ObservableTests, ParentPropertyChangedEvent_IsSentWhenChildObservableChanges)
    {
        // Arrange
        auto dispatcher = std::make_shared<MessageCoordinator>();
        auto settings = ParentObservableTestSettings(dispatcher);
        auto parent_property_change_count = 0;
        dispatcher->register_handler(
            [&parent_property_change_count](Message& msg)
            {
                if (handle_property_changed<&ParentObservableTestSettings::child>(msg))
                    ++parent_property_change_count;
            });

        // Act
        settings.child->value = 2;

        // Assert
        EXPECT_EQ(parent_property_change_count, 1);
    }

    TEST(ObservableTests, NoDispatcherObservable_DoesNotEmitChangeMessages)
    {
        // Arrange
        auto settings = ChildObservableTestSettings();

        // Act
        settings.value = 2;

        // Assert
        EXPECT_EQ(settings.value.value, 2);
    }
}

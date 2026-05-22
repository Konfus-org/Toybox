#include "tbx/systems/messages/message_coordinator.h"
#include "tbx/systems/messaging/observable.h"

namespace tbx::tests::messaging
{
    struct ChildObservableTestSettings
    {
        ChildObservableTestSettings(IMessageDispatcher& dispatcher)
            : value(dispatcher, *this, &ChildObservableTestSettings::value, 1)
        {
        }

        ChildObservableTestSettings()
            : value(*this, &ChildObservableTestSettings::value, 1)
        {
        }

        explicit ChildObservableTestSettings(int initial_value)
            : value(*this, &ChildObservableTestSettings::value, initial_value)
        {
        }

        template <typename TOwner>
        ChildObservableTestSettings(
            IMessageDispatcher& dispatcher,
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

        ChildObservableTestSettings(const ChildObservableTestSettings& other)
            : ChildObservableTestSettings(other.value.value)
        {
        }

        ChildObservableTestSettings& operator=(const ChildObservableTestSettings& other)
        {
            if (this == &other)
                return *this;

            value = other.value.value;
            return *this;
        }

        Observable<ChildObservableTestSettings, int> value;
    };

    struct ParentObservableTestSettings
    {
        ParentObservableTestSettings(IMessageDispatcher& dispatcher)
            : child(dispatcher, *this, &ParentObservableTestSettings::child, std::in_place)
        {
        }

        Observable<ParentObservableTestSettings, ChildObservableTestSettings> child;
    };

    TEST(ObservableTests, ParentPropertyChangedEvent_IsSentWhenChildObservableChanges)
    {
        // Arrange
        auto dispatcher = MessageCoordinator();
        auto settings = ParentObservableTestSettings(dispatcher);
        auto parent_property_change_count = 0;
        dispatcher.register_handler(
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

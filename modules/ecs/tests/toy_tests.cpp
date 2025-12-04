#include "pch.h"
#include "tbx/ecs/stage.h"
#include "tbx/ecs/toy.h"
#include <any>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace tbx::tests::ecs
{
    class RecordingDispatcher : public IMessageDispatcher
    {
      public:
        template <typename TMessage>
        void SetHandler(const std::function<void(TMessage&)>& handler)
        {
            _handlers[std::type_index(typeid(TMessage))] = [handler](Message& message)
            {
                handler(static_cast<TMessage&>(message));
            };
        }

        Result send(Message& msg) const override
        {
            last_message_type = &typeid(msg);
            auto iterator = _handlers.find(std::type_index(typeid(msg)));
            if (iterator != _handlers.end())
            {
                iterator->second(msg);
            }

            return {};
        }

        Result post(const Message&) override
        {
            return {};
        }

        mutable const std::type_info* last_message_type = nullptr;

      private:
        std::unordered_map<std::type_index, std::function<void(Message&)>> _handlers = {};
    };

    struct TestBlock
    {
        int value = 0;
    };

    TEST(StageTests, GetViewDispatchesRequestUsingDispatcher)
    {
        RecordingDispatcher dispatcher = {};
        const Uuid stage_id = Uuid::generate();
        Stage stage(dispatcher, "Arena", stage_id);

        ToyDescription toy_description = ToyDescription("Player", {}, invalid::uuid, Uuid::generate());
        bool full_view_called = false;
        bool filtered_view_called = false;
        bool validity_checked = false;

        dispatcher.SetHandler<StageViewRequest>([&](StageViewRequest& request)
        {
            EXPECT_EQ(request.stage_id, stage_id);

            if (request.block_type_filter.empty())
            {
                full_view_called = true;
            }
            else
            {
                filtered_view_called = true;
                ASSERT_EQ(request.block_type_filter.size(), 1u);
                EXPECT_EQ(*request.block_type_filter[0], typeid(TestBlock));
            }

            request.result = { toy_description };
        });

        dispatcher.SetHandler<IsToyValidRequest>([&](IsToyValidRequest& request)
        {
            validity_checked = true;
            EXPECT_EQ(request.toy_id, toy_description.id);
            request.result = true;
        });

        std::vector<Toy> filtered_toys = stage.get_view<TestBlock>();
        std::vector<Toy> full_toys = stage.get_full_view();

        ASSERT_TRUE(filtered_view_called);
        ASSERT_TRUE(full_view_called);
        ASSERT_EQ(filtered_toys.size(), 1u);
        ASSERT_EQ(full_toys.size(), 1u);
        EXPECT_EQ(filtered_toys[0].get_id(), toy_description.id);
        EXPECT_EQ(full_toys[0].get_id(), toy_description.id);
        EXPECT_TRUE(filtered_toys[0].is_valid());
        EXPECT_TRUE(full_toys[0].is_valid());
        EXPECT_TRUE(validity_checked);
    }

    TEST(StageTests, AddAndRemoveToyUseDispatcherHandlers)
    {
        RecordingDispatcher dispatcher = {};
        const Uuid stage_id = Uuid::generate();
        Stage stage(dispatcher, "Arena", stage_id);

        bool add_called = false;
        const Uuid created_toy_id = Uuid::generate();
        dispatcher.SetHandler<AddToyToStageRequest>([&](AddToyToStageRequest& request)
        {
            add_called = true;
            EXPECT_EQ(request.stage_id, stage_id);
            EXPECT_EQ(request.toy_name, "NewToy");
            request.result = ToyDescription("NewToy", {}, invalid::uuid, created_toy_id);
        });

        bool remove_called = false;
        dispatcher.SetHandler<RemoveToyFromStageRequest>([&](RemoveToyFromStageRequest& request)
        {
            remove_called = true;
            EXPECT_EQ(request.stage_id, stage_id);
            EXPECT_EQ(request.toy_id, created_toy_id);
        });

        Toy created = stage.add_toy("NewToy");

        ASSERT_TRUE(add_called);
        EXPECT_EQ(created.get_id(), created_toy_id);

        stage.remove_toy(created_toy_id);
        EXPECT_TRUE(remove_called);
    }

    TEST(ToyTests, BlockHelpersUseDispatcherResults)
    {
        RecordingDispatcher dispatcher = {};
        const Uuid toy_id = Uuid::generate();
        Toy toy(dispatcher, ToyDescription("Player", {}, invalid::uuid, toy_id));

        TestBlock stored_block = {};

        bool full_view_called = false;
        bool filtered_view_called = false;
        dispatcher.SetHandler<ToyViewRequest>([&](ToyViewRequest& request)
        {
            if (request.block_type_filter.empty())
            {
                full_view_called = true;
            }
            else
            {
                filtered_view_called = true;
                ASSERT_EQ(request.block_type_filter.size(), 1u);
                EXPECT_EQ(*request.block_type_filter[0], typeid(TestBlock));
            }

            request.result = { stored_block };
        });

        bool add_block_called = false;
        dispatcher.SetHandler<AddBlockToToyRequest>([&](AddBlockToToyRequest& request)
        {
            add_block_called = true;
            EXPECT_EQ(request.toy_id, toy_id);
            ASSERT_EQ(request.block_data.type(), typeid(TestBlock));
            stored_block = std::any_cast<TestBlock>(request.block_data);
            stored_block.value = 42;
            request.result = std::ref(stored_block);
        });

        bool get_block_called = false;
        dispatcher.SetHandler<GetToyBlockRequest>([&](GetToyBlockRequest& request)
        {
            get_block_called = true;
            EXPECT_EQ(request.toy_id, toy_id);
            EXPECT_EQ(request.block_type, typeid(TestBlock));
            request.result = std::ref(stored_block);
        });

        bool remove_block_called = false;
        dispatcher.SetHandler<RemoveBlockFromToyRequest>([&](RemoveBlockFromToyRequest& request)
        {
            remove_block_called = true;
            EXPECT_EQ(request.toy_id, toy_id);
            EXPECT_EQ(request.block_type, typeid(TestBlock));
        });

        TestBlock& added_block = toy.add_block<TestBlock>();
        ASSERT_TRUE(add_block_called);
        EXPECT_EQ(&added_block, &stored_block);
        EXPECT_EQ(added_block.value, 42);

        added_block.value = 64;
        EXPECT_TRUE(toy.has_block<TestBlock>());
        EXPECT_TRUE(get_block_called);

        TestBlock& retrieved_block = toy.get_block<TestBlock>();
        EXPECT_EQ(&retrieved_block, &stored_block);
        EXPECT_EQ(retrieved_block.value, 64);

        std::vector<Block> full_view = toy.get_full_view();
        ASSERT_TRUE(full_view_called);
        ASSERT_EQ(full_view.size(), 1u);
        EXPECT_EQ(std::any_cast<TestBlock>(full_view[0]).value, stored_block.value);

        std::vector<Block> filtered_view = toy.get_view<TestBlock>();
        ASSERT_TRUE(filtered_view_called);
        ASSERT_EQ(filtered_view.size(), 1u);
        EXPECT_EQ(std::any_cast<TestBlock>(filtered_view[0]).value, stored_block.value);

        toy.remove_block<TestBlock>();
        EXPECT_TRUE(remove_block_called);
    }
}

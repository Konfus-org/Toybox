#include "pch.h"

#include "tbx/time/delta_time.h"

#include <chrono>

#include <thread>



namespace tbx::tests::time

{

    TEST(DeltaTimerTests, TickReportsElapsedTime)

    {

        DeltaTimer timer;

        std::this_thread::sleep_for(std::chrono::milliseconds(2));



        DeltaTime delta = timer.tick();



        EXPECT_GT(delta.seconds, 0.0);

        EXPECT_GT(delta.milliseconds, 0.0);

        EXPECT_NEAR(delta.milliseconds, delta.seconds * 1000.0, 0.5);

    }



    TEST(DeltaTimerTests, ResetRestartsElapsedWindow)

    {

        DeltaTimer timer;

        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        timer.tick();



        timer.reset();

        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        DeltaTime delta = timer.tick();



        EXPECT_GT(delta.milliseconds, 0.0);

        EXPECT_LT(delta.milliseconds, 50.0);

    }

}


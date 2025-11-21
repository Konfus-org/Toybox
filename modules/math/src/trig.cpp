#include "tbx/math/trig.h"

#include <glm/glm.hpp>



namespace tbx

{

    float degrees_to_radians(float degrees)

    {

        return glm::radians(degrees);

    }



    float radians_to_degrees(float radians)

    {

        return glm::degrees(radians);

    }



    float cos(float x)

    {

        return glm::cos(x);

    }



    float sin(float x)

    {

        return glm::sin(x);

    }



    float tan(float x)

    {

        return glm::tan(x);

    }



    float acos(float x)

    {

        return glm::acos(x);

    }



    float asin(float x)

    {

        return glm::asin(x);

    }



    float atan(float x)

    {

        return glm::atan(x);

    }

}


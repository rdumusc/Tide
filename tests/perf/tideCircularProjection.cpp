/*********************************************************************/
/* Copyright (c) 2018, EPFL/Blue Brain Project                       */
/*                     Raphael Dumusc <raphael.dumusc@epfl.ch>       */
/* All rights reserved.                                              */
/*                                                                   */
/* Redistribution and use in source and binary forms, with or        */
/* without modification, are permitted provided that the following   */
/* conditions are met:                                               */
/*                                                                   */
/*   1. Redistributions of source code must retain the above         */
/*      copyright notice, this list of conditions and the following  */
/*      disclaimer.                                                  */
/*                                                                   */
/*   2. Redistributions in binary form must reproduce the above      */
/*      copyright notice, this list of conditions and the following  */
/*      disclaimer in the documentation and/or other materials       */
/*      provided with the distribution.                              */
/*                                                                   */
/*    THIS  SOFTWARE IS PROVIDED  BY THE  UNIVERSITY OF  TEXAS AT    */
/*    AUSTIN  ``AS IS''  AND ANY  EXPRESS OR  IMPLIED WARRANTIES,    */
/*    INCLUDING, BUT  NOT LIMITED  TO, THE IMPLIED  WARRANTIES OF    */
/*    MERCHANTABILITY  AND FITNESS FOR  A PARTICULAR  PURPOSE ARE    */
/*    DISCLAIMED.  IN  NO EVENT SHALL THE UNIVERSITY  OF TEXAS AT    */
/*    AUSTIN OR CONTRIBUTORS BE  LIABLE FOR ANY DIRECT, INDIRECT,    */
/*    INCIDENTAL,  SPECIAL, EXEMPLARY,  OR  CONSEQUENTIAL DAMAGES    */
/*    (INCLUDING, BUT  NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE    */
/*    GOODS  OR  SERVICES; LOSS  OF  USE,  DATA,  OR PROFITS;  OR    */
/*    BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON ANY THEORY OF    */
/*    LIABILITY, WHETHER  IN CONTRACT, STRICT  LIABILITY, OR TORT    */
/*    (INCLUDING NEGLIGENCE OR OTHERWISE)  ARISING IN ANY WAY OUT    */
/*    OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF ADVISED  OF  THE    */
/*    POSSIBILITY OF SUCH DAMAGE.                                    */
/*                                                                   */
/* The views and conclusions contained in the software and           */
/* documentation are those of the authors and should not be          */
/* interpreted as representing official policies, either expressed   */
/* or implied, of Ecole polytechnique federale de Lausanne.          */
/*********************************************************************/

#include "CommandLineParser.h"

#include <iostream>
#include <string>

namespace po = boost::program_options;

class ProjectionOptions : public CommandLineParser
{
public:
    ProjectionOptions()
    {
        // clang-format off
        desc.add_options()
            ("screens,s", po::value<size_t>()->default_value(7u),
             "Number of segement along the semi-circle (180 degrees)")
            ("screen-width,w", po::value<size_t>()->default_value(2160u),
             "width of a screen in pixels")
            ("bezel-width,b", po::value<size_t>()->default_value(530u),
             "width of screen overlap in pixels")
            ("radius,r", po::value<double>()->default_value(2.55),
             "radius of the projection surface in meters")
            ("height,h", po::value<double>()->default_value(2.3),
             "height of the projection surface in meters")
            ("details,d", "print detailed information")
        ;
        // clang-format on
    }
    size_t screens() const { return vm["screens"].as<size_t>(); }
    size_t screenWidth() const { return vm["screen-width"].as<size_t>(); }
    size_t bezelWidth() const { return vm["bezel-width"].as<size_t>(); }
    double radius() const { return vm["radius"].as<double>(); }
    double height() const { return vm["height"].as<double>(); }
    bool details() const { return vm.count("details"); }
};

inline double approx(const double value, const double precision = 0.000001)
{
    return std::round(value / precision) * precision;
}

class Projection
{
public:
    Projection(const ProjectionOptions& options)
        : o(options)
    {
    }

    size_t startPosition(const size_t n) const
    {
        return n * (o.screenWidth() - o.bezelWidth());
    }

    size_t endPosition(const size_t n) const
    {
        return (n + 1) * o.screenWidth() - n * o.bezelWidth();
    }

    size_t length() const { return endPosition(o.screens() - 1); }
    double startAngle(const size_t n) const
    {
        return M_PI * startPosition(n) / length();
    }

    double endAngle(const size_t n) const
    {
        return M_PI * endPosition(n) / length();
    }

    double startX(const size_t n) const
    {
        return approx(std::cos(startAngle(n)) * o.radius());
    }

    double endX(const size_t n) const
    {
        return approx(std::cos(endAngle(n)) * o.radius());
    }

    double startZ(const size_t n) const
    {
        return -approx(std::sin(startAngle(n)) * o.radius());
    }

    double endZ(const size_t n) const
    {
        return -approx(std::sin(endAngle(n)) * o.radius());
    }

    void printDetails(const size_t n) const
    {
        std::cout << startPosition(n) << " -> " << endPosition(n);
        std::cout << "       ";
        std::cout << startAngle(n) << " -> " << endAngle(n);
        std::cout << "       ";
        std::cout << startX(n) << ";" << startZ(n);
        std::cout << " -> ";
        std::cout << endX(n) << ";" << endZ(n);
        std::cout << std::endl;
    }

    void printCorners(const size_t n) const
    {
        std::cout << "bottom_left [ ";
        std::cout << endX(n) << " 0.0 " << endZ(n) << " ]";
        std::cout << std::endl;

        std::cout << "bottom_right [ ";
        std::cout << startX(n) << " 0.0 " << startZ(n) << " ]";
        std::cout << std::endl;

        std::cout << "top_left [ ";
        std::cout << endX(n) << " " << o.height() << " " << endZ(n) << " ]";
        std::cout << std::endl;
    }

    void printCornersLeftToRight() const
    {
        for (auto i = 0u; i < o.screens(); ++i)
        {
            std::cout << (i + 1) << ")" << std::endl;
            const auto counterclockwiseIndex = o.screens() - i - 1;
            if (o.details())
                printDetails(counterclockwiseIndex);
            printCorners(counterclockwiseIndex);
        }
    }

private:
    const ProjectionOptions& o;
};

/**
 * Compute circular projection frustums for the OpenDeck.
 */
int main(int argc, char** argv)
{
    COMMAND_LINE_PARSER_CHECK(ProjectionOptions, "tideCircularProjection");

    const auto projection = Projection{commandLine};
    projection.printCornersLeftToRight();

    return EXIT_SUCCESS;
}

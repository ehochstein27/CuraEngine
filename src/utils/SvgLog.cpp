//Copyright (c) 2020 Ultimaker B.V.
//CuraEngine is released under the terms of the AGPLv3 or higher.

#include "SvgLog.h"

namespace cura
{
namespace svglog
{
SVGlog::~SVGlog()
{
    // todo: Make sure all available data is processed
}

void SVGlog::registerSink(const std::string& sink)
{
    auto reg_sink = std::make_pair(sink, std::make_shared<SVGsink>());
    sinks.insert(reg_sink);
}
}
}

//Copyright (c) 2020 Ultimaker B.V.
//CuraEngine is released under the terms of the AGPLv3 or higher.

#include "ExtruderTrain.h"
#include "FffGcodeWriter.h"
#include "InsetOrderOptimizer.h"
#include "LayerPlan.h"
#include "utils/logoutput.h"
#include "WallToolPaths.h"

namespace cura
{

static int findAdjacentEnclosingPoly(const ConstPolygonRef& enclosed_inset, const std::vector<ConstPolygonPointer>& possible_enclosing_polys, const coord_t max_gap)
{
    // given an inset, search a collection of insets for the adjacent enclosing inset
    for (unsigned int enclosing_poly_idx = 0; enclosing_poly_idx < possible_enclosing_polys.size(); ++enclosing_poly_idx)
    {
        const ConstPolygonRef& enclosing = *possible_enclosing_polys[enclosing_poly_idx];
        // as holes don't overlap, if the insets intersect, it is safe to assume that the enclosed inset is inside the enclosing inset
        if (PolygonUtils::polygonsIntersect(enclosing, enclosed_inset) && PolygonUtils::polygonOutlinesAdjacent(enclosed_inset, enclosing, max_gap))
        {
            return enclosing_poly_idx;
        }
    }
    return -1;
}

InsetOrderOptimizer::InsetOrderOptimizer(const FffGcodeWriter& gcode_writer, const SliceDataStorage& storage, LayerPlan& gcode_layer, const SliceMeshStorage& mesh, const int extruder_nr, const PathConfigStorage::MeshPathConfigs& mesh_config, const SliceLayerPart& part, unsigned int layer_nr) :
    gcode_writer(gcode_writer),
    storage(storage),
    gcode_layer(gcode_layer),
    mesh(mesh),
    extruder_nr(extruder_nr),
    mesh_config(mesh_config),
    part(part),
    layer_nr(layer_nr),
    z_seam_config(mesh.settings.get<EZSeamType>("z_seam_type"), mesh.getZSeamHint(), mesh.settings.get<EZSeamCornerPrefType>("z_seam_corner")),
    added_something(false),
    retraction_region_calculated(false)
{
}

short indexedOrder(std::vector<ExtrusionJunction>, std::vector<ExtrusionJunction>)
{
    return 0; // TODO: implement!
}

short optimizedOrder(std::vector<ExtrusionJunction>, std::vector<ExtrusionJunction>)
{
    return 0; // TODO: implement!
}

bool InsetOrderOptimizer::optimize()
{
    if (InsetOrderOptimizer::optimizingInsetsIsWorthwhile(mesh, part))
    {
        const order_comp_t order(std::bind(&InsetOrderOptimizer::optimizedOrder, this, std::placeholders::_1, std::placeholders::_2));
        return processInsetsOrdered(order);
    }
    else
    {
        const order_comp_t order(std::bind(&InsetOrderOptimizer::indexedOrder, this, std::placeholders::_1, std::placeholders::_2));
        return processInsetsOrdered(order);
    }
}

bool InsetOrderOptimizer::processInsetsOrdered(const order_comp_t& ordering)
{
    //Bin the insets in order to print the inset indices together, and to optimize the order of each bin to reduce travels.
    BinJunctions insets = variableWidthPathToBinJunctions(part.wall_toolpaths);

    //If printing the outer inset first, start with the lowest inset.
    //Otherwise start with the highest inset and iterate backwards.
    const bool outer_inset_first = mesh.settings.get<bool>("outer_inset_first");
    size_t start_inset;
    size_t end_inset;
    int direction;
    if(outer_inset_first)
    {
        start_inset = 0;
        end_inset = insets.size();
        direction = 1;
    }
    else
    {
        start_inset = insets.size() - 1;
        end_inset = -1;
        direction = -1;
    }

    // TODO: use supplied ordering functor to calculate ordering

    //Add all of the insets one by one.
    constexpr Ratio flow = 1.0_r;
    const bool retract_before_outer_wall = mesh.settings.get<bool>("travel_retract_before_outer_wall");
    const coord_t wall_0_wipe_dist = mesh.settings.get<coord_t>("wall_0_wipe_dist");
    for(size_t inset = start_inset; inset != end_inset; inset += direction)
    {
        if(insets[inset].empty())
        {
            continue; //Don't switch extruders either, etc.
        }
        added_something = true;
        gcode_writer.setExtruder_addPrime(storage, gcode_layer, extruder_nr);
        gcode_layer.setIsInside(true); //Going to print walls, which are always inside.
        ZSeamConfig z_seam_config(mesh.settings.get<EZSeamType>("z_seam_type"), mesh.getZSeamHint(), mesh.settings.get<EZSeamCornerPrefType>("z_seam_corner"));

        if(inset == 0) //Print using outer wall config.
        {
            gcode_layer.addWalls(insets[inset], mesh, mesh_config.inset0_config, mesh_config.bridge_inset0_config, z_seam_config, wall_0_wipe_dist, flow, retract_before_outer_wall);
        }
        else
        {
            gcode_layer.addWalls(insets[inset], mesh, mesh_config.insetX_config, mesh_config.bridge_insetX_config, z_seam_config, 0, flow, false);
        }
    }
    return added_something;
}

void InsetOrderOptimizer::moveInside()
{
    const coord_t outer_wall_line_width = mesh_config.inset0_config.getLineWidth();
    Point p = gcode_layer.getLastPlannedPositionOrStartingPosition();
    // try to move p inside the outer wall by 1.1 times the outer wall line width
    if (PolygonUtils::moveInside(part.insets[0], p, outer_wall_line_width * 1.1f) != NO_INDEX)
    {
        if (!retraction_region_calculated)
        {
            retraction_region = part.insets[0].offset(-outer_wall_line_width);
            retraction_region_calculated = true;
        }
        // move to p if it is not closer than a line width from the centre line of the outer wall
        if (retraction_region.inside(p))
        {
            gcode_layer.addTravel_simple(p);
            gcode_layer.forceNewPathStart();
        }
        else
        {
            // p is still too close to the centre line of the outer wall so move it again
            // this can occur when the last wall finished at a right angle corner as the first move
            // just moved p along one edge rather than into the part
            if (PolygonUtils::moveInside(part.insets[0], p, outer_wall_line_width * 1.1f) != NO_INDEX)
            {
                // move to p if it is not closer than a line width from the centre line of the outer wall
                if (retraction_region.inside(p))
                {
                    gcode_layer.addTravel_simple(p);
                    gcode_layer.forceNewPathStart();
                }
            }
        }
    }
}

bool InsetOrderOptimizer::optimizingInsetsIsWorthwhile(const SliceMeshStorage& mesh, const SliceLayerPart& part)
{
    if (!mesh.settings.get<bool>("optimize_wall_printing_order"))
    {
        // optimization disabled
        return false;
    }
    if (part.insets.size() == 0)
    {
        // no outlines at all, definitely not worth optimizing
        return false;
    }
    if (part.insets.size() < 2 && part.insets[0].size() < 2)
    {
        // only a single outline and no holes, definitely not worth optimizing
        return false;
    }
    // optimize all other combinations of walls and holes
    return true;
}

BinJunctions InsetOrderOptimizer::variableWidthPathToBinJunctions(const VariableWidthPaths& toolpaths)
{
    BinJunctions insets(toolpaths.size());
    for (const VariableWidthLines& path : toolpaths)
    {
        if (path.empty()) // Don't bother printing these.
        {
            continue;
        }
        const size_t inset_index = path.front().inset_idx;

        // Convert list of extrusion lines to vectors of extrusion junctions, and add those to the binned insets.
        for (const ExtrusionLine& line : path)
        {
            insets[inset_index].emplace_back(line.junctions.begin(), line.junctions.end());
        }
    }
    return insets;
}

}//namespace cura

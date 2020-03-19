#include <geode/basic/assert.h>
#include <geode/basic/logger.h>

#include <geode/mesh/core/tetrahedral_solid.h>
#include <geode/mesh/detail/common.h>
#include <geode/mesh/io/polyhedral_solid_output.h>
#include <geode/mesh/io/tetrahedral_solid_input.h>

int main( int argc, char* argv[] )
{
    using namespace geode;

    try
    {
        OPENGEODE_EXCEPTION(
            argc == 3, "Usage: /path/to/input/mesh /path/to/output/mesh" );
        initialize_mesh_io();
        auto solid = TetrahedralSolid3D::create();
        load_tetrahedral_solid( *solid, argv[1] );
        Logger::info( "Nb tetras: ", solid->nb_polyhedra() );
        save_polyhedral_solid( *solid, argv[2] );

        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
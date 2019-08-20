
set(sources
    "${lib_source_dir}/common.cpp"
    "${lib_source_dir}/msh_input.cpp"
)
set(public_headers
    "${lib_include_dir}/detail/common.h"
)
set(advanced_headers
    "${lib_include_dir}/detail/msh_input.h"
)
target_link_libraries(${target_name}
    PRIVATE
        OpenGeode::basic
        OpenGeode::mesh
        OpenGeode::georepresentation
)

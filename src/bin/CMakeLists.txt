add_geode_binary(
    SOURCE "geode-convert-solid.cpp"
    DEPENDENCIES 
        OpenGeode::basic
        OpenGeode::mesh
        ${PROJECT_NAME}::mesh
)
set(srcfiles
    check.cpp
    code.cpp
    expr.cpp
    lfscc.cpp
    libwriter.cpp
    print_smt2.cpp
    scccode.cpp
    sccwriter.cpp
    trie.cpp
    token.cpp)

flex_target(Lexer lexer.flex  ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp)
add_library (objlib OBJECT ${srcfiles} ${FLEX_Lexer_OUTPUTS})
set_property (TARGET objlib PROPERTY POSITION_INDEPENDENT_CODE ON)

add_executable (lfscc $<TARGET_OBJECTS:objlib> main.cpp)
install (TARGETS lfscc
  DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
)

#add_library (liblfscc SHARED $<TARGET_OBJECTS:objlib>)
#set_target_properties (liblfscc PROPERTIES
#  OUTPUT_NAME lfscc
#  PUBLIC_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/lfscc.h"  
#)
#install (TARGETS liblfscc
#  LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/lib"
#  PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_PREFIX}/include"
#)

add_library (liblfscc STATIC $<TARGET_OBJECTS:objlib>)
set_target_properties (liblfscc PROPERTIES
  OUTPUT_NAME lfscc
  PUBLIC_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/lfscc.h"  
)
install (TARGETS liblfscc
  ARCHIVE DESTINATION "${CMAKE_INSTALL_PREFIX}/lib"
  PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_PREFIX}/include"
)

target_link_libraries(lfscc ${LIBRARIES})


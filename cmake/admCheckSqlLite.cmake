# Outputs:
#   SQLITE3_INCLUDEDIR
#   SQLITE3_LINK_LIBRARIES

MACRO(checkSqlite3)
    IF (NOT SQLITE3_CHECKED)

        MESSAGE(STATUS "Checking for Sqlite3")
        MESSAGE(STATUS "********************")

        PKG_CHECK_MODULES(SQLITE3 REQUIRED sqlite3)
        PRINT_LIBRARY_INFO("Sqlite3" SQLITE3_FOUND "${SQLITE3_INCLUDEDIR}" "${SQLITE3_LDFLAGS}")

#       IF (SQLITE3_FOUND)
#            ADM_CHECK_FUNCTION_EXISTS(sqlite3_close "${SQLITE3_LDFLAGS}" SQLITE3_CLOSE_FUNCTION_FOUND "" -I"${SQLITE3_INCLUDEDIR}")
            #IF (SQLITE3_CLOSE_FUNCTION_FOUND)
#                   SET(SQLITE3_FOUND 1)
            #ELSE()
                #MESSAGE(FATAL_ERROR "Working SQLITE3 library not found")
            #ENDIF (SQLITE3_CLOSE_FUNCTION_FOUND)
#       ENDIF (SQLITE3_FOUND)

        SET(SQLITE3_CHECKED 1)
        MESSAGE("")
    ENDIF (NOT SQLITE3_CHECKED)

ENDMACRO(checkSqlite3)

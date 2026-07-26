# Fetches a model file if it is not already present. Run as a script with
# -DURL=... -DDEST=..., so the download happens at build time with progress
# rather than blocking configuration.

if(EXISTS "${DEST}")
    return()
endif()

get_filename_component(_directory "${DEST}" DIRECTORY)
file(MAKE_DIRECTORY "${_directory}")

message(STATUS "aiopt: fetching ${URL}")
message(STATUS "aiopt: into ${DEST}")

# Downloaded beside the target and renamed, so an interrupted transfer never
# leaves something that looks like a complete model.
file(DOWNLOAD "${URL}" "${DEST}.part" SHOW_PROGRESS STATUS _status)

list(GET _status 0 _code)
if(NOT _code EQUAL 0)
    list(GET _status 1 _message)
    file(REMOVE "${DEST}.part")
    message(FATAL_ERROR "aiopt: could not fetch ${URL}: ${_message}")
endif()

file(RENAME "${DEST}.part" "${DEST}")
message(STATUS "aiopt: model ready")

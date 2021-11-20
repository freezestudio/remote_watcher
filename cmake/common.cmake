
add_compile_definitions(_WINDOWS UNICODE _UNICODE)
add_compile_options(/std:c++latest /wd"4819")
add_link_options(
    /SUBSYSTEM:WINDOWS
    /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'"
)

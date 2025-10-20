cmake_minimum_required(VERSION 3.17)

configure_file(ca-cert.jks . COPYONLY)

add_subdirectory(idp_responses)
add_subdirectory(snowflake_responses)

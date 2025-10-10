cmake_minimum_required(VERSION 3.16)

idf_component_register(SRCS "main.c" "src/uiT.c"
                                        "src/boilerT.c"
                                        "src/interruptsT.c"
                                        "src/controlT.c"
                    INCLUDE_DIRS "." "include")

Per poder utilitzar el main s'ha de modificar el CMakelists, dins de src, i ficar en el parentesis el seguent: REQUIRES canopennode , ha de quedar aixi: idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES canopennode 
                    )

idf_component_register(SRCS "cJSON.c" "User_MQTT.c" "User_NVSuse.c" "User_uartDebug.c" "keep_stamp.c" "User_SNTP.c" "User_protocol_utils.c" "User_WIFI.c" "User_timer.c" "User_IO_init.c" "hello_world_main.c" 
                    INCLUDE_DIRS "./include"
                    EMBED_TXTFILES "certs/servercert.pem"
                                   "certs/prvtkey.pem")

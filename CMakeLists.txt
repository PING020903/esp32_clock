# 遍历当前目录下的所有源文件, 并获取到 SOURCES
file(GLOB SOURCES "./*.c")

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS "./include"
                    EMBED_TXTFILES #"certs/servercert.pem"
                                   #"certs/prvtkey.pem"
                                   "html/select.html"
                                   "html/server.html"
                                   "html/clock.html"
                                   "html/showInfo.html"
                                   "html/WIFI_connect.html"
                                   "html/favicon.ico")

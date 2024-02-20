add_library(mbed-tftpd STATIC)

target_sources(mbed-tftpd
    PRIVATE
        TFTPServer.cpp
        threadTFTPServer.cpp
)

target_include_directories(mbed-tftpd 
    INTERFACE 
    .
)

target_link_libraries(mbed-tftpd
    PUBLIC
        mbed-netsocket
        mbed-rtos-flags
)


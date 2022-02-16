add_executable(rsp_snd
               agc_gtw.cpp
               agc_rsp.cpp
               config.cpp
               file.cpp
               ringbuffer.cpp
               rsp_snd.cpp
               rsp.cpp
               snd.cpp
              )

target_link_libraries(rsp_snd ${SDRPLAY_API_LIBRARIES} ${ALSA_LIBRARIES})

include(GNUInstallDirs)
install(TARGETS rsp_snd)

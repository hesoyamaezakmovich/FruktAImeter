cmake_minimum_required(VERSION 3.16)
project(ru.tk.FruktAImeter VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Conan - указываем путь к файлам из Build Engine
if(EXISTS "/home/mersdk/conan_toolchain.cmake")
    include("/home/mersdk/conan_toolchain.cmake")
endif()
list(APPEND CMAKE_PREFIX_PATH "/home/mersdk")
list(APPEND CMAKE_MODULE_PATH "/home/mersdk")

# Qt5
find_package(Qt5 REQUIRED COMPONENTS Core Qml Quick Concurrent)

# Aurora
find_package(PkgConfig REQUIRED)
pkg_check_modules(AURORAAPP REQUIRED auroraapp)

# OpenCV
find_package(OpenCV REQUIRED)

# OpenCV (из Conan) - все библиотеки
file(GLOB OPENCV_LIB_DIR "/home/mersdk/.conan2/p/b/openc*/p/lib")
set(OPENCV_CORE_LIB "${OPENCV_LIB_DIR}/libopencv_core.a")
set(OPENCV_IMGPROC_LIB "${OPENCV_LIB_DIR}/libopencv_imgproc.a")
set(OPENCV_IMGCODECS_LIB "${OPENCV_LIB_DIR}/libopencv_imgcodecs.a")
set(OPENCV_FLANN_LIB "${OPENCV_LIB_DIR}/libopencv_flann.a")
set(OPENCV_FEATURES2D_LIB "${OPENCV_LIB_DIR}/libopencv_features2d.a")
set(OPENCV_CALIB3D_LIB "${OPENCV_LIB_DIR}/libopencv_calib3d.a")

# Зависимости OpenCV
file(GLOB ZLIB_LIB "/home/mersdk/.conan2/p/b/zlib*/p/lib/libz.a")
file(GLOB OPENJPEG_LIB "/home/mersdk/.conan2/p/b/openj*/p/lib/libopenjp2.a")
file(GLOB QUIRC_LIB "/home/mersdk/.conan2/p/b/quirc*/p/lib/libquirc.a")
file(GLOB LIBDEFLATE_LIB "/home/mersdk/.conan2/p/b/libde*/p/lib/libdeflate.a")
file(GLOB OPENEXR_LIB "/home/mersdk/.conan2/p/b/opene*/p/lib/libOpenEXR*.a")
file(GLOB IMATH_LIB "/home/mersdk/.conan2/p/b/imath*/p/lib/libImath*.a")

# NCNN - нужны ОБА пути
set(NCNN_INCLUDE_DIR "/home/mersdk/ncnn/build/src" CACHE PATH "NCNN include")
set(NCNN_SRC_DIR "/home/mersdk/ncnn/src" CACHE PATH "NCNN source include")
set(NCNN_LIBRARY "/home/mersdk/ncnn/build/src/libncnn.a" CACHE FILEPATH "NCNN library")

# Armadillo (из Conan)
file(GLOB ARMADILLO_INCLUDE_DIRS "/home/mersdk/.conan2/p/b/armad*/p/include")
file(GLOB ARMADILLO_LIBRARIES "/home/mersdk/.conan2/p/b/armad*/p/lib/libarmadillo.a")

# OpenBLAS
file(GLOB OPENBLAS_LIB "/home/mersdk/.conan2/p/b/openb*/p/lib/libopenblas.a")

# Cereal
file(GLOB CEREAL_INCLUDE_DIR "/home/mersdk/.conan2/p/cerea*/p/include")

# Ensmallen (зависимость MLpack)
file(GLOB ENSMALLEN_INCLUDE_DIR "/home/mersdk/.conan2/p/ensma*/p/include")

# MLpack (header-only)
set(MLPACK_INCLUDE_DIR "/home/mersdk/mlpack/src" CACHE PATH "MLpack include")

# Исходники из src/
file(GLOB SOURCES "src/*.cpp")
file(GLOB HEADERS "src/*.h")

# QML ресурсы
set(RESOURCES qml/qml.qrc assets/assets.qrc)

# Исполняемый файл
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${NCNN_INCLUDE_DIR}
    ${NCNN_SRC_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${AURORAAPP_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
    ${MLPACK_INCLUDE_DIR}
    ${ARMADILLO_INCLUDE_DIRS}
    ${CEREAL_INCLUDE_DIR}
    ${ENSMALLEN_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Qml
    Qt5::Quick
    Qt5::Concurrent
    ${AURORAAPP_LIBRARIES}
    ${NCNN_LIBRARY}
    ${ARMADILLO_LIBRARIES}
    ${OPENBLAS_LIB}
    ${OPENCV_IMGCODECS_LIB}
    ${OPENCV_FEATURES2D_LIB}
    ${OPENCV_CALIB3D_LIB}
    ${OPENCV_FLANN_LIB}
    ${OPENCV_IMGPROC_LIB}
    ${OPENCV_CORE_LIB}
    ${QUIRC_LIB}
    ${OPENJPEG_LIB}
    ${LIBDEFLATE_LIB}
    ${OPENEXR_LIB}
    ${IMATH_LIB}
    ${ZLIB_LIB}
    pthread
    m
    dl
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    ARMA_DONT_USE_WRAPPER
)

# Установка
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

install(FILES ru.tk.FruktAImeter.desktop
    DESTINATION share/applications
)

install(FILES ru.tk.FruktAImeter.png
    DESTINATION share/icons/hicolor/86x86/apps
)

install(DIRECTORY qml
    DESTINATION share/${PROJECT_NAME}
)

# Установка
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(FILES ru.tk.FruktAImeter.desktop DESTINATION share/applications)

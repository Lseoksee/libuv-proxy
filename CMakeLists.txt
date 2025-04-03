# 버전 지정
cmake_minimum_required(VERSION 3.5.0)
# 프로젝트 명
set (PROJECT_NAME "uv_proxy")
project(${PROJECT_NAME} VERSION 0.1.0 LANGUAGES C)

# 실행파일 지정
file(GLOB SOURCES "src/*.c" "src/parser/*.c" "src/sha256/*.c")
add_executable(${PROJECT_NAME} ${SOURCES})
add_subdirectory(src)

# 실행파일에 help.txt 파일 포함하기
if($ENV{charset} STREQUAL "UTF-8")
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/res/help.txt
        ${CMAKE_CURRENT_BINARY_DIR}/help.txt
        COPYONLY
    )
elseif($ENV{charset} STREQUAL "EUC-KR")
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/res/help_euc-kr.txt
        ${CMAKE_CURRENT_BINARY_DIR}/help.txt
        COPYONLY
    )
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fexec-charset=EUC-KR")
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES 
    COMPILE_DEFINITIONS "DATA_FILE=\"${CMAKE_CURRENT_BINARY_DIR}/help.txt\""
)

# vcpkg 패키지 경로 설정
# set(CMAKE_PREFIX_PATH "vcpkg_installed/vcpkg/pkgs/libuv_x64-windows/share/libuv")

# 패키기 추가 & 라이브러리 로드 (패키지 설치시 출력 구문 잘 확인할것)
find_package(libuv CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE libuv::uv_a)

target_include_directories(${PROJECT_NAME} PRIVATE src src/parser src/sha256)

#== install 옵션==

# 실행파일
install(TARGETS ${PROJECT_NAME} DESTINATION "$ENV{CMAKE_INSTALL_PREFIX}")

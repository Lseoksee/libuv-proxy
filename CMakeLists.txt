# 버전 지정
cmake_minimum_required(VERSION 3.5.0)
# 프로젝트 명
project(K-WarningBypass VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_COMPILER "gcc")
set(CMAKE_C_STANDARD 23)

# 실행파일 지정
add_executable(K-WarningBypass src/Main.c)
add_subdirectory(src)

# vcpkg 패키지 경로 설정
# set(CMAKE_PREFIX_PATH "vcpkg_installed/vcpkg/pkgs/libuv_x64-windows/share/libuv")

# 패키기 추가 & 라이브러리 로드 (패키지 설치시 출력 구문 잘 확인할것)
find_package(libuv CONFIG REQUIRED)
target_link_libraries(K-WarningBypass PRIVATE libuv::uv_a)

# picohttpparser. c & .h를 라이브러리로 추가
target_include_directories(K-WarningBypass PRIVATE include)
add_library(ParseHttp STATIC lib/ParseHttp.c)
target_link_libraries(K-WarningBypass PRIVATE ParseHttp)
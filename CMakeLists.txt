
# 버전 지정
cmake_minimum_required(VERSION 3.5.0)
# 프로젝트 명
project(K-WarningBypass VERSION 0.1.0 LANGUAGES C)

# 실행파일 지정
add_executable(K-WarningBypass src/Main.c)

add_subdirectory(src)
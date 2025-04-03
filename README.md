# libuv-proxy

libuv로 간단한 크로스 플렛폼 HTTP 프록시 서버 구현

> 원래는 SNI 암호화 기능을 추가 하려고 했는데 실패함...


## 사용법

### 서버구성

1. **OS에 맞는 최신 릴리스 다운로드**

2. **프록시 서버 실행 (기본 포트: 1503)**

    ```bash
    uv_proxy
    ```

#### 옵션
```
사용법: uv_proxy [옵션...]

옵션:
[--port, -p <port>]: 프록시 서버에 포트번호를 지정합니다. (default: 1503)
[--dns <DNS 주소1,DNS 주소2>]: DNS 서버를 변경합니다. (default: 기본 DNS 주소)
 > ex) --dns 1.1.1.1,8.8.8.8  또는 --dns 1.1.1.1 

[--help, -h]: 해당 화면을 출력합니다.
```

### 클라이언트 접속

프록시 서버 주소는 `http://<도메인|ip주소>` 이런식으로 해야 함 

#### 윈도우
1. 설정 -> 네트워크 및 이더넷 -> 프록시 -> 수동 프록시 설정 -> **프록시 서버 사용 체크**

2. 주소, 포트를 적절히 설정

#### 안드로이드

1. Wi-Fi 설정 -> 현재 접속된 Wi-Fi에 대한 설정 -> **프록시를 사용안함 에서 수동 으로 변경**

2. 주소, 포트를 적절히 설정

## 직접 빌드하여 설치

### 윈도우

1. **Cmake & vcpkg 설치**
    
    Visual Studio가 있다면 Visual Studio Installer에서 설치하는것을 추천

2. **(Visual Studio Installer에서 설치 한 경우) 환경 변수 설정**

    ```
    cmake: C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin
    vcpkg: C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg
    ```

3. **프로젝트 폴더로 이동**

4. **`CMakePresets.json` 파일을 열어서 아래 부분을 수정**

    > 아래 경로에 파일이 제대로 존재하는지 확인해서 수정할 것

    ```bash
    ...
    "VCPKG_TOOLCHAIN_WINDOWS": "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake",
    ...
    ```

5. **프로젝트 폴더에서 명령프롬프트 열기**

6. **프로젝트 빌드**

    > 프리셋은 `CMakePresets.json` 에서 확인

    ```bash
    # 빌드 환경 셋팅
    cmake --preset <프리셋>
    # 빌드
    cmake --build --preset <프리셋> 
    ```

7. **최종 실행파일 생성**

    > 실행파일은 ./dist/bin 폴더에 생성됨

    ```bash
    cmake --install build/<프리셋>
    ```

### 리눅스 (우분투)

1. **Cmake & GCC & G++ & pkg-config 설치**
    
    ```bash
    sudo apt install cmake gcc g++ pkg-config
    ```

2. **vcpkg 설치**
    
   - 설치하고 싶은 폴더로 이동

    - vcpkg 프로젝트 clone

        ```bash
        git clone https://github.com/microsoft/vcpkg.git
        ```

    - clone 받은 vcpkg 폴더로 이동해서 vcpkg 설치 진행

        ```bash
        ./bootstrap-vcpkg.sh
        vcpkg integrate install
        ```

        설치되면 이런 메지시가 뜰텐데 해당 경로를 잘 기억해야함

        ```
        DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake
        ```

    - 환경 변수 등록
    
        ```bash
        echo export PATH="$PATH:<vcpkg 경로>" >> ~/.bashrc
        source ~/.bashrc
        ```

3.  **프로젝트 폴더로 이동**

4. **`CMakePresets.json` 파일을 열어서 아래 부분을 수정**

    ```bash
    ...
    "VCPKG_TOOLCHAIN_LINUX": <vcpkg 설치과정준 기억해둔 그 경로>
    ...
    ```


5. **프로젝트 빌드**

    > 프리셋은 `CMakePresets.json` 에서 확인

    ```bash
    # 빌드 환경 셋팅
    cmake --preset <프리셋>
    # 빌드
    cmake --build --preset <프리셋> 
    ```

6. **최종 실행파일 생성**

    > 실행파일은 /usr/bin 폴더에 생성됨

    ```bash
    sudo cmake --install build/<프리셋>
    ```
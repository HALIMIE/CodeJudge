# C를 활용한 채점 서버 만들어보기

TCP 통신으로 파일을 전송하고, 파일을 받아 컴파일한 뒤 실행하여 결과를 클라이언트에게 보내준다.

## make 설정

### make configure

build 폴더를 만들고, 루트 디렉토리의 CMakeLists.txt를 실행한다.

### make clean

build 폴더를 완전히 삭제한다. 다음 make configure에 해당하는 동작을 수행한다.

### make

make configure 이후 실행할 수 있으며, 프로젝트 전체를 빌드한다.

## 사용 방법

```build/src/main``` 을 실행하면 TCP 서버가 49999 포트에서 열린다.

```build/src/client_test <ip> <port> <file>``` 을 실행하면 서버와 TCP 통신을 수립한 후, 파일을 전송하고 채점 결과를 전송받는다.

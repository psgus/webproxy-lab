/*
 * csapp.h 안에는
 * - MAXLINE 같은 상수
 * - Getnameinfo, Freeaddrinfo 같은 CS:APP wrapper 함수
 * - getaddrinfo, struct addrinfo 등에 필요한 시스템 헤더
 * 가 함께 들어 있다.
 */
#include "csapp.h"

int main(int argc, char **argv)
{
    /*
     * hints  : getaddrinfo에 "어떤 주소를 원한다"라고 알려주는 조건표
     * listp  : getaddrinfo가 돌려준 주소 후보 리스트의 시작점
     * p      : 그 주소 후보 리스트를 한 칸씩 따라가며 볼 때 쓰는 포인터
     */
    struct addrinfo hints, *listp, *p;

    /* 사람이 읽을 수 있는 IP 주소 문자열을 잠깐 담아둘 버퍼 */
    char buf[MAXLINE];

    /* getaddrinfo 호출 결과(성공/실패 코드)를 저장할 변수 */
    int rc;

    /* Getnameinfo에 넘길 출력 옵션 */
    int flags;

    /*
     * 이 프로그램은 인자를 정확히 1개만 더 받아야 한다.
     *
     * 예:
     *   ./addrinfo_demo localhost
     *
     * 여기서 argc는
     *   argv[0] = 프로그램 이름
     *   argv[1] = 사용자가 입력한 도메인 이름
     * 까지 포함해서 총 2가 된다.
     */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <domain name>\n", argv[0]);
        exit(0);
    }

    /*
     * hints 구조체를 먼저 전부 0으로 초기화한다.
     * 이렇게 해야 쓰지 않는 필드에 쓰레기값이 남지 않는다.
     */
    memset(&hints, 0, sizeof(struct addrinfo));

    /* IPv4 주소만 찾겠다는 뜻 (예: 127.0.0.1) */
    hints.ai_family = AF_INET;

    /* TCP용 주소만 찾겠다는 뜻 */
    hints.ai_socktype = SOCK_STREAM;

    /*
     * argv[1]에 들어 있는 도메인 이름(예: localhost)을
     * 실제 소켓에서 쓸 수 있는 주소 후보 리스트로 변환한다.
     *
     * 결과는 listp가 가리키는 연결 리스트 형태로 돌아온다.
     * 실패하면 rc에 에러 코드가 들어간다.
     */
    if ((rc = getaddrinfo(argv[1], NULL, &hints, &listp)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rc));
        exit(1);
    }

    /*
     * 출력할 때 localhost 같은 이름이 아니라
     * 127.0.0.1 같은 숫자 IP 문자열로 보이게 하는 옵션이다.
     */
    flags = NI_NUMERICHOST;

    /*
     * listp에서 시작해서 주소 후보 리스트를 끝까지 순회한다.
     * p = p->ai_next 는 "다음 후보로 이동"이라는 뜻이다.
     */
    for (p = listp; p != NULL; p = p->ai_next) {
        /*
         * 현재 후보 주소(p->ai_addr)를 사람이 읽을 수 있는 문자열로 바꿔
         * buf에 저장한다.
         */
        Getnameinfo(p->ai_addr, p->ai_addrlen, buf, MAXLINE, NULL, 0, flags);

        /* 변환된 IP 주소 문자열을 한 줄 출력한다 */
        printf("%s\n", buf);
    }

    /*
     * getaddrinfo가 내부적으로 만들어 준 주소 리스트 메모리를 정리한다.
     * 이걸 하지 않으면 메모리 누수가 생긴다.
     */
    Freeaddrinfo(listp);

    /* 프로그램이 정상적으로 끝났다는 뜻 */
    return 0;
}

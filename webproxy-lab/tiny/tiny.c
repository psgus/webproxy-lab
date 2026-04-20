/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"
#include "strings.h"

typedef enum
{
  HTTP_METHOD_GET,
  HTTP_METHOD_HEAD,
  HTTP_METHOD_POST,
  HTTP_METHOD_UNSUPPORTED
} http_method_t;

typedef struct
{
  http_method_t method;
  int send_body;
  int content_length;
  char content_type[MAXLINE];
  char body[MAXBUF];
  char method_str[MAXLINE];
  char uri[MAXLINE];
  char version[MAXLINE];
} http_request_t;

void doit(int fd);
void read_requesthdrs(rio_t *rp, http_request_t *req);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, const http_request_t *req);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, const http_request_t *req);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);
void sigchld_handler(int sig);
http_method_t parse_method(const char *method_str);
int method_should_send_body(http_method_t method);

http_method_t parse_method(const char *method_str)
{
  if (!strcasecmp(method_str, "GET"))
    return HTTP_METHOD_GET;
  if (!strcasecmp(method_str, "HEAD"))
    return HTTP_METHOD_HEAD;
  if (!strcasecmp(method_str, "POST"))
    return HTTP_METHOD_POST;
  return HTTP_METHOD_UNSUPPORTED;
}
int method_should_send_body(http_method_t method)
{
  return method != HTTP_METHOD_HEAD;
}

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  Signal(SIGPIPE, SIG_IGN);
  Signal(SIGCHLD, sigchld_handler);
  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit
    Close(connfd); // line:netp:tiny:close
  }
}

void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;
  http_request_t req;

  /* 연결된 소켓 fd를 Rio 읽기 버퍼와 연결한다. */
  Rio_readinitb(&rio, fd);

  /* 요청의 첫 줄(Request-Line)을 읽는다. 예: GET /index.html HTTP/1.0 */
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);

  /* Request-Line에서 method, URI, HTTP version을 분리한다. */
  sscanf(buf, "%s %s %s", req.method_str, req.uri, req.version);

  req.method = parse_method(req.method_str);
  req.send_body = method_should_send_body(req.method);

  if (req.method == HTTP_METHOD_UNSUPPORTED)
  {
    /**
     * (Not Implemented)는 웹 서버가 클라이언트(브라우저)의 요청을 처리하는 데 필요한 기능이나 메서드(POST, GET 등)를 지원하지 않을 때 발생하는 HTTP 상태 코드
     */
    clienterror(fd, req.method_str, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  /* 나머지 요청 헤더들을 끝(blank line)까지 읽어서 버린다. */
  /* [HTTP REQUEST FLOW] =============================================== */
  /* HTTP 요청은 다음 순서로 도착한다.                                  */
  /*   1) request line                                                   */
  /*   2) request headers                                                */
  /*   3) blank line                                                     */
  /*   4) request body (POST일 때)                                       */
  /*                                                                    */
  /* POST body 길이(Content-Length)는 헤더 안에 들어 있으므로,            */
  /* body를 읽기 전에 헤더를 먼저 끝까지 읽어야 한다.                    */
  read_requesthdrs(&rio, &req);

  if (req.method == HTTP_METHOD_POST)
  {
    /* [POST BODY SIZE CHECK] ========================================== */
    /* req.body는 고정 크기 배열(char body[MAXBUF])이다.                 */
    /* 따라서 Content-Length가 MAXBUF 이상이면 body를 담을 공간이        */
    /* 부족해져 버퍼 오버플로가 날 수 있으므로 413으로 거절한다.         */
    /*                                                                    */
    /* 여기서는 뒤에 '\0'를 붙여 문자열처럼 다루고 싶기 때문에,          */
    /* 실제로는 "MAXBUF - 1 바이트까지"만 안전하다고 생각하면 된다.      */
    if (req.content_length >= MAXBUF)
    {
      clienterror(fd, "body too large", "413", "Payload Too large", "Tiny buffer too small");
      return;
    }

    /* [Rio_readnb vs Rio_readn] ======================================= */
    /* Rio_readn(fd, ...)    : 정수형 파일 디스크립터(fd)에서 직접 읽는다. */
    /* Rio_readnb(&rio, ...) : rio_t 내부 버퍼를 통해 읽는다.             */
    /*                                                                    */
    /* 지금은 이미 Rio_readinitb(&rio, fd)로 rio 버퍼를 만들었고,          */
    /* request line과 headers도 모두 그 rio 버퍼를 통해 읽어왔다.         */
    /* 따라서 POST body도 같은 rio 버퍼에서 이어서 읽어야 한다.          */
    /*                                                                    */
    /* 여기서 raw fd용 함수인 Rio_readn(fd, ...)을 섞어 쓰면,              */
    /* rio 내부 버퍼에 남아 있던 데이터를 건너뛰거나 입력 흐름이 꼬일 수   */
    /* 있다. 그래서 POST body는 Rio_readnb(&rio, ...)가 맞다.            */
    Rio_readnb(&rio, req.body, req.content_length);
    req.body[req.content_length] = '\0';
  }

  /*
   * URI를 해석해서
   * 1) 정적 콘텐츠인지 동적 콘텐츠인지 구분하고
   * 2) 실제 파일 경로(filename)와 CGI 인자(cgiargs)를 만든다.
   */
  is_static = parse_uri(req.uri, filename, cgiargs);

  /* 요청한 파일의 메타데이터를 읽는다. 존재하지 않으면 404를 보낸다. */
  if (stat(filename, &sbuf) < 0)
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if (is_static)
  {
    /* 정적 파일은 일반 파일이어야 하고, 서버가 읽을 수 있어야 한다. */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }

    /* 파일 내용을 HTTP 응답으로 보낸다. */
    serve_static(fd, filename, sbuf.st_size, &req);
  }
  else
  {
    /* CGI 프로그램은 일반 파일이어야 하고, 실행 권한이 있어야 한다. */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }

    /* CGI 프로그램을 실행해서 동적 콘텐츠를 응답으로 보낸다. */
    serve_dynamic(fd, filename, cgiargs, &req);
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  // Rio_writen(fd, buf, strlen(buf));
  if (safe_rio_writen(fd, buf, strlen(buf)) < 0)
    return;
  sprintf(buf, "Content-type: text/html\r\n");
  // Rio_writen(fd, buf, strlen(buf));
  if (safe_rio_writen(fd, buf, strlen(buf)) < 0)
    return;
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  // Rio_writen(fd, buf, strlen(buf));
  if (safe_rio_writen(fd, buf, strlen(buf)) < 0)
    return;
  // Rio_writen(fd, body, strlen(body));
  if (safe_rio_writen(fd, body, strlen(body)) < 0)
    return;
}

void read_requesthdrs(rio_t *rp, http_request_t *req)
{
  char buf[MAXLINE];

  /* [HEADER PARSE] ==================================================== */
  /* Tiny는 대부분의 요청 헤더를 실제 처리에 쓰지는 않지만,              */
  /* 이번 요청을 정확히 끝까지 소비하기 위해 blank line까지는 읽어야 한다.*/
  /* POST에서 필요한 Content-Length / Content-Type만 따로 저장해 둔다.    */
  req->content_length = 0;
  req->content_type[0] = '\0';

  while (Rio_readlineb(rp, buf, MAXLINE) > 0)
  {
    printf("%s", buf);
    if (!strcmp(buf, "\r\n"))
      break;

    if (!strncasecmp(buf, "Content-Length:", 15))
      req->content_length = atoi(buf + 15);
    else if (!strncasecmp(buf, "Content-Type:", 13))
      sscanf(buf + 13, "%s", req->content_type);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin"))
  {
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri) - 1] == '/')
      strcat(filename, "home.html");
    return 1;
  }
  else
  {
    ptr = index(uri, '?');
    if (ptr)
    {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else
    {
      strcpy(cgiargs, "");
    }
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize, const http_request_t *req)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  // Rio_writen(fd, buf, strlen(buf));
  if (safe_rio_writen(fd, buf, strlen(buf)) < 0)
    return;
  printf("Response headers:\n");
  printf("%s", buf);

  if (!req->send_body)
    return;

  srcfd = Open(filename, O_RDONLY, 0);
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  if (filesize > 0)
  {
    srcp = Malloc(filesize);
    Rio_readn(srcfd, srcp, filesize);
    // Rio_writen(fd, srcp, filesize);
    if (safe_rio_writen(fd, buf, strlen(buf)) < 0)
    {
      Free(srcp);
      Close(srcfd);
      return;
    }
    Free(srcp);
  }
  Close(srcfd);
  // Munmap(srcp, filesize);
}

void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs, const http_request_t *req)
{
  char buf[MAXLINE], *emptylist[] = {NULL};
  int pfd[2];

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  // Rio_writen(fd, buf, strlen(buf));
  if (safe_rio_writen(fd, buf, strlen(buf)) < 0)
    return;
  sprintf(buf, "Server: Tiny Web Server\r\n");
  // Rio_writen(fd, buf, strlen(buf));
  if (safe_rio_writen(fd, buf, strlen(buf)) < 0)
    return;

  /* [CGI + POST + pipe()] ============================================= */
  /* GET은 /cgi-bin/adder?num1=3&num2=5 처럼 URL 뒤 query string으로     */
  /* 데이터를 보내고, CGI는 getenv("QUERY_STRING")로 이를 읽는다.       */
  /*                                                                    */
  /* POST는 num1=3&num2=5 같은 데이터를 request body에 담아 보낸다.      */
  /* CGI 입장에서는 이 body를 stdin에서 읽어야 하므로, POST일 때는        */
  /* pipe()를 하나 만들고 parent가 body를 pipe에 써 넣은 뒤, child가      */
  /* 그 pipe의 read end를 자신의 STDIN_FILENO에 연결한다.                */
  if (req->method == HTTP_METHOD_POST && pipe(pfd) < 0)
    unix_error("pipe error");

  if (Fork() == 0)
  {
    char lenbuf[32];

    /* CGI 프로그램은 REQUEST_METHOD를 보고 GET/POST 분기를 할 수 있다. */
    setenv("REQUEST_METHOD", req->method_str, 1);

    if (req->method == HTTP_METHOD_GET)
    {
      setenv("QUERY_STRING", cgiargs, 1);
    }
    else if (req->method == HTTP_METHOD_POST)
    {
      /* POST에서는 query string 대신 body를 stdin으로 넘긴다. */
      setenv("QUERY_STRING", "", 1);
      sprintf(lenbuf, "%d", req->content_length);
      setenv("CONTENT_LENGTH", lenbuf, 1);
      setenv("CONTENT_TYPE", req->content_type, 1);

      /* child: pipe의 읽기 끝을 stdin으로 연결한다. */
      close(pfd[1]);
      Dup2(pfd[0], STDIN_FILENO);
      Close(pfd[0]);
    }

    Dup2(fd, STDOUT_FILENO);
    Execve(filename, emptylist, environ);
  }

  if (req->method == HTTP_METHOD_POST)
  {
    /* parent: 미리 읽어 둔 POST body를 pipe로 CGI 자식에게 밀어 넣는다. */
    close(pfd[0]);
    Rio_writen(pfd[1], req->body, req->content_length);
    if (safe_rio_writen(pfd[1], req->body, strlen(req->body)) < 0)
    {
      close(pfd[1]);
      return;
    }
    close(pfd[1]);
  }
  // Wait(NULL);
}

void sigchld_handler(int sig)
{
  int olderrno = errno;

  /**
   * waitpid(-1, NULL, WNOHANG)는 “끝난 자식이 있으면 바로 수거하고, 없으면 기다리지 말고 돌아와라”는 뜻
   * while로 도는 이유는 자식이 여러 개 한꺼번에 끝났을 수도 있기 때문
   * errno를 저장했다가 복구하는 건 신호 핸들러에서 흔히 하는 안전한 습관
   *
   *
   * 바꾸고 난 흐름
    1. CGI 요청이 들어옴
    2. 부모가 fork()로 자식 생성
    3. 자식은 Execve()로 CGI 실행해서 클라이언트에 출력
    4. 부모는 기다리지 않고 serve_dynamic()에서 바로 리턴
    5. main()은 부모 쪽 connfd를 닫고 다음 요청을 받으러 감
    6. CGI 자식이 끝나면 커널이 SIGCHLD 보냄
    7. 핸들러가 자식을 수거해서 좀비를 없앰
   */
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  errno = olderrno;
}

// 불완전하게 닫힌 연결은 “응답이 끝나기 전에 peer가 먼저 떠나서, 서버가 죽은 소켓에 쓰게 되는 상황을 방지
static int safe_rio_writen(int fd, void *buf, size_t n)
{
  if (rio_writen(fd, buf, n) != (ssize_t)n)
  {
    if (errno == EPIPE)
      return -1; // 클라이언트가 먼저 끊음 : 현재 요청만 중지

    unix_error("safe_rio_writen error");
  }
  return 0;
}
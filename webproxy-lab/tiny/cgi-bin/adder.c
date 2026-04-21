/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

static void parse_form(char *buf, int *n1, int *n2)
{
  char *p;
  char arg1[MAXLINE], arg2[MAXLINE];

  /* [FORM BODY FORMAT] ================================================ */
  /* 브라우저는 기본 form 인코딩에서 아래처럼 한 줄 문자열을 보낸다.      */
  /*   num1=3&num2=5                                                     */
  /* 여기서는 '&'를 기준으로 둘로 나눈 뒤, '=' 뒤 숫자만 atoi로 꺼낸다.   */
  if (buf == NULL || *buf == '\0')
    return;

  p = strchr(buf, '&');
  if (p == NULL)
    return;

  /*
   * '&' 자리를 '\0'로 바꾸면 C 문자열이 잠시 두 조각으로 나뉜다.
   *   buf   -> "num1=3"
   *   p + 1 -> "num2=5"
   */
  *p = '\0';
  strcpy(arg1, buf);
  strcpy(arg2, p + 1);

  /* 원래 입력 문자열을 다시 출력할 수 있도록 '&'를 복구해 둔다. */
  *p = '&';

  *n1 = atoi(strchr(arg1, '=') + 1);
  *n2 = atoi(strchr(arg2, '=') + 1);
}

int main(void)
{
  char *method;
  char *buf = NULL;
  char postbuf[MAXLINE];
  char content[MAXLINE];
  int n1 = 0, n2 = 0;

  /* [CGI METHOD SPLIT] ================================================= */
  /* GET  : Tiny가 QUERY_STRING 환경변수에 "num1=3&num2=5"를 넣어 준다. */
  /* POST : Tiny가 pipe + stdin으로 body를 넘겨 주고,                    */
  /*        CONTENT_LENGTH / CONTENT_TYPE도 환경변수에 넣어 준다.        */
  method = getenv("REQUEST_METHOD");

  if (method && !strcasecmp(method, "GET"))
  {
    buf = getenv("QUERY_STRING");
  }
  else if (method && !strcasecmp(method, "POST"))
  {
    char *lenstr = getenv("CONTENT_LENGTH");
    int len = lenstr ? atoi(lenstr) : 0;

    /* postbuf도 고정 배열이므로, 읽을 수 있는 최대 길이를 제한한다. */
    if (len >= MAXLINE)
      len = MAXLINE - 1;

    if (len > 0)
    {
      /*
       * POST body는 Tiny가 child stdin으로 pipe 연결해 준 덕분에
       * 여기서 일반 파일처럼 fread(stdin)으로 읽을 수 있다.
       */
      fread(postbuf, 1, len, stdin);
      postbuf[len] = '\0';
      buf = postbuf;
    }
  }

  parse_form(buf, &n1, &n2);

  sprintf(content, "input:%s\r\n<p>", buf ? buf : "");
  sprintf(content + strlen(content), "The answer is: %d + %d = %d\r\n<p>", n1, n2, n1 + n2);

  printf("Content-type: text/html\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("\r\n");
  printf("%s", content);
  fflush(stdout);

  return 0;
}
/* $end adder */

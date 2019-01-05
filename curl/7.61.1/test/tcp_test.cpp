#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <pthread.h>
#include <unistd.h>
 
CURL *curl;
CURLcode res;
curl_socket_t sockfd; /* socket */
 
/* Auxiliary function that waits on the socket. */
static int wait_on_socket(curl_socket_t sockfd, int for_recv, long timeout_ms)
{
  struct timeval tv;
  fd_set infd, outfd, errfd;
  int ret;
 
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec= (timeout_ms % 1000) * 1000;
 
  FD_ZERO(&infd);
  FD_ZERO(&outfd);
  FD_ZERO(&errfd);
 
  FD_SET(sockfd, &errfd); /* always check for error */
 
  if(for_recv)
  {
    FD_SET(sockfd, &infd);
  }
  else
  {
    FD_SET(sockfd, &outfd);
  }
 
  /* select() returns the number of signalled sockets or -1 */
  ret = select(sockfd + 1, &infd, &outfd, &errfd, &tv);
  return ret;
}
 
void* test_recv(void *)
{
  size_t iolen;
  curl_off_t nread;
  puts("Reading response.");
 
  /* read the response */
  for(;;)
  {
    char buf[1024] = {0};
    wait_on_socket(sockfd, 1, 60000L);
    res = curl_easy_recv(curl, buf, 1024, &iolen);
 
    if(CURLE_OK != res)
    break;
 
    nread = (curl_off_t)iolen;
 
    printf("%s\n", buf);
    printf("Received %" CURL_FORMAT_CURL_OFF_T " bytes.\n", nread);
  }
}
 
int main(void)
{
  /* Minimalistic http request */
  //const char *request = "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n";
  const char *request = "test sending\n";
  long sockextr;
  size_t iolen;
  curl_off_t nread;
 
  /* A general note of caution here: if you're using curl_easy_recv() or
  curl_easy_send() to implement HTTP or _any_ other protocol libcurl
  supports "natively", you're doing it wrong and you should stop.
  This example uses HTTP only to show how to use this API, it does not
  suggest that writing an application doing this is sensible.
  */
 
  curl = curl_easy_init();
  if(curl)
  {
    curl_easy_setopt(curl, CURLOPT_URL, "127.0.0.1");
    curl_easy_setopt(curl, CURLOPT_PORT, 9006);
    /* Do not do the transfer - only connect to host */
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
    res = curl_easy_perform(curl);
 
    if(CURLE_OK != res)
    {
      printf("Error: %s\n", strerror(res));
      return 1;
    }
 
    /* Extract the socket from the curl handle - we'll need it for waiting.
    * Note that this API takes a pointer to a 'long' while we use
    * curl_socket_t for sockets otherwise.
    */
    res = curl_easy_getinfo(curl, CURLINFO_LASTSOCKET, &sockextr);
 
    if(CURLE_OK != res)
    {
      printf("Error: %s\n", curl_easy_strerror(res));
      return 1;
    }
 
    sockfd = (curl_socket_t)sockextr;
 
    /* wait for the socket to become ready for sending */
    if(!wait_on_socket(sockfd, 0, 60000L))
    {
      printf("Error: timeout.\n");
      return 1;
    }
 
    pthread_t thread_recv_id;
    pthread_create(&thread_recv_id, NULL, test_recv, NULL);
 
    puts("Sending request.");
    /* Send the request. Real applications should check the iolen
    * to see if all the request has been sent */
LOOP:
    res = curl_easy_send(curl, request, strlen(request), &iolen);
 
    if(CURLE_OK != res)
    {
      printf("Error: %s\n", curl_easy_strerror(res));
      return 1;
    }
    sleep(3);
    goto LOOP;
 
    /* always cleanup */
    curl_easy_cleanup(curl);
  }
  return 0;

}

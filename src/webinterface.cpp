#include "webinterface.h"

pthread_t webInterfaceThread_id;

#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

#define MULTI_PERFORM_HANG_TIMEOUT 60 * 1000

#define FROM     "<keymaster@toyshedstudios.com>"
#define TO       "<8018310780@mms.att.net>"
#define CC       "<info@example.com>"

static const char *fromAddr = FROM;
static const char *toAddr[] = { TO , NULL };

static struct timeval tvnow(void)
{
  struct timeval now;

  /* time() returns the value of time in seconds since the epoch */
  now.tv_sec = (long)time(NULL);
  now.tv_usec = 0;

  return now;
}

static long tvdiff(struct timeval newer, struct timeval older)
{
  return (newer.tv_sec - older.tv_sec) * 1000 +
    (newer.tv_usec - older.tv_usec) / 1000;
}

/*
static const char *payload_text[] = {
  "Date: {{ dateTime }}\r\n",
  "From: {{ fromAddr }}\r\n",
  "{% for addr in toAddrList %}To: {{ addr }}\r\n{% endfor %}",
  "{% for addr in ccAddrList %}Cc: {{ addr }}\r\n{% endfor %}",
  "Message-ID: <dcd7cb36-11db-487a-9f3a-e652a9458efd@rfcpedant.example.org>\r\n",
  "Subject: {{ subject }}\r\n",
  "\r\n", // empty line to divide headers from body, see RFC5322
  "Access was {{ grantedDenied }} for {{ user }}\r\n",
  NULL
};
*/

struct emailFields {
  int lines_read;
  char *fromAddr;
  char *toAddr[];
  char *ccAddr[];
  char *subject;
  char **lines;
};

static size_t payload_source(void *ptr, size_t size, size_t nmemb, void *userp)
{
  struct emailFields *upload_ctx = (struct emailFields *)userp;
  const char *data;

  syslog(LOG_ERR, "payload_source() %d, %d : \n", size, nmemb, upload_ctx->lines_read);


  if((size == 0) || (nmemb == 0) || ((size*nmemb) < 1)) {
    return 0;
  }

  data = upload_ctx->lines[upload_ctx->lines_read];

  if(data) {
    size_t len = strlen(data);
    memcpy(ptr, data, len);
    upload_ctx->lines_read++;

    return len;
  }

  return 0;
}

int sendMMSemail(const char *fromAddr, const char *toAddr[], const char **lines, const char **attachments)
{
  CURL *curl;
  CURLM *mcurl;
  int still_running = 1;
  struct timeval mp_start;
  struct curl_slist *recipients = NULL;
  struct emailFields upload_ctx;

  upload_ctx.lines_read = 0;
  upload_ctx.lines = lines;

  curl_global_init(CURL_GLOBAL_DEFAULT);

  curl = curl_easy_init();
  if(!curl)
    return 1;

  mcurl = curl_multi_init();
  if(!mcurl)
    return 2;

  /* This is the URL for your mailserver */
  curl_easy_setopt(curl, CURLOPT_URL, "smtp://localhost");

  /* Note that this option isn't strictly required, omitting it will result in
   * libcurl sending the MAIL FROM command with empty sender data. All
   * autoresponses should have an empty reverse-path, and should be directed
   * to the address in the reverse-path which triggered them. Otherwise, they
   * could cause an endless loop. See RFC 5321 Section 4.5.5 for more details.
   */
  curl_easy_setopt(curl, CURLOPT_MAIL_FROM, fromAddr);

  /* Add two recipients, in this particular case they correspond to the
   * To: and Cc: addressees in the header, but they could be any kind of
   * recipient. */
  for (int i = 0; toAddr[i] != NULL; i++)
    recipients = curl_slist_append(recipients, toAddr[i]);
  //recipients = curl_slist_append(recipients, CC);
  curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

  /* We're using a callback function to specify the payload (the headers and
   * body of the message). You could just use the CURLOPT_READDATA option to
   * specify a FILE pointer to read from. */
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
  curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

  /* Tell the multi stack about our easy handle */
  curl_multi_add_handle(mcurl, curl);

  /* Record the start time which we can use later */
  mp_start = tvnow();

  /* We start some action by calling perform right away */
  curl_multi_perform(mcurl, &still_running);

  while(still_running) {
    struct timeval timeout;
    fd_set fdread;
    fd_set fdwrite;
    fd_set fdexcep;
    int maxfd = -1;
    int rc;

    long curl_timeo = -1;

    /* Initialise the file descriptors */
    FD_ZERO(&fdread);
    FD_ZERO(&fdwrite);
    FD_ZERO(&fdexcep);

    /* Set a suitable timeout to play around with */
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    curl_multi_timeout(mcurl, &curl_timeo);
    if(curl_timeo >= 0) {
      timeout.tv_sec = curl_timeo / 1000;
      if(timeout.tv_sec > 1)
        timeout.tv_sec = 1;
      else
        timeout.tv_usec = (curl_timeo % 1000) * 1000;
    }

    /* Get file descriptors from the transfers */
    curl_multi_fdset(mcurl, &fdread, &fdwrite, &fdexcep, &maxfd);

    /* In a real-world program you OF COURSE check the return code of the
       function calls.  On success, the value of maxfd is guaranteed to be
       greater or equal than -1.  We call select(maxfd + 1, ...), specially in
       case of (maxfd == -1), we call select(0, ...), which is basically equal
       to sleep. */
    rc = select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);

    if(tvdiff(tvnow(), mp_start) > MULTI_PERFORM_HANG_TIMEOUT) {
      fprintf(stderr,
              "ABORTING: Since it seems that we would have run forever.\n");
      break;
    }

    switch(rc) {
    case -1:  /* select error */
      break;
    case 0:   /* timeout */
    default:  /* action */
      curl_multi_perform(mcurl, &still_running);
      break;
    }
  }

  /* Free the list of recipients */
  curl_slist_free_all(recipients);

  /* Always cleanup */
  curl_multi_remove_handle(mcurl, curl);
  curl_multi_cleanup(mcurl);
  curl_easy_cleanup(curl);
  curl_global_cleanup();

  return 0;
}


/**
* @short Asks a question and returns it.
*
* The returned pointer must be freed by user.
*/
/// Format query string a little bit to understand the query itself
void format_query(void *tmp, const char *key, const void *value, int flags){
	char *temp = (char *)tmp;
    strcat(temp," ");
    strcat(temp,key);
    strcat(temp,"=");
    strcat(temp,(const char *)value);
}

/**
* @ short Returns a the ten most recent access attempts
*/
onion_connection_status accessLog_handler(void *none, onion_request *req, onion_response *res){
    char temp[1024];
    strcpy(temp, onion_request_get_path(req));
    onion_dict_preorder(onion_request_get_query_dict(req),format_query,temp);
	//printf("%s\n", temp);

	onion_dict *out = onion_dict_new();
	dbGetAuditLog(out, 10);

	return onion_shortcut_response_json(out, req, res);
}

/**
 * @short Returns the list of access credentials in the system and the associated user
 */
onion_connection_status accessTable_handler(void *none, onion_request *req, onion_response *res) {
    char temp[1024];

    syslog(LOG_ERR, "Requested Access Table.\n");

    strcpy(temp, onion_request_get_path(req));
    onion_dict_preorder(onion_request_get_query_dict(req),format_query,temp);
	//printf("%s\n", temp);

	onion_dict *out = onion_dict_new();
	dbQueryAccessTable(out);

	return onion_shortcut_response_json(out, req, res);

}

/**
 * @short Returns the list of registered users
 */
onion_connection_status userTable_handler(void *none, onion_request *req, onion_response *res) {
    char temp[1024];

    syslog(LOG_ERR, "Requested User Table.\n");

    strcpy(temp, onion_request_get_path(req));
    onion_dict_preorder(onion_request_get_query_dict(req),format_query,temp);
	//printf("%s\n", temp);

	onion_dict *out = onion_dict_new();
	dbQueryAllUsers(out);

	return onion_shortcut_response_json(out, req, res);

}

/**
* @short Returns the camera's current image as a PNG
*/
onion_connection_status camImage_handler(void *none, onion_request *req, onion_response *res){
    char temp[1024];
    strcpy(temp, onion_request_get_path(req));
    int result = OCS_NOT_PROCESSED;

    // get image from camera
    int camWidth = g_cam->getOutput(0)->Width;
    int camHeight = g_cam->getOutput(0)->Height;

    int img_buff_size = camWidth * camHeight * 4;
    void *img_buff = cvAlloc(img_buff_size);

    if (img_buff != NULL) {
		g_cam->getOutput(0)->ReadFrame(img_buff, img_buff_size);
		result = onion_png_response((unsigned char *)img_buff, 4, camWidth, camHeight, res);
		cvFree(&img_buff);
	}

    return result;
}

/**
 * @short adds a new user to the system
 */
onion_connection_status userAdd_handler(void *none, onion_request *req, onion_response *res) {
    char temp[1024];
    //strcpy(temp, onion_request_get_path(req));
    //onion_dict_preorder(onion_request_get_query_dict(req),format_query,temp);
	//printf("%s\n", temp);
    syslog(LOG_ERR, "userAdd_handler\n");

    if (onion_request_get_flags(req)&OR_HEAD){
        onion_response_write_headers(res);
        return OCS_PROCESSED;
    }

	onion_dict *post = onion_request_get_post_dict(req);
	// alternatively onion_dict *post = onion_dict_from_json(json_data);

	onion_block *json = onion_dict_to_json(post);
	printf("POST DATA: %s\n", onion_block_data(json));
	onion_block_free(json);

	onion_dict *out = onion_dict_new();
	dbQueryAllUsers(out);

	onion_response_set_code(res, HTTP_CREATED);
	return onion_shortcut_redirect("/user", req, res);
}

/**
 * @short adds a new user to the system
 */
 // this method gets query params from the GET method
onion_connection_status user_handler(void *none, onion_request *req, onion_response *res) {
    char temp[1024];
    strcpy(temp, onion_request_get_path(req));
    onion_dict_preorder(onion_request_get_query_dict(req),format_query,temp);
	//printf("%s\n", temp);

	onion_dict *post = onion_request_get_post_dict(req);
	// alternatively onion_dict *post = onion_dict_from_json(json_data);

	onion_block *json = onion_dict_to_json(post);
	printf("POST DATA: %s\n", onion_block_data(json));
	onion_block_free(json);

	onion_dict *out = onion_dict_new();
	dbQueryAllUsers(out);

	onion_response_set_code(res, HTTP_CREATED);
	//return onion_shortcut_response_json(out, req, res);
	return OCS_PROCESSED;
}

/**
 * @short sends an email formatted for MMS, with the more recent entry in the audit log
 */
onion_connection_status sendEmail_handler(void *none, onion_request *req, onion_response *res) {
    syslog(LOG_ERR, "Sending MMS email.\n");

	onion_dict *log = onion_dict_new();
	dbGetAuditLog(log, 1);

    onion_dict *access = onion_dict_get_dict(log, "attempt000");
	onion_dict *user = onion_dict_get_dict(access, "userid");

	const char *accessReason = onion_dict_get(access, "reason");
	const char *accessTime = onion_dict_get(access, "time");
	const char *accessType = onion_dict_get(access, "type");
	const char *accessCode = onion_dict_get(access, "code");
	const char *accessMessage = onion_dict_get(access, "message");
	const char *firstName = onion_dict_get(user, "firstName");
	const char *lastName = onion_dict_get(user, "lastName");

	if (firstName == NULL)
        firstName = "Unknown";
    if (lastName == NULL)
        lastName = "User";

	char msgLine[144] = "";
	char *body[] = { msgLine, NULL };

	if (!strcmp("GRANTED", accessReason))
        sprintf(msgLine, "[TOYSHED] %s : %s %s %s access via %s.", accessTime, firstName, lastName, accessReason, accessType);
    else
        sprintf(msgLine, "[TOYSHED] %s : %s %s %s access via %s using code %s: %s", accessTime, firstName, lastName, accessReason, accessType, accessCode, accessMessage);

	sendMMSemail(fromAddr, toAddr, body, NULL);

    if (req != NULL)
        return onion_shortcut_response("Success.", OCS_PROCESSED, req, res);
    else
        return 0;
}

onion_connection_status staticContent_handler(void *none, onion_request *req, onion_response *res) {
    char path[1024];
    strcpy(path, onion_request_get_path(req));
    syslog(LOG_ERR, "staticContent_handler: %s\n", path);

    if (onion_request_get_flags(req)&OR_HEAD){
        onion_response_write_headers(res);
        return OCS_PROCESSED;
    }

	return onion_shortcut_redirect(path, req, res);
}

/**
* @short In a simple loop asks the user for the answer of the query.
*/
static void *webInterfaceThread(void *data) {
    onion *server=onion_new(O_POOL);

	onion_url *url = onion_root_url(server);
    onion_set_hostname(server, "0.0.0.0");
	onion_set_port(server, "8120"); // 18724

    ONION_INFO("Listening at http://0.0.0.0:18724");

	onion_url_add(url, "camImage.png", (void*)camImage_handler);
	onion_url_add(url, "accessLog.json", (void*)accessLog_handler);
	onion_url_add(url, "accessTable.json", (void*)accessTable_handler);
	onion_url_add(url, "userTable.json", (void*)userTable_handler);

	onion_url_add(url, "testEmail.smtp", (void*)sendEmail_handler);

	//onion_url_add();

	onion_url_add_handler(url, "^html/", onion_handler_export_local_new("html")); /*
        "<html><body><form method=\"POST\" action=\"/addUser\">"
        "<input type=\"text\" name=\"firstName\"><input type=\"text\" name=\"lastName\">"
        "<input type=\"text\" name=\"phone\"><input type=\"text\" name=\"email\">"
        "<input type=\"submit\">"
        "</form></body></html>", HTTP_OK); */
	onion_url_add(url, "addUser", (void*)userAdd_handler);
	//onion_url_add(url, "code/*", (void*)code_handler);

    onion_listen(server);
    onion_free(server);

    return 0;
}

int initWebInterface(){
    pthread_create(&webInterfaceThread_id, NULL, &webInterfaceThread, NULL);

    return 0;
}

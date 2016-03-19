#ifndef __DATABASE_H__
#define __DATABASE_H__

#pragma GCC diagnostic ignored "-fpermissive"

#ifdef __cplusplus
#include <cstdlib>
#include <string>
#include <iostream>
#else
#include <stdlib.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sqlite3.h>
#include <opencv/cv.h>
#include <curl/curl.h>

#include <onion/dict.h>
#include <syslog.h>

#include <ts_util.h>

#define VALID_USERCODE  0
#define INVALID_CODE	1
#define UNKNOWN_USER	2
#define CODE_EXPIRED	3
#define USER_LOCKED		4
#define USER_DISABLED	5
#define USER_RETIRED	6

extern const char *reasonMsgs[];
extern const char *sqlInsertAuditLog;
extern const char *sqlQueryAuditLog;
extern const char *sqlQueryAccessCode;

sqlite3* openDB(const char *fname);
void closeDB(sqlite3 *pDb);
sqlite3_stmt* prepStatement(sqlite3 *pDb, char *sqlStatement);
int execStatement(sqlite3 *pDb, sqlite3_stmt *stmt);
int getRowDict(onion_dict *row, sqlite3 *pDb, sqlite3_stmt *stmt);
int getDictResponse(onion_dict *dict, sqlite3 *pDb, sqlite3_stmt *stmt);

int dbQueryUser(int userid, onion_dict *dict);
int dbQueryAllUsers(onion_dict *dict);
int dbCreateUser(onion_dict *dict);
int dbUpdateUser(int userid, onion_dict *dict);
int dbDeleteUser(int userid);
int dbQueryAccessTable(onion_dict *dict);
int dbQueryAccessEntry(int accessId, onion_dict *dict);

int dbGetAuditLog(char lines[][81], int maxLineLength, int maxLines);
int dbGetAuditLog(onion_dict *dict, int maxLines);

int getCodeDetails(sqlite3* pDb, int method, uint64_t code, int &nearestUserId, int &authorized, int &reason);
void dbLogAccessAttempt(sqlite3 *pDb, int method, uint64_t code, int authorized, const char *msg, int userid, void *image, uint32_t imageLen);
void netLogAccessAttempt(const char *url, int method, uint64_t code, int authorized, const char *msg, int userid, void *image, uint32_t imageLen);

#endif

#include <database.h>

const char *sqlInsertAuditLog = "insert into log (code, type, userid, reason, message, image) values (:code, :method, :userid, :reason, :message, :image)";
const char *sqlQueryAuditLog = "select * from log order by time desc limit 6";
const char *sqlQueryAccessCode = "select userid,active,expires from access where type=:method and code=:code";
const char *sqlQueryAccessTable = "select * from access order by time desc";

const char *sqlQueryUsersTable = "select * from users";
const char *sqlQueryUser = "select * from users where userid=:id";
const char *sqlCreateUser = "insert into users (firstName, lastName, email, phone, status, image) values (:firstName, :lastName, :email, :phone, :status, :image)";
const char *sqlUpdateUser = "update users set firstName=:firstName, lastName=:lastName, email=:email, phone=:phone, status=:status, image=:image where userid=:id";
const char *sqlDeleteUser = "delete from users where userid=:id";

const char *reasonMsgs[] = {	"Valid User/Code",
                              "Invalid Code",
                              "Unknown User",
                              "Code Expired",
                              "User Locked",
                              "User Disabled",
                              "User Retired"
                           };

/*

CREATE TABLE IF NOT EXISTS log (
time TIMESTAMP PRIMARY KEY DEFAULT (strftime('%s', 'now')),
code TEXT,
type TEXT CHECK( type IN ('RFID','PIN','NA') ) NOT NULL DEFAULT 'NA',
userid INTEGER,
message TEXT NOT NULL,
image BLOB,
reason TEXT CHECK( reason IN ('GRANTED','DENIED','NA') ) NOT NULL DEFAULT 'NA'
);

DROP TABLE IF EXISTS users;

CREATE TABLE users (
userid INTEGER PRIMARY KEY AUTOINCREMENT,
firstName TEXT,
lastName TEXT,
email TEXT,
phone TEXT,
image BLOB,
status TEXT CHECK( status IN ('RETIRED','EXPIRED','ACTIVE','DISABLED','LOCKED') ) NOT NULL DEFAULT 'LOCKED',
creator INTEGER NOT NULL,
time TIMESTAMP DEFAULT (strftime('%s', 'now'))
);

CREATE TABLE access (
userid INTEGER NOT NULL,
active TIMESTAMP NOT NULL,
expires TIMESTAMP NOT NULL,
type TEXT CHECK( type IN ('RFID', 'PIN') ) NOT NULL,
code TEXT,
creator INTEGER NOT NULL,
time TIMESTAMP DEFAULT (strftime('%s', 'now')),
PRIMARY KEY(userid, active, expires)
);

CREATE TABLE config (
logLevel INTEGER NOT NULL DEFAULT 0,
logDest TEXT NOT NULL DEFAULT 'keymaster',
facialDetection INTEGER NOT NULL DEFAULT 0,
writeFaces INTEGER NOT NULL DEFAULT 0,
writeFacesPath TEXT,
fontPath TEXT,
fontClockTypeface TEXT,
fontLogTypeface TEXT
);

*/

sqlite3* openDB(const char *fname) {
    sqlite3 *pDb = NULL;

    if ( sqlite3_open_v2(fname, &pDb, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK ) {
        syslog(LOG_ERR, "Error opening sqlite database: %s", fname);
        return NULL;
    }
    syslog(LOG_DEBUG, "Opened sqlite db: %s", fname);
    return pDb;
}

void closeDB(sqlite3 *pDb) {
    sqlite3_close(pDb);
    syslog(LOG_DEBUG, "Closed sqlite database.");
}

sqlite3_stmt* prepStatement(sqlite3 *pDb, const char *sqlStatement) {
    sqlite3_stmt *stmt = NULL;
		int status = sqlite3_prepare_v2(pDb, sqlStatement, -1, &stmt, NULL);
    if ( status != SQLITE_OK ) {
        syslog(LOG_ERR, "Error executing statement: %s", sqlStatement);
        printf("Error executing statement: %s\n", sqlStatement);
        closeDB(pDb);
        stmt = NULL;
		}
		return stmt;
}

int execStatement(sqlite3 *pDb, sqlite3_stmt *stmt) {
		int status;
		while ((status = sqlite3_step(stmt)) != SQLITE_DONE) {
				if (status == SQLITE_BUSY) {
						sleep_ms(100);
				} else if (status == SQLITE_ERROR) {
						printf("Error: %s\n", sqlite3_errmsg(pDb));
						break;
				}
		}
		sqlite3_finalize(stmt);
		return status;
}

int getRowDict(onion_dict *row, sqlite3 *pDb, sqlite3_stmt *stmt) {
    int type;
    const char *name, *table;
    unsigned char* value;
    char timestamp[22];

    syslog(LOG_DEBUG, "sqlitedb: Decoding row columns to onion_dict");
    int numCols = sqlite3_column_count(stmt);

    for (int i = 0; i < numCols; i++) {
        type = sqlite3_column_type(stmt, i);
        table = sqlite3_column_table_name(stmt, i);
        name = sqlite3_column_name(stmt, i);
        value = sqlite3_column_text(stmt, i);
        if (type == SQLITE_INTEGER) {
            if (	!strcmp("active", name) ||
                    !strcmp("expires", name) ||
                    !strcmp("time", name) ) {
                time_t t = sqlite3_column_int(stmt, i);
                tm* localTime = localtime(&t);
                sprintf(timestamp, "[%4d-%02d-%02d %02d:%02d:%02d]",
                        localTime->tm_year + 1900, localTime->tm_mon + 1, localTime->tm_mday,
                        localTime->tm_hour, localTime->tm_min, localTime->tm_sec);
                value = timestamp;
            } else if (	(!strcmp("userid", name) ||
                         !strcmp("creator", name)) &&
                        strcmp("users", table) ) { // don't dig recursively into the users table
                //printf("%d: '%s' => [user info].\n", i, name);
                int userid = sqlite3_column_int(stmt, i);
                onion_dict *userDict = onion_dict_new();
                dbQueryUser(userid, userDict);
                onion_dict_add(row, name, userDict, OD_DICT | OD_DUP_KEY | OD_REPLACE);
                continue;
            }
        }
        if (value != NULL) {
            //printf("%d: '%s' => '%s'.\n", i, name, value);
            onion_dict_add(row, name, value, OD_DUP_ALL | OD_STRING);
        }
    }
    return numCols;
}

int getDictResponse(onion_dict *dict, sqlite3 *pDb, sqlite3_stmt *stmt, const char *base_rowname) {
    char rowname[32];
    int rownum = 0;
    int status = SQLITE_ERROR;
    syslog(LOG_DEBUG, "sqlitedb: Decoding response rows");

    while (true) {
        status = sqlite3_step(stmt);
        if (status == SQLITE_BUSY) {
            // retry... Maybe we should sleep? and track a retry count?
            sleep_ms(1000*1000);
        } else if (status == SQLITE_DONE) {
            break;
        } else if (status == SQLITE_ERROR) {
            printf("Error: %s\n", sqlite3_errmsg(pDb));
            break;
        } else if (status == SQLITE_ROW) {
            // got a result back...
            onion_dict *row = onion_dict_new();
            getRowDict(row, pDb, stmt);
            sprintf(rowname, "%s%03d", base_rowname, rownum++);
            onion_dict_add(dict, rowname, row, OD_DICT | OD_DUP_KEY);
        }
    }
    return status;
}

int dbQueryAllUsers(onion_dict *dict) {
    sqlite3 *pDb = NULL;
    sqlite3_stmt *stmt = NULL;

    syslog(LOG_DEBUG, "sqlitedb: Querying all users");
    if ((pDb = openDB("auditlog.db")) != NULL) {
			if ((stmt = prepStatement(pDb, sqlQueryUsersTable)) != NULL) {
				getDictResponse(dict, pDb, stmt, "user");
				sqlite3_finalize(stmt);
			}
			closeDB(pDb);
		}
    return 0; // should return the number of rows?
}

int dbQueryUser(int userid, onion_dict *dict) {
    sqlite3 *pDb = NULL;
    sqlite3_stmt *stmt = NULL;

    syslog(LOG_DEBUG, "sqlitedb: Querying User: %d", userid);
    if ((pDb = openDB("auditlog.db")) != NULL) {
        if ((stmt = prepStatement(pDb, sqlQueryUser)) != NULL) {
            sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, ":id"), userid);
            int state = sqlite3_step(stmt);
			if (state == SQLITE_ROW) {
                getRowDict(dict, pDb, stmt);
			}
			sqlite3_finalize(stmt);
        }
		closeDB(pDb);
    }
    return 0;
}

int dbCreateUser(onion_dict *dict) {
    sqlite3 *pDb = NULL;
    sqlite3_stmt *stmt = NULL;

    syslog(LOG_DEBUG, "sqlitedb: Creating new user: %s %s", onion_dict_get(dict, "firstName"), onion_dict_get(dict, "lastName"));
    if ((pDb = openDB("auditlog.db")) != NULL) {
			if ((stmt = prepStatement(pDb, sqlCreateUser)) != NULL) {
        sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":firstName"), onion_dict_get(dict, "firstName"), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":lastName"), onion_dict_get(dict, "lastName"), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":phone"), onion_dict_get(dict, "phone"), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":email"), onion_dict_get(dict, "email"), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":status"), onion_dict_get(dict, "status"), -1, SQLITE_STATIC);
        // sqlite3_bind_blob(stmt, sqlite3_bind_parameter_index(stmt, ":image"), image, imageLen, SQLITE_TRANSIENT);   // -- remember, we need to get the image and imageLen before we uncomment this
        execStatement(pDb, stmt);
      }
			closeDB(pDb);
    }
    return 0;
}

int dbUpdateUser(int userid, onion_dict *dict) {
    sqlite3 *pDb = NULL;
    sqlite3_stmt *stmt = NULL;

    syslog(LOG_DEBUG, "sqlitedb: Updating User: %d", userid);
    if ((pDb = openDB("auditlog.db")) != NULL) {
			if ((stmt = prepStatement(pDb, sqlUpdateUser)) != NULL) {
        sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, ":id"), userid);
        sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":firstName"), onion_dict_get(dict, "firstName"), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":lastName"), onion_dict_get(dict, "lastName"), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":phone"), onion_dict_get(dict, "phone"), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":email"), onion_dict_get(dict, "email"), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":status"), onion_dict_get(dict, "status"), -1, SQLITE_STATIC);
        // sqlite3_bind_blob(stmt, sqlite3_bind_parameter_index(stmt, ":image"), image, imageLen, SQLITE_TRANSIENT);   // -- remember, we need to get the image and imageLen before we uncomment this
        execStatement(pDb, stmt);
      }
			closeDB(pDb);
    }
    return 0;
}

int dbDeleteUser(int userid) {
    sqlite3 *pDb = NULL;
    sqlite3_stmt *stmt = NULL;

    syslog(LOG_DEBUG, "sqlitedb: Updating User: %d", userid);
    if ((pDb = openDB("auditlog.db")) != NULL) {
			if ((stmt = prepStatement(pDb, sqlDeleteUser)) != NULL) {
        sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, ":id"), userid);
        execStatement(pDb, stmt);
      }
			closeDB(pDb);
    }
    return 0;
}


int dbQueryAccessTable(onion_dict *dict) {
    sqlite3 *pDb = NULL;
    sqlite3_stmt *stmt = NULL;

    syslog(LOG_DEBUG, "sqlitedb: Querying Access Table");
    if ((pDb = openDB("auditlog.db")) != NULL) {
			if ((stmt = prepStatement(pDb, sqlQueryAccessTable)) != NULL) {
				getDictResponse(dict, pDb, stmt, "entry");
				sqlite3_finalize(stmt);
			}
			closeDB(pDb);
		}
    return 0; // should return ;
}

int dbQueryAccessEntry(int accessId, onion_dict *dict) {
    sqlite3 *pDb = NULL;
    sqlite3_stmt *stmt = NULL;

    syslog(LOG_DEBUG, "sqlitedb: Querying Access Entry: %d", accessId);
    if ((pDb = openDB("auditlog.db")) != NULL) {
			if ((stmt = prepStatement(pDb, sqlQueryAccessTable)) != NULL) {
				getDictResponse(dict, pDb, stmt, "entry");
				sqlite3_finalize(stmt);
			}
			closeDB(pDb);
		}
    return 0; // should return ;
}


int dbGetAuditLog(char lines[][81], int maxLineLength, int maxLines) {
    sqlite3 *pDb = NULL;
    sqlite3_stmt *stmt = NULL;
    int curLine = 0;
    int status;

    syslog(LOG_DEBUG, "sqlitedb: Querying Audit Table");
    // printf("Reading Audit Log...\n");
    if ((pDb = openDB("auditlog.db")) != NULL) {
			if ((stmt = prepStatement(pDb, sqlQueryAuditLog)) != NULL) {
				while (curLine < maxLines) {
					status = sqlite3_step(stmt);
					if (status == SQLITE_BUSY) {
							// retry... Maybe we should sleep? and track a retry count?
					} else if (status == SQLITE_DONE) {
							break;
					} else if (status == SQLITE_ERROR) {
							printf("Error: %s\n", sqlite3_errmsg(pDb));
							break;
					} else if (status == SQLITE_ROW) {
							// got a result back...
							time_t t = sqlite3_column_int(stmt, 0);
							tm* localTime = localtime(&t);
							char timestamp[22];
							sprintf(timestamp, "[%4d-%02d-%02d %02d:%02d:%02d]",
											localTime->tm_year + 1900, localTime->tm_mon + 1, localTime->tm_mday,
											localTime->tm_hour, localTime->tm_min, localTime->tm_sec);

							//int copyLen = maxLineLength - strlen(timestamp) + 1;
							sprintf(lines[curLine], "%s %s : %s", timestamp, sqlite3_column_text(stmt, 3), sqlite3_column_text(stmt, 4));
							curLine++;
					}
				}
				sqlite3_finalize(stmt);
			}
			closeDB(pDb);
		}

    return curLine;
}

// THIS Should be replaced with 'sqlite3_exec' and a callback function (actually, this should probably be the callback function).
int dbGetAuditLog(onion_dict *dict, int maxLines) {
    sqlite3 *pDb = NULL;
    sqlite3_stmt *stmt = NULL;
    int curLine = 0;

    syslog(LOG_DEBUG, "sqlitedb: Querying Audit Table");
    if ((pDb = openDB("auditlog.db")) != NULL) {
		if ((stmt = prepStatement(pDb, sqlQueryAuditLog)) != NULL) {
			getDictResponse(dict, pDb, stmt, "attempt");
			sqlite3_finalize(stmt);
			curLine = onion_dict_count(dict);
		}
		closeDB(pDb);
    } else {
	}
    return curLine;
}

int getCodeDetails(sqlite3* pDb, int method, uint64_t code, int &nearestUserId, int &authorized, int &reason) {
    sqlite3_stmt *stmt = NULL;
    nearestUserId = -99;
    time_t nearestTime = LONG_MAX;
    time_t now = time(NULL);
    int code_found = 0;

    authorized = false;
    reason = INVALID_CODE;

    int status = sqlite3_prepare_v2(pDb, sqlQueryAccessCode, -1, &stmt, NULL);
    if ( status == SQLITE_OK ) {
        int idxMethodParam = sqlite3_bind_parameter_index(stmt, ":method");
        sqlite3_bind_text(stmt, idxMethodParam, method ? "PIN" : "RFID", -1, SQLITE_STATIC);

        char szCode[48];
        sprintf(szCode, method ? "%llu" : "%llX", code);
        int idxCodeParam = sqlite3_bind_parameter_index(stmt, ":code");
        sqlite3_bind_text(stmt, idxCodeParam, szCode, -1, SQLITE_STATIC);

        while ((status = sqlite3_step(stmt)) != SQLITE_DONE) {
            if (status == SQLITE_BUSY) {
                sleep_ms(100);
            } else if (status == SQLITE_ERROR) {
                printf("Error: %s\n", sqlite3_errmsg(pDb));
                break;
            } else if (status == SQLITE_ROW) {
                code_found++;
                reason = UNKNOWN_USER;

                // got a result back...
                int userId = sqlite3_column_int(stmt, 0);
                time_t active = sqlite3_column_int(stmt, 1);
                time_t expires = sqlite3_column_int(stmt, 2);
                printf("Record Found: %u [ %lu  <  %lu < %lu ]\n", userId, active, now, expires);

                authorized = ((now >= active) && ( now <= expires));
                if (authorized) {
                    printf("Authorized User: %u [ %lu  <  %lu < %lu ]\n", userId, active, now, expires);
                    nearestUserId = userId;
                    reason = VALID_USERCODE;
                    break;
                } else {
                    reason = CODE_EXPIRED;
                }

                time_t nearest = MIN(expires - now, now - active);
                if (nearest < nearestTime) {
                    printf("Newest Contender: %u [ %lu  <  %lu < %lu ]\n", userId, active, now, expires);
                    nearestTime = nearest;
                    nearestUserId = userId;
                }
            }
        }

        sqlite3_finalize(stmt);
        return nearestUserId;
    }
    printf("Error executing statement: %s\n", sqlite3_errmsg(pDb));
    return -1;

}

void dbLogAccessAttempt(sqlite3 *pDb, int method, uint64_t code, int authorized, const char *msg, int userid, void *image, uint32_t imageLen) {
    char *zErrMsg = NULL;
    sqlite3_stmt *stmt = NULL;

    int status = sqlite3_prepare_v2(pDb, sqlInsertAuditLog, -1, &stmt, NULL);
    if ( status == SQLITE_OK ) {
        char szCode[48];
        sprintf(szCode, method ? "%llu" : "%llX", code);
        sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":code"), szCode, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":method"), method ? "PIN" : "RFID", -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, ":userid"), userid);
        sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":reason"), authorized ? "GRANTED" : "DENIED", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":message"), msg, -1, SQLITE_TRANSIENT);
        sqlite3_bind_blob(stmt, sqlite3_bind_parameter_index(stmt, ":image"), image, imageLen, SQLITE_TRANSIENT);

        while ((status = sqlite3_step(stmt)) != SQLITE_DONE) {
            if (status == SQLITE_BUSY) {
                sleep_ms(100);
            } else if (status == SQLITE_ERROR) {
                printf("Error: %s\n", sqlite3_errmsg(pDb));
                break;
            }
        }
        sqlite3_finalize(stmt);
    } else {
        printf("SQL Error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
}

void netLogAccessAttempt(const char *url, int method, uint64_t code, int authorized, const char *msg, int userid, void *image, uint32_t imageLen) {
    CURL *curl;
    CURLcode res;
    struct curl_httppost* post = NULL;
    struct curl_httppost* last = NULL;
    char szMethod[5];
    char szCode[49];
    char szUserID[32];

    snprintf(szMethod, 5, "%s", method ? "PIN" : "RFID");
    snprintf(szCode, 48, method ? "%llu" : "%llX", code);
    snprintf(szUserID, 32, "%d", userid);

    fprintf(stderr, " %s : %s : %s : %s\n", szMethod, szCode, szUserID, msg);

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl) {
        curl_formadd(&post, &last,
                     CURLFORM_COPYNAME, "method",
                     CURLFORM_COPYCONTENTS, szMethod,
                     CURLFORM_END);

        curl_formadd(&post, &last,
                     CURLFORM_COPYNAME, "code",
                     CURLFORM_COPYCONTENTS, szCode,
                     CURLFORM_END);

        curl_formadd(&post, &last,
                     CURLFORM_COPYNAME, "userid",
                     CURLFORM_COPYCONTENTS, szUserID,
                     CURLFORM_END);

        curl_formadd(&post, &last,
                     CURLFORM_COPYNAME, "msg",
                     CURLFORM_COPYCONTENTS, msg,
                     CURLFORM_END);

        if (image != NULL) {
            fprintf(stderr, "Adding image to POST\n");
            curl_formadd(&post, &last,
                         CURLFORM_PTRNAME, "image",
                         CURLFORM_BUFFER, "imagename.jpg",
                         CURLFORM_BUFFERPTR, image,
                         CURLFORM_BUFFERLENGTH, imageLen,

                         CURLFORM_END);
        }

        //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "Error posting Access Log entry: %d", res);
        }

        curl_formfree(post);
        curl_easy_cleanup(curl);
    }
}


/* Copyright (C) 2009 PrimeBase Technologies GmbH, Germany
 *
 * PrimeBase Media Stream for MySQL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Barry Leslie
 *
 * 2009-06-17
 *
 * H&G2JCtL
 *
 * PBMS transaction handling test driver.
 *
 * This is a test driver for the PBMS transaction log. It uses 2 tables in a database and
 * inserts transaction records into 1 while writing them to the transaction log. The transaction
 * log reader thread reads the transactions from the log and writes them to the second table.
 * After a recovery the 2 tables should be identical.
 *
 * Built in crash points can be triggered to test that the recovery works correctly.
 *
 */
 
#ifdef UNIT_TEST

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>

#include "cslib/CSConfig.h"
#include "cslib/CSGlobal.h"
#include "cslib/CSThread.h"
#include "cslib/CSStrUtil.h"
#include "cslib/CSStorage.h"

#include "trans_cache_ms.h"
#include "trans_log_ms.h"

#include "mysql.h"

#define CREATE_TABLE_BODY "\
 (\
  blob_ref INT NOT NULL AUTO_INCREMENT,\
  tab_id INT NOT NULL,\
  blob_id BIGINT NOT NULL, \
  committed BOOLEAN NOT NULL DEFAULT 0, \
  PRIMARY KEY (blob_ref, tab_id)\
)\
ENGINE = INNODB\
"
#ifdef LOG_TABLE
#undef LOG_TABLE
#endif

#define LOG_TABLE	"translog"
#define REF_TABLE	"transref_%d"
#define MAX_THREADS	20

#define A_DB_ID 123

#define TEST_DATABASE_NAME "TransTest"
static const char *user_name = "root";
static const char *user_passwd = "";
static int	port = 3306;
static const char *host = "localhost";
static int nap_time = 1000;
static int max_transaction = 10; // The maximum number of records generated per transaction
static bool dump_log = false, overflow_crash = false;
static int crash_site = 0;		// The location to crash at.
static int num_threads = 1;		// The number of writer threads.
//static int rate = 1000;			// The maximum transactions per second to allow.
static time_t timeout = 60;		// How long to run for before crashing or shutting down.
static bool revover_only = false;
static bool recreate = false;

static uint32_t cache_size = 0, log_size = 0;

static MSTrans *trans_log;

static CSThreadList *thread_list;

static MYSQL *new_connection(bool check_for_db);

static CSThread *main_thread;

//------------------------------------------------
class TransTestThread : public CSDaemon {
public:
	TransTestThread(): 
		CSDaemon(thread_list),
		count(0),
		myActivity(0),
		log(NULL),
		stopit(false),
		finished(false),
		mysql(NULL)
		{}

	 ~TransTestThread()
	{
		if (log)
			log->release();
			
		if (mysql)
			mysql_close(mysql);
	}
	 
	MSTrans *log;
	MYSQL *mysql;
	uint32_t count;
	uint32_t myActivity;

	bool stopit;
	bool finished;
	
	virtual bool doWork() {return true;}	
};

//------------------------------------------------
class TransTestWriterThread : public TransTestThread {
public:
	TransTestWriterThread():TransTestThread() {}
	
	uint32_t tab_id;
	FILE	*myLog;
	
	void generate_records();
	bool doWork() 
	{
		generate_records();
		finished = true;
		return true;
	}
	
	static TransTestWriterThread *newTransTestWriterThread(uint32_t id)
	{
		TransTestWriterThread *tt;
		enter_();
		
		
		new_(tt, TransTestWriterThread());
		
		char name[32];
		sprintf(name, "write_%d.log", id);
		if (recreate)
			tt->myLog = fopen(name, "w+");
		else {
			tt->myLog = fopen(name, "a+");
			fprintf(tt->myLog, "====================================================\n");
		}
		
		tt->tab_id = id ;
		tt->mysql = new_connection(false);
		tt->log = trans_log;
		trans_log->retain();
		
		return_(tt); 
	}
	
	
};

//------------------------------------------------
class TransTestReaderThread : public TransTestThread {
public:
	TransTestReaderThread():TransTestThread(){}
	
	bool recovering;
	void processTransactionLog();
	bool doWork() 
	{
		processTransactionLog();
		return true;
	}
	
	static TransTestReaderThread *newTransTestReaderThread(MSTrans *log)
	{
		TransTestReaderThread *tt;
		enter_();
		
		new_(tt, TransTestReaderThread());
		tt->mysql = new_connection(false);
		tt->log = log;
		tt->log->retain();
		
		tt->log->txn_SetReader(tt); // The reader daemon is passed in unreferenced.
		tt->recovering = false;
		return_(tt); 
	}
	
	bool rec_found(uint64_t id, uint32_t tab_id) 
	{
		char stmt[100];
		MYSQL_RES *results = NULL;            
		bool found;
		
		sprintf(stmt, "SELECT blob_ref FROM "LOG_TABLE" WHERE blob_ref = %"PRIu64" AND tab_id = %"PRIu32"", id, tab_id); 
		if (mysql_query(mysql, stmt)) {
			printf( "MySQL ERROR: %d \"%s\" line %d\n", mysql_errno(mysql), mysql_error(mysql), __LINE__);
			printf("%s\n", stmt);
			exit(1);
		}
		
		
		results = mysql_store_result(mysql);
		if (!results){
			printf( "MySQL ERROR: %d \"%s\" line %d\n", mysql_errno(mysql), mysql_error(mysql), __LINE__);
			exit(1);
		}

		found = (mysql_num_rows(results) == 1);		
		mysql_free_result(results);
		
		return found;
		
	}
	
	
};

TransTestReaderThread *TransReader;
//------------------------------------------------
static void report_mysql_error(MYSQL *mysql, int line, const char *msg)
{
	printf( "MySQL ERROR: %d \"%s\" line %d\n", mysql_errno(mysql), mysql_error(mysql), line);
	if (msg)
		printf("%s\n", msg);
	exit(1);
}


//------------------------------------------------
static MYSQL *new_connection(bool check_for_db)
{
	MYSQL *mysql;

	mysql = mysql_init(NULL);
	if (!mysql) {
		printf( "mysql_init() failed.\n");
		exit(1);
	}

	if (mysql_real_connect(mysql, host, user_name, user_passwd, NULL, port, NULL, 0) == NULL)
		report_mysql_error(mysql, __LINE__, "mysql_real_connect()");

	if (check_for_db) {
		MYSQL_RES *results = NULL;            
		
		if (mysql_query(mysql, "show databases like \"" TEST_DATABASE_NAME "\""))
			report_mysql_error(mysql, __LINE__, "show databases like \"" TEST_DATABASE_NAME "\"");
		
		results = mysql_store_result(mysql);
		if (!results)
			report_mysql_error(mysql, __LINE__, "mysql_store_result()");


		if (mysql_num_rows(results) != 1) {
			if (mysql_query(mysql, "create database " TEST_DATABASE_NAME ))
				report_mysql_error(mysql, __LINE__, "create database " TEST_DATABASE_NAME );
		}
		mysql_free_result(results);
	}
	
	if (mysql_query(mysql, "use " TEST_DATABASE_NAME ))
		report_mysql_error(mysql, __LINE__, "use " TEST_DATABASE_NAME );

	return mysql;
}

//------------------------------------------------
static void init_database(MYSQL *mysql, int cnt)
{
	char stmt[1024];
	
	unlink("ms-trans-log.dat");
	mysql_query(mysql, "drop table if exists " LOG_TABLE  ";");
	
	if (mysql_query(mysql, "create table " LOG_TABLE  CREATE_TABLE_BODY ";")){
		printf( "MySQL ERROR: %d \"%s\" line %d\n", mysql_errno(mysql), mysql_error(mysql), __LINE__);
		exit(1);
	}

	while (cnt) {
		sprintf(stmt, "drop table if exists " REF_TABLE  ";", cnt);
		mysql_query(mysql, stmt);
		sprintf(stmt, "create table " REF_TABLE  CREATE_TABLE_BODY ";", cnt);
		if (mysql_query(mysql, stmt)){
			printf( "MySQL ERROR: %d \"%s\" line %d\n", mysql_errno(mysql), mysql_error(mysql), __LINE__);
			exit(1);
		}
		cnt--;
	}
}


//------------------------------------------------
static void display_help(const char *app)
{
	printf("\nUsage:\n");
	printf("%s -help | -r  [-t<num_threads>] | -d | [-n] [-sc <cache_size>] [-sl <log_size>] [-c <crash_site>]  [-t<num_threads>] [<timeout>]\n\n", app);
	
	printf("-r: Test recovery after a crash or shutdown.\n");
	printf("-d: Dump the transaction log.\n");
	printf("-n: Recreate the tables and recovery log.\n");
	printf("-c <crash_site>: Crash at this location rather than shutting down. Max = %d\n", MAX_CRASH_POINT+1);
	printf("-t<num_threads>: The number of writer threads to use, default is %d.\n", num_threads);
	//printf("-r<rate>: The number af records to be inserted per second, default is %d.\n", rate);
	printf("<timeout>: The number seconds the test should run before shuttingdown or crashing, default is %d.\n\n", timeout);
	exit(1);
}

//---------------------------------	
static void process_args(int argc, const char * argv[])
{
	if (argc < 2)
		return;
		
	for (int i = 1; i < argc; ) {
		if ( argv[i][0] != '-') { // Must be timeout
			timeout = atoi(argv[i]);
			i++;
			if ((i != argc) || !timeout)
				display_help(argv[0]);
		} else {
			switch (argv[i][1]) {
				case 'h':
					display_help(argv[0]);
					break;
					
				case 'r':
					if (argc > 4 || argv[i][2])
						display_help(argv[0]);
					revover_only = true;
					i++;
					break;
					
				case 'd':
					if (argc != 2 || argv[i][2])
						display_help(argv[0]);
					dump_log = true;
					i++;
					break;
					
				case 'n':
					if (argv[i][2])
						display_help(argv[0]);
					recreate = true;
					i++;
					break;
					
				case 'c':
					if (argv[i][2])
						display_help(argv[0]);
					i++;
					crash_site = atoi(argv[i]);
					if (crash_site == (MAX_CRASH_POINT + 1))
						overflow_crash = true;
					else if ((!crash_site) || (crash_site >  MAX_CRASH_POINT))
						display_help(argv[0]);
					i++;
					break;
					
				case 's': {
						uint32_t size;
						
						size = atol(argv[i+1]);
						if (!size)
							display_help(argv[0]);
							
						if (argv[i][2] == 'c') 
							cache_size = size;
						else if (argv[i][2] == 'l')
							log_size = size;
						else 
							display_help(argv[0]);
						
						i+=2;
					}
					break;
					
				case 't':
					if (argv[i][2])
						display_help(argv[0]);
					i++;
					num_threads = atoi(argv[i]);
					if (!num_threads)
						display_help(argv[0]);
					i++;
					break;
/*					
				case 'r':
					i++;
					rate = atoi(argv[i]);
					if (!rate)
						display_help(argv[0]);
					i++;
					break;
*/
				default:
					display_help(argv[0]);
			}
			
		}
	}
}

//---------------------------------	
static void init_env()
{
	cs_init_memory();
	CSThread::startUp();
	if (!(main_thread = CSThread::newCSThread())) {
		CSException::logOSError(CS_CONTEXT, ENOMEM);
		exit(1);
	}
	
	CSThread::setSelf(main_thread);
	
	enter_();
	try_(a) {
		trans_log = MSTrans::txn_NewMSTrans("./ms-trans-log.dat", /*dump_log*/ true);
		new_(thread_list, CSThreadList()); 
	}
	catch_(a) {
		self->logException();
		CSThread::shutDown();
		exit(1);
	}
	cont_(a);
	
}
//---------------------------------	
static void deinit_env()
{
	if (thread_list) {
		thread_list->release();
		thread_list = NULL;
	}
	
	if (trans_log) {
		trans_log->release();
		trans_log = NULL;
	}
	
	if (main_thread) {
		main_thread->release();
		main_thread = NULL;
	}
	
	CSThread::shutDown();
	cs_exit_memory();
}
//---------------------------------	
static bool verify_database(MYSQL *mysql)
{
	MYSQL_RES **r_results, *l_results = NULL;            
	MYSQL_ROW r_record, l_record;
	bool ok = false;
	int i, log_row_cnt, ref_row_cnt = 0, tab_id;
	char stmt[1024];
	
	r_results = (MYSQL_RES **) malloc(num_threads * sizeof(MYSQL_RES *));
	
	if (mysql_query(mysql, "select * from "LOG_TABLE" where committed = 0 order by blob_ref")) 
		report_mysql_error(mysql, __LINE__, "select * from "LOG_TABLE" order by blob_ref");
						
	l_results = mysql_store_result(mysql);
	if (!l_results)
		report_mysql_error(mysql, __LINE__, "mysql_store_result()");

	log_row_cnt = mysql_num_rows(l_results);
	mysql_free_result(l_results);
	if (log_row_cnt)
		printf("Uncommitted references: %d\n", log_row_cnt);

	//---------
	for (i =0; i < num_threads; i++) {
		sprintf(stmt, "select * from "REF_TABLE" order by blob_ref", i+1);
		if (mysql_query(mysql, stmt)) 
			report_mysql_error(mysql, __LINE__, stmt);
							
		r_results[i] = mysql_store_result(mysql);
		if (!r_results)
			report_mysql_error(mysql, __LINE__, "mysql_store_result()");
			
		ref_row_cnt += mysql_num_rows(r_results[i]);
	}	
	//---------
	if (mysql_query(mysql, "select * from "LOG_TABLE" order by blob_ref")) 
		report_mysql_error(mysql, __LINE__, "select * from "LOG_TABLE" order by blob_ref");
						
	l_results = mysql_store_result(mysql);
	if (!l_results)
		report_mysql_error(mysql, __LINE__, "mysql_store_result()");

	log_row_cnt = mysql_num_rows(l_results);
	
	if (log_row_cnt != ref_row_cnt) {
		if (ref_row_cnt > log_row_cnt) {
			printf("verify_database() Failed: row count doesn't match: log_row_cnt(%d) != ref_row_cnt(%d)\n", log_row_cnt,  ref_row_cnt);
			goto done;
		}
		
		printf("verify_database() Warnning: row count doesn't match: log_row_cnt(%d) != ref_row_cnt(%d)\n", log_row_cnt,  ref_row_cnt);		
		printf("Possible unreferenced BLOBs\n");
	}
	
	if (log_row_cnt == ref_row_cnt) {
		for ( i = 0; i < log_row_cnt; i++) {
			l_record = mysql_fetch_row(l_results);
			tab_id = atol(l_record[1]);
			r_record = mysql_fetch_row(r_results[tab_id-1]);		
			if ((atol(l_record[0]) != atol(r_record[0])) ||
				(atol(l_record[1]) != atol(r_record[1])) ||
				(atol(l_record[2]) != atol(r_record[2]))) {
				
				printf("verify_database() Failed: in row %d, tab_id %d\n", i+1, tab_id);
				printf("field 1:  %d =? %d\n", atol(l_record[0]), atol(r_record[0]));
				printf("field 2:  %d =? %d\n", atol(l_record[1]), atol(r_record[1]));
				printf("field 3:  %d =? %d\n", atol(l_record[2]), atol(r_record[2]));
				goto done;
			}
				
		}
	} else { // The important thing is that there are no BLOBs in the ref tabels that are not in the log table.

		for (i =0; i < num_threads; i++) {
			mysql_free_result(r_results[i]);
			
			sprintf(stmt, "select * from "REF_TABLE" where  blob_ref not in (select blob_ref from TransTest.translog where tab_id = %d)", i+1, i+1);
			if (mysql_query(mysql, stmt)) 
				report_mysql_error(mysql, __LINE__, stmt);
								
			r_results[i] = mysql_store_result(mysql);
			if (!r_results)
				report_mysql_error(mysql, __LINE__, "mysql_store_result()");
				
			if (mysql_num_rows(r_results[i])) {
				printf("verify_database() Failed, Missing BLOBs: %s\n", stmt);
				goto done;
			}
		}	
	}
	
	printf("verify_database() OK.\n");
	ok = true;
	
	done:
	
	for (i =0; i < num_threads; i++) {
		mysql_free_result(r_results[i]);
	}
	free(r_results);
	
	mysql_free_result(l_results);
	
#ifdef DEBUG	
	if (!ok) {
		trans_log->txn_DumpLog("trace.log");
	}
#endif	
	return ok;
}

//------------------------------------------------
void TransTestReaderThread::processTransactionLog()
{
	MSTransRec rec = {0};
	MS_TxnState state;
	char stmt[1024];
	uint32_t last_tid = 0;
	enter_();
	
	// Read in transactions from the log and update
	// the database table based on them.
	
	try_(a) {
		while (!myMustQuit && !stopit) {
			// This will sleep while waiting for the next 
			// completed transaction.
			log->txn_GetNextTransaction(&rec, &state); 
			if (myMustQuit)
				break;

			myActivity++;
#ifdef CHECK_TIDS
			if (num_threads == 1) {
				ASSERT( ((last_tid + 1) == rec.tr_id) || (last_tid  == rec.tr_id) || !last_tid);
				last_tid = rec.tr_id;
			}
#endif			
			if (!recovering) 
				count++;
			
			switch (TRANS_TYPE(rec.tr_type)) {
				case MS_ReferenceTxn:
				case MS_DereferenceTxn:
				case MS_RollBackTxn:
				case MS_CommitTxn:
				case MS_RecoveredTxn:
				break;
				default:
					printf("Unexpected transaction type: %d\n", rec.tr_type);
					exit(1);							
			}
			
			if (state == MS_Committed){
				// Dereferences are applied when the transaction is commited.
				// References are applied imediatly and removed if the transaction is rolled back.
				if (TRANS_TYPE(rec.tr_type) == MS_DereferenceTxn) {
					sprintf(stmt, "DELETE FROM "LOG_TABLE" WHERE blob_ref = %"PRIu64" AND tab_id = %d AND blob_id = %"PRIu64"", rec.tr_blob_ref_id, rec.tr_tab_id, rec.tr_blob_id); 
					if (mysql_query(mysql, stmt))  
						report_mysql_error(mysql, __LINE__, stmt);
				} else if (TRANS_TYPE(rec.tr_type) == MS_ReferenceTxn) {
					sprintf(stmt, "UPDATE "LOG_TABLE" SET committed = 1 WHERE blob_ref = %"PRIu64" AND tab_id = %d AND blob_id = %"PRIu64"", rec.tr_blob_ref_id, rec.tr_tab_id, rec.tr_blob_id); 
					if (mysql_query(mysql, stmt))  
						report_mysql_error(mysql, __LINE__, stmt);
				}
			} else if (state == MS_RolledBack) { 
				//printf("ROLLBACK!\n");
				if (TRANS_TYPE(rec.tr_type) == MS_ReferenceTxn) {
					sprintf(stmt, "DELETE FROM "LOG_TABLE" WHERE blob_ref = %"PRIu64" AND tab_id = %d AND blob_id = %"PRIu64"", rec.tr_blob_ref_id, rec.tr_tab_id, rec.tr_blob_id); 
					if (mysql_query(mysql, stmt))  
						report_mysql_error(mysql, __LINE__, stmt);
				}
			} else if (state == MS_Recovered) { 
				printf("Recovered transaction being ignored:\n");
				printf("blob_ref = %"PRIu64", tab_id = %d, blob_id = %"PRIu64"\n\n", rec.tr_blob_ref_id, rec.tr_tab_id, rec.tr_blob_id);
			} else {
				printf("Unexpected transaction state: %d\n", state);
				exit(1);							
			}
			
			
		}
	}
	catch_(a) {
		self->logException();
		printf("\n\n!!!!!!!! THE TRANSACTION LOG READER DIED! !!!!!!!!!!!\n\n");
		if (!myMustQuit && !stopit)
			exit(1);
	}
	cont_(a);
	printf("The transaction log reader shutting down.\n");
	exit_();
}

//------------------------------------------------
void TransTestWriterThread::generate_records()
{

	MS_Txn	txn_type;		
	uint64_t	blob_id;	
	uint64_t	blob_ref_id;	
	int tsize, i;
	bool do_delete;
		
	char stmt[1024];
	enter_();

	try_(a) {
		while (!myMustQuit && !stopit) {
		
			myActivity++;
			usleep(nap_time); // Give up a bit of time
			if (myMustQuit || stopit)
				break;
				
			tsize = rand() % max_transaction;
			
			if (mysql_autocommit(mysql, 0))
				report_mysql_error(mysql, __LINE__, "mysql_autocommit()");
				
			i = 0;
			do {
				do_delete = ((rand() %2) == 0);
				
				// decide if this is an insert or delete
				if (do_delete) {
					MYSQL_RES *results = NULL;            
					MYSQL_ROW record;
					int cnt;
					
					// If we are deleting then randomly select a record to delete
					// and delete it. 
					
					txn_type = MS_DereferenceTxn;

					sprintf(stmt, "select * from "REF_TABLE, tab_id); 
					if (mysql_query(mysql, stmt)) 
						report_mysql_error(mysql, __LINE__, stmt);
						
					results = mysql_store_result(mysql);
					if (!results)
						report_mysql_error(mysql, __LINE__, "mysql_store_result()");
						
					cnt = mysql_num_rows(results);
					if (!cnt)
						do_delete = false; // There is nothing to delete
					else {
						mysql_data_seek(results, rand()%cnt);
						record = mysql_fetch_row(results);
							
						blob_ref_id = atol(record[0]);
						blob_id = atol(record[2]);
						
						sprintf(stmt, "DELETE FROM "REF_TABLE" WHERE blob_ref = %"PRIu64" AND blob_id = %"PRIu64"", tab_id, blob_ref_id, blob_id); 
						if (mysql_query(mysql, stmt))  
							report_mysql_error(mysql, __LINE__, stmt);
							
						if (mysql_affected_rows(mysql) == 0)
							do_delete = false; // Another thread must have deleted the row first.
						else
							fprintf(myLog, "DELETE %"PRIu64" %"PRIu64"\n", blob_ref_id, blob_id); 
					}
					
					mysql_free_result(results);
				} 
				
				if (!do_delete) {
					blob_id = self->myTID; // Assign the tid as the blob id to help with debugging.
					txn_type = MS_ReferenceTxn;
					
					sprintf(stmt, "INSERT INTO "REF_TABLE" VALUES( NULL, %d, %"PRIu64", 0)", tab_id, tab_id, blob_id); 
					if (mysql_query(mysql, stmt)) 
						report_mysql_error(mysql, __LINE__, stmt);
						
					blob_ref_id = mysql_insert_id(mysql);
					if (!blob_ref_id)
						report_mysql_error(mysql, __LINE__, "mysql_insert_id() returned 0");
					
					fprintf(myLog, "INSERT %"PRIu64" %"PRIu64"\n", blob_ref_id, blob_id);	
					// Apply the blob reference now. This will be undone if the transaction is rolled back.
					sprintf(stmt, "INSERT INTO "LOG_TABLE" VALUES(%"PRIu64", %d, %"PRIu64", 0)", blob_ref_id, tab_id, blob_id); 
					if (mysql_query(mysql, stmt)) 
						report_mysql_error(mysql, __LINE__, stmt);
				}

				i++;
				count++;
				if (i >= tsize) { //Commit the database transaction before the log transaction.
					bool rollback;
					
					rollback = ((tsize > 0) && ((rand() % 1000) == 0));
					if (rollback) {
						printf("Rollback\n");
						if (mysql_rollback(mysql)) // commit the staement to the database,
							report_mysql_error(mysql, __LINE__, "mysql_rollback()");	
						fprintf(myLog, "Rollback %"PRIu32"\n", self->myTID);	
						log->txn_LogTransaction(MS_RollBackTxn);
					} else {
						if (mysql_commit(mysql)) // commit the staement to the database,
							report_mysql_error(mysql, __LINE__, "mysql_commit()");	
						fprintf(myLog, "Commit %"PRIu32"\n", self->myTID);	
						log->txn_LogTransaction(txn_type, true, A_DB_ID, tab_id, blob_id, blob_ref_id);
					}
				} else
					log->txn_LogTransaction(txn_type, false, A_DB_ID, tab_id, blob_id, blob_ref_id);
								
			} while ( i < tsize);
						
		}
	}
	
	catch_(a) {
		self->logException();
		printf("\n\nA writer thread for table %d died! \n\n", tab_id);
		if (i == tsize) {
			printf(" It is possible that the last %d operations on table %d were committed to the database but not to the log.\n", tsize, tab_id);
		}
		if (!myMustQuit && !stopit)
			exit(1);
	}
	cont_(a);
	printf("Writer thread for table %d is shutting down.\n", tab_id);
	exit_();
}

// SELECT * FROM TransTest.translog where  blob_ref not in (select blob_ref from TransTest.transref)
// SELECT * FROM TransTest.transref_1 where  blob_ref not in (select blob_ref from TransTest.translog where tab_id = 1)
// SELECT * FROM TransTest.translog where  tab_id = 1 AND blob_ref not in (select blob_ref from TransTest.transref_1)
// select count(*) from TransTest.translog where committed = 1
//---------------------------------	
int main (int argc, const char * argv[]) 
{
	MYSQL *mysql;
	TransTestWriterThread **writer = NULL;
	int rtc = 1;
	
	process_args(argc, argv);
	
	mysql = new_connection(true);
	
	if (recreate)
		init_database(mysql, num_threads);
		
	init_env();
	enter_();
	
	if (dump_log) {
		printf("LOG dumped\n");
		exit(1);
	}
	
	TransReader = TransTestReaderThread::newTransTestReaderThread(trans_log);
	push_(TransReader);
	TransReader->recovering = true;
	TransReader->start();
	
	// wait until the recovery is complete.
	while (trans_log->txn_GetNumRecords())
		usleep(100);
		
	TransReader->recovering = false;
	
	if (log_size)
		trans_log->txn_SetLogSize(log_size);
		
	if (cache_size)
		trans_log->txn_SetCacheSize(cache_size);
		
	if (revover_only) {
		TransReader->stopit = true;
		if (verify_database(mysql))
			rtc = 0;
		goto done;
	}
	
	try_(a) {
		writer = (TransTestWriterThread **) cs_malloc(num_threads * sizeof(TransTestWriterThread *));
		for (int i = 0; i < num_threads; i++) {
			TransTestWriterThread *wt = TransTestWriterThread::newTransTestWriterThread(i+1);
			wt->start();
			writer[i] = wt;
		}
	
		printf("Timeout: %d seconds\n", timeout); 
		timeout += time(NULL);
		int header = 0;
		while (timeout > time(NULL)) {
			MSTransStatsRec stats;
			self->sleep(1000);
			trans_log->txn_GetStats(&stats);
			
			
			if (!(header%20)) {
				for (int i = 0; i < num_threads; i++) {				
					if (writer[i]->myActivity == 0) {
						printf("Writer thread %d HUNG!!!\n", i);
					}
					writer[i]->myActivity = 0;
				}
				
				if (TransReader->myActivity == 0) {
					printf("Reader thread HUNG!!!\n");
				}
				TransReader->myActivity = 0;
					
				printf("%s | %s | %s | %s | %s | %s | %s | %s\n", "LogSize", "Full", "MaxSize", "Overflows", "Overflowing", "CacheSize", "Cache Used", "Cache Hit");
			}
			header++;
			//printf("Writes: %d \t\t Reads: %d \t%d \t start: %"PRIu64"\t\t eol:%"PRIu64"\n", count, TransReader->count, count - TransReader->count, trans_log->txn_Start, trans_log->txn_EOL);
			printf("%7llu | %3d%% | %7llu | %9d | %11s | %9d | %9d%% | %9d%%\n",// | \t\t\t%"PRIu64" \t%"PRIu64"\n", 
				stats.ts_LogSize,
				stats.ts_PercentFull,
				stats.ts_MaxSize,
				stats.ts_OverflowCount,
				(stats.ts_IsOverflowing)?"Over Flow": "   ---   ",
				stats.ts_TransCacheSize,
				stats.ts_PercentTransCacheUsed,
				stats.ts_PercentCacheHit//, trans_log->txn_Start, trans_log->txn_EOL
				);
				
				if (stats.ts_IsOverflowing && overflow_crash) {
					printf("Simulating crash while in overflow\n");
					exit(1);
				}
		}

#ifdef CRASH_TEST		
		if (crash_site) {
			printf("Crashing at crash site %d\n", crash_site);
			trans_test_crash_point = crash_site;
			// set the crash site and wait to die.
			while(1)
				self->sleep(1000);
		}
#endif
		
		printf("Shutting down the writer threads:\n");
		for (int i = 0; i < num_threads; i++) {
			writer[i]->stopit = true;
		}
		
		TransReader->stopit = true;
		// Give the writers a chance to shutdown by themselves.
		int cnt = 100;
		while (cnt) {
			int i;
			for (i = 0; i < num_threads && writer[i]->finished; i++);
			if (i == num_threads && TransReader->finished)
				break;
			self->sleep(10);	
			cnt--;			
		}
		
		for (int i = 0; i < num_threads; i++) {
			writer[i]->stop();
		}
		
	}
	rtc = 0;
	catch_(a) {
		printf("Main thread abort.\n");
		self->logException();
	}
	cont_(a);
	if (writer) {
		for (int i = 0; i < num_threads; i++) {
			writer[i]->stop();
			writer[i]->release();
		}
		cs_free(writer);
	}
		
done:
	TransReader->stop();
	release_(TransReader);
	
	outer_();
	
	thread_list->stopAllThreads();
	deinit_env();
	mysql_close(mysql);
	exit(rtc);
}

#endif // UNIT_TEST

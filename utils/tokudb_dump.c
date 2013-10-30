/* -*- mode: C; c-basic-offset: 3 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <arpa/inet.h>
#include "tokudb_common.h"

//tokufs hacks
int meta_dump;
int novals;

typedef struct {
   bool     leadingspace;
   bool     plaintext;
   bool     header;
   bool     footer;
   bool     is_private;
   bool     recovery_and_txn;
   char*    progname;
   char*    homedir;
   char*    database;
   char*    subdatabase;
   int      exitcode;
   int      recover_flags;
   DBTYPE   dbtype;
   DBTYPE   opened_dbtype;
   DB*      db;
   DB_ENV*  dbenv;
} dump_globals;

dump_globals g;
#include "tokudb_common_funcs.h"

static int   usage          (void);
static int   create_init_env(void);
static int   dump_database  (void);
static int   open_database  (void);
static int   dump_pairs     (void);
static int   dump_footer    (void);
static int   dump_header    (void);
static int   close_database (void);

int test_main(int argc, char *const argv[]) {
   int ch;
   int retval;

   /* Set up the globals. */
   memset(&g, 0, sizeof(g));
   g.leadingspace   = true;
   //TODO: Uncomment when DB_UNKNOWN + db->get_type are implemented.
   g.dbtype         = DB_UNKNOWN;
   //g.dbtype         = DB_BTREE;
   g.progname       = argv[0];
   g.header         = true;
   g.footer         = true;
   g.recovery_and_txn = false;
   (void) dump_footer;
   (void) dump_header;

   if (verify_library_version() != 0) goto error;

   while ((ch = getopt(argc, argv, "qmd:f:h:klNP:ps:RrVTx")) != EOF) {
      switch (ch) {
         case 'q':
             novals=1;
             break;
         case ('d'): {
            PRINT_ERRORX("-%c option not supported.\n", ch);
            goto error;
         }
         case ('f'): {
            if (freopen(optarg, "w", stdout) == NULL) {
               fprintf(stderr,
                       "%s: %s: reopen: %s\n",
                       g.progname, optarg, strerror(errno));
               goto error;
            }
            break;
         }
         case ('h'): {
            g.homedir = optarg;
            break;
         }
         case ('k'): {
            PRINT_ERRORX("-%c option not supported.\n", ch);
            goto error;
         }
         case ('l'): {
            //TODO: Implement (Requires master database support)
            PRINT_ERRORX("-%c option not supported.\n", ch); //YET!
            goto error;
         }
         case 'm':
            meta_dump = 1;
            break;
         case ('N'): {
            PRINT_ERRORX("-%c option not supported.\n", ch);
            goto error;
         }
         case ('P'): {
            /* Clear password. */
            memset(optarg, 0, strlen(optarg));
            PRINT_ERRORX("-%c option not supported.\n", ch);
            goto error;
         }
         case ('p'): {
            g.plaintext = true;
            break;
         }
         case ('R'): {
            //TODO: Uncomment when DB_SALVAGE,DB_AGGRESSIVE are implemented.
            /*g.recover_flags |= DB_SALVAGE | DB_AGGRESSIVE;*/

            //TODO: Implement aggressive recovery (requires db->verify())
            PRINT_ERRORX("-%c option not supported.\n", ch);
            goto error;
         }
         case ('r'): {
            //TODO: Uncomment when DB_SALVAGE,DB_AGGRESSIVE are implemented.
            /*g.recover_flags |= DB_SALVAGE;*/

            //TODO: Implement recovery (requires db->verify())
            PRINT_ERRORX("-%c option not supported.\n", ch);
            goto error;
         }
         case ('s'): {
            g.subdatabase = optarg;
            break;
         }
         case ('V'): {
            printf("%s\n", db_version(NULL, NULL, NULL));
            goto cleanup;
         }
         case ('T'): {
            g.plaintext    = true;
            g.leadingspace = false;
            g.header       = false;
            g.footer       = false;
            break;
         }
         case ('x'): {
	    g.recovery_and_txn = false;
	    break;
	 }
         case ('?'):
         default: {
            g.exitcode = usage();
            goto cleanup;
         }
      }
   }
   argc -= optind;
   argv += optind;

   //TODO: Uncomment when DB_SALVAGE,DB_AGGRESSIVE,DB_PRINTABLE,db->verify are implemented.
   /*
   if (g.plaintext) g.recover_flags |= DB_PRINTABLE;

   if (g.subdatabase != NULL && IS_SET_ALL(g.recover_flags, DB_SALVAGE)) {
      if (IS_SET_ALL(g.recover_flags, DB_AGGRESSIVE)) {
         PRINT_ERRORX("The -s and -R options may not both be specified.\n");
         goto error;
      }
      PRINT_ERRORX("The -s and -r options may not both be specified.\n");
      goto error;
      
   }
   */

   if (argc != 1) {
      g.exitcode = usage();
      goto cleanup;
   }

   init_catch_signals();
   
   g.database = argv[0];
   if (caught_any_signals()) goto cleanup;
   if (create_init_env() != 0) goto error;
   if (caught_any_signals()) goto cleanup;
   if (dump_database() != 0) goto error;
   if (false) {
error:
      g.exitcode = EXIT_FAILURE;
      fprintf(stderr, "%s: Quitting out due to errors.\n", g.progname);
   }
cleanup:
   if (g.dbenv && (retval = g.dbenv->close(g.dbenv, 0)) != 0) {
      g.exitcode = EXIT_FAILURE;
      fprintf(stderr, "%s: %s: dbenv->close\n", g.progname, db_strerror(retval));
   }
   //   if (g.subdatabase)      free(g.subdatabase);
   resend_signals();

   return g.exitcode;
}

int dump_database()
{
   int retval;

   /* Create a database handle. */
   retval = db_create(&g.db, g.dbenv, 0);
   if (retval != 0) {
      PRINT_ERROR(retval, "db_create");
      return EXIT_FAILURE;
   }

   /*
   TODO: If/when supporting encryption
   if (g.password && (retval = db->set_flags(db, DB_ENCRYPT))) {
      PRINT_ERROR(ret, "DB->set_flags: DB_ENCRYPT");
      goto error;
   }
   */
   if (open_database() != 0) goto error;
   if (caught_any_signals()) goto cleanup;
   //if (g.header && dump_header() != 0) goto error;
   if (caught_any_signals()) goto cleanup;
   if (dump_pairs() != 0) goto error;
   if (caught_any_signals()) goto cleanup;
   //if (g.footer && dump_footer() != 0) goto error;

   if (false) {
error:
      g.exitcode = EXIT_FAILURE;
   }
cleanup:

   if (close_database() != 0) g.exitcode = EXIT_FAILURE;

   return g.exitcode;
}

int usage()
{
   fprintf(stderr,
           "usage: %s [-pVT] [-x] [-f output] [-h home] [-s database] db_file\n",
           g.progname);
   return EXIT_FAILURE;
}

int create_init_env()
{
   int retval;
   DB_ENV* dbenv;
   int flags;
   //TODO: Experiments to determine right cache size for tokudb, or maybe command line argument.

   retval = db_env_create(&dbenv, 0);
   if (retval) {
      fprintf(stderr, "%s: db_dbenv_create: %s\n", g.progname, db_strerror(retval));
      goto error;
   }
   ///TODO: UNCOMMENT/IMPLEMENT dbenv->set_errfile(dbenv, stderr);
   dbenv->set_errpfx(dbenv, g.progname);
   /*
   TODO: Anything for encryption?
   */

   /* Open the dbenvironment. */
   g.is_private = false;
   //flags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_USE_ENVIRON;
   flags = DB_INIT_LOCK | DB_INIT_MPOOL; ///TODO: UNCOMMENT/IMPLEMENT | DB_USE_ENVIRON;
   if (g.recovery_and_txn) {
      SET_BITS(flags, DB_INIT_LOG | DB_INIT_TXN | DB_RECOVER);
   }
   
   /*
   ///TODO: UNCOMMENT/IMPLEMENT  Notes:  We require DB_PRIVATE
   if (!dbenv->open(dbenv, g.homedir, flags, 0)) goto success;
   */

   /*
   ///TODO: UNCOMMENT/IMPLEMENT 
   retval = dbenv->set_cachesize(dbenv, 0, cache, 1);
   if (retval) {
      PRINT_ERROR(retval, "DB_ENV->set_cachesize");
      goto error;
   }
   */
   g.is_private = true;
   //TODO: Do we want to support transactions even in single-process mode?
   //Logging is not necessary.. this is read-only.
   //However, do we need to use DB_INIT_LOG to join a logging environment?
   //REMOVE_BITS(flags, DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_TXN);
   SET_BITS(flags, DB_CREATE | DB_PRIVATE);
#if defined(USE_BDB) && USE_BDB==1
   {
       int r;
       r = dbenv->set_lk_max_objects(dbenv, 100000);
       assert(r==0);
       r = dbenv->set_lk_max_locks(dbenv, 100000);
       assert(r==0);
   }
#endif

   retval = dbenv->open(dbenv, g.homedir, flags, 0);
   if (retval) {
      PRINT_ERROR(retval, "DB_ENV->open");
      goto error;
   }
   g.dbenv = dbenv;
   return EXIT_SUCCESS;

error:
   return EXIT_FAILURE;
}

#define DUMP_FLAG(bit, dump)    if (IS_SET_ALL(flags, bit)) printf(dump);

#define DUMP_IGNORED_FLAG(bit, dump)


int dump_header()
{
   u_int32_t flags;
   int retval;
   DB* db = g.db;
   
   assert(g.header);
   printf("VERSION=3\n");
   printf("format=%s\n", g.plaintext ? "print" : "bytevalue");
   //TODO: Uncomment when DB_UNKNOWN + db->get_type are implemented.
   /*assert(g.dbtype == DB_BTREE || (g.dbtype == DB_UNKNOWN && g.opened_dbtype == DB_BTREE));*/
   printf("type=btree\n");
   //TODO: Get page size from db.  Currently tokudb does not support db->get_pagesize.
   //Don't print this out //printf("db_pagesize=4096\n");
   if (g.subdatabase) {
      printf("subdatabase=");
      outputplaintextstring(g.subdatabase);
      printf("\n");      
   }
   //TODO: Uncomment when db->get_flags is implemented
   if ((retval = db->get_flags(db, &flags)) != 0) {
      PRINT_ERROR(retval, "DB->get_flags");
      goto error;
   }
   DUMP_IGNORED_FLAG(DB_CHKSUM,    "chksum=1\n");
   DUMP_IGNORED_FLAG(DB_RECNUM,    "recnum=1\n");
   printf("HEADER=END\n");
   
   if (ferror(stdout)) goto error;
   return EXIT_SUCCESS;

error:
   return EXIT_FAILURE;
}

int dump_footer()
{
   printf("DATA=END\n");
   if (ferror(stdout)) goto error;

   return EXIT_SUCCESS;
error:
   return EXIT_FAILURE;
}

int open_database()
{
   DB* db = g.db;
   int retval;

   int open_flags = 0;//|DB_RDONLY;
   //TODO: Transaction auto commit stuff
   SET_BITS(open_flags, DB_AUTO_COMMIT);

   retval = db->open(db, NULL, g.database, g.subdatabase, g.dbtype, open_flags, 0666);
   if (retval != 0) {
      PRINT_ERROR(retval, "DB->open: %s", g.database);
      goto error;
   }
   //TODO: Uncomment when DB_UNKNOWN + db->get_type are implemented.
   /*
   retval = db->get_type(db, &g.opened_dbtype);
   if (retval != 0) {
      PRINT_ERROR(retval, "DB->get_type");
      goto error;
   }
   if (g.opened_dbtype != DB_BTREE) {
      PRINT_ERRORX("Unsupported db type %d\n", g.opened_dbtype);
      goto error;
   }
   if (g.dbtype != DB_UNKNOWN && g.opened_dbtype != g.dbtype) {
      PRINT_ERRORX("DBTYPE %d does not match opened DBTYPE %d.\n", g.dbtype, g.opened_dbtype);
      goto error;
   }*/
   return EXIT_SUCCESS;
error:
   fprintf(stderr, "Quitting out due to errors.\n");
   return EXIT_FAILURE;
}
static uint64_t ntohl64(uint64_t x)
{
    uint64_t right = ntohl(x >> 32);
    uint64_t left = (uint64_t) ntohl(x & 0xFFFFFFFF) << 32;
    return left | right;
}
#if 0
static int dump_dbt(DBT* dbt)
{
   char* str;
   u_int32_t idx;
   
   assert(dbt);
   str = (char*)dbt->data;
   if (g.leadingspace) printf(" ");
   if (dbt->size > 0) {
      assert(dbt->data);
      for (idx = 0; idx < dbt->size; idx++) {
         outputbyte(str[idx]);
         if (ferror(stdout)) {
            perror("stdout");
            goto error;
         }
      }
   }
   printf("\n");
   if (false) {
error:
      g.exitcode = EXIT_FAILURE;
   }
   return g.exitcode;
}
#endif

static int dump_dbt(DBT* dbt, bool key)
{
   char* str;
   u_int32_t idx;
   
   assert(dbt);
   str = (char*)dbt->data;
   if (g.leadingspace) printf(" ");
   if (dbt->size > 0 && (key || !novals)) {
      assert(dbt->data);
      if (key && !meta_dump) {
          // data block keys have null terminated string prefixes
          char * keystr = dbt->data;
          printf("%s", keystr);
          // HACK account for the magic data db key byte
          uint64_t * n = dbt->data + dbt->size - sizeof(uint64_t) - 1;
          uint64_t x = ntohl64(*n);
          printf("[%lu]", x);
      } else if (key && meta_dump) {
          // meta keys are null terminated strings
          char * keystr = dbt->data; 
          printf("%s", keystr);
      } else {
          for (idx = 0; idx < dbt->size; idx++) {
             outputbyte(str[idx]);
             if (ferror(stdout)) {
                perror("stdout");
                goto error;
             }
          }
          printf("\n");
      }
   } else if (!key && novals) {
       printf("\n");
   }
   if (false) {
error:
      g.exitcode = EXIT_FAILURE;
   }
   return g.exitcode;
}

static int dump_pairs()
{
   int retval;
   DBT key;
   DBT data;
   DB* db = g.db;
   DBC* dbc = NULL;

   memset(&key, 0, sizeof(key));
   memset(&data, 0, sizeof(data));

   DB_TXN* txn = NULL;
   if (g.recovery_and_txn) {
      retval = g.dbenv->txn_begin(g.dbenv, NULL, &txn, 0);
      if (retval) {
	 PRINT_ERROR(retval, "DB_ENV->txn_begin");
	 goto error;
      }
   }

   if ((retval = db->cursor(db, txn, &dbc, 0)) != 0) {
      PRINT_ERROR(retval, "DB->cursor");
      goto error;
   }
   while ((retval = dbc->c_get(dbc, &key, &data, DB_NEXT)) == 0) {
      if (caught_any_signals()) goto cleanup;
      //if (dump_dbt(&key) != 0) goto error;
      //if (dump_dbt(&data) != 0) goto error;
      if (dump_dbt(&key, 1) != 0) goto error;
      if (dump_dbt(&data, 0) != 0) goto error;
   }
   if (retval != DB_NOTFOUND) {
      PRINT_ERROR(retval, "DBC->c_get");
      goto error;
   }
   
   if (false) {
error:
      g.exitcode = EXIT_FAILURE;
   }
cleanup:
   if (dbc && (retval = dbc->c_close(dbc)) != 0) {
      PRINT_ERROR(retval, "DBC->c_close");
      g.exitcode = EXIT_FAILURE;
   }
   if (txn) {
       if (retval) {
           int r2 = txn->abort(txn);
           if (r2) PRINT_ERROR(r2, "DB_TXN->abort");
       }
       else {
           retval = txn->commit(txn, 0);
           if (retval) PRINT_ERROR(retval, "DB_TXN->abort");
       }
   }
   return g.exitcode;
}

int close_database()
{
   DB* db = g.db;
   int retval;

   assert(db);
   if ((retval = db->close(db, 0)) != 0) {
      PRINT_ERROR(retval, "DB->close");
      goto error;
   }
   return EXIT_SUCCESS;
error:
   return EXIT_FAILURE;
}

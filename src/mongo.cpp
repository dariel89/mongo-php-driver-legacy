// mongo.c

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <php.h>
#include <mongo/client/dbclient.h>

#include "mongo.h"
#include "bson.h"
#include "db.h"
#include "collection.h"
#include "cursor.h"

/** Objects */
zend_class_entry *mongo_class; 
zend_class_entry *db_class; 
zend_class_entry *collection_class; 
zend_class_entry *cursor_class; 

/** Resources */
int le_db_client_connection;

static function_entry mongo_functions[] = {
  PHP_NAMED_FE( __construct, PHP_FN( mongo___construct ), NULL) 
  PHP_NAMED_FE( connect, PHP_FN( mongo_connect ), NULL )
  PHP_NAMED_FE( close, PHP_FN( mongo_close ), NULL )
  PHP_NAMED_FE( set_db, PHP_FN( mongo_set_db ), NULL )
  PHP_NAMED_FE( get_db, PHP_FN( mongo_get_db ), NULL )
  PHP_NAMED_FE( ensure_index, PHP_FN( mongo_ensure_index ), NULL )
  PHP_NAMED_FE( find, PHP_FN( mongo_find ), NULL )
  PHP_NAMED_FE( find_one, PHP_FN( mongo_find_one ), NULL )
  PHP_NAMED_FE( has_next, PHP_FN( mongo_has_next ), NULL )
  PHP_NAMED_FE( kill, PHP_FN( mongo_kill_cursors ), NULL )
  PHP_NAMED_FE( next2, PHP_FN( mongo_next ), NULL )
  PHP_NAMED_FE( remove, PHP_FN( mongo_remove ), NULL )
  PHP_NAMED_FE( insert, PHP_FN( mongo_insert ), NULL )
  PHP_NAMED_FE( update, PHP_FN( mongo_update ), NULL )
  PHP_NAMED_FE( limit, PHP_FN( mongo_limit ), NULL )
  PHP_NAMED_FE( sort2, PHP_FN( mongo_sort ), NULL )
  {NULL, NULL, NULL}
};

static function_entry db_functions[] = {
  PHP_NAMED_FE( __construct, PHP_FN( db___construct ), NULL) 
  {NULL, NULL, NULL}
};

static function_entry collection_functions[] = {
  PHP_NAMED_FE( __construct, PHP_FN( collection___construct ), NULL) 
  {NULL, NULL, NULL}
};

static function_entry cursor_functions[] = {
  PHP_NAMED_FE( __construct, PHP_FN( cursor___construct ), NULL) 
  {NULL, NULL, NULL}
};

zend_module_entry mongo_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
  STANDARD_MODULE_HEADER,
#endif
  PHP_MONGO_EXTNAME,
  mongo_functions,
  PHP_MINIT(mongo),
  PHP_MSHUTDOWN(mongo),
  NULL,
  NULL,
  NULL,
#if ZEND_MODULE_API_NO >= 20010901
  PHP_MONGO_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_MONGO
ZEND_GET_MODULE(mongo)
#endif


PHP_MINIT_FUNCTION(mongo) {
  le_db_client_connection = zend_register_list_destructors_ex(NULL, NULL, PHP_DB_CLIENT_CONNECTION_RES_NAME, module_number);

  zend_class_entry mongo; 
  INIT_CLASS_ENTRY(mongo, "Mongo", mongo_functions); 
  mongo_class = zend_register_internal_class(&mongo TSRMLS_CC);

  zend_class_entry db; 
  INIT_CLASS_ENTRY(db, "DB", db_functions); 
  db_class = zend_register_internal_class(&db TSRMLS_CC);

  zend_class_entry collection; 
  INIT_CLASS_ENTRY(collection, "Collection", collection_functions); 
  collection_class = zend_register_internal_class(&collection TSRMLS_CC);

  zend_class_entry cursor; 
  INIT_CLASS_ENTRY(cursor, "Cursor", cursor_functions); 
  cursor_class = zend_register_internal_class(&cursor TSRMLS_CC);
  
  return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(mongo) { 
  mongo_class = null;
  return SUCCESS; 
}

PHP_FUNCTION(mongo___construct) {
  // nothing needed here, yet
}

PHP_FUNCTION(mongo_connect) {
  mongo::DBClientConnection *conn = new mongo::DBClientConnection( 1, 0 );
  char *server;
  int server_len;
  string error;
  
  switch( ZEND_NUM_ARGS() ) {
  case 0:
    server = "127.0.0.1";
    break;
  case 1:
    // get the server name from the first parameter
    if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &server, &server_len) == FAILURE ) {
      RETURN_FALSE;
    }
    break;
  default:
    RETURN_FALSE;
  }

  if ( server_len == 0 ) {
    RETURN_FALSE;
  }

  if ( ! conn->connect( server, error ) ){
    RETURN_FALSE;
  }
  
  // return connection
  int resource_num = ZEND_REGISTER_RESOURCE( NULL, conn, le_db_client_connection );
  add_property_resource( getThis(), "connection", resource_num );
}

PHP_FUNCTION(mongo_close) {
  zval *zconn = zend_read_property( mongo_class, getThis(), "connection", 10, 0 TSRMLS_CC );
  zval_dtor( zconn );
  RETURN_TRUE;
}

PHP_FUNCTION(mongo_set_db) {
  char *dbname;
  int dbname_len;
  int argc = ZEND_NUM_ARGS();
  if( argc == 0 ) {
    zend_error( E_ERROR, "incorrect parameters" );
    RETURN_FALSE;
  }
  else if( argc > 1 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &dbname, &dbname_len) == FAILURE ) {
    zend_error( E_ERROR, "incorrect parameters" );
    RETURN_FALSE;
  }

  if( dbname_len == 0 )
    zend_error( E_ERROR, "invalid db name" );

  add_property_stringl( getThis(), "db", dbname, dbname_len, 1 );
}

PHP_FUNCTION(mongo_get_db) {
  zval *zdb = zend_read_property( mongo_class, getThis(), "db", 2, 0 TSRMLS_CC );
  php_printf( "db: %s\n", Z_STRVAL_P( zdb ) );
}

PHP_FUNCTION(mongo_ensure_index) {
  zval *zconn;
  char *collection;
  int collection_len;
  zval *zarray;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rsa", &zconn, &collection, &collection_len, &zarray) == FAILURE) {
    php_printf( "mongo_ensure_index: parameter parse failure\n" );
    RETURN_FALSE;
  }

  mongo::DBClientConnection *conn_ptr = (mongo::DBClientConnection*)zend_fetch_resource(&zconn TSRMLS_CC, -1, PHP_DB_CLIENT_CONNECTION_RES_NAME, NULL, 1, le_db_client_connection);
  if (!conn_ptr) {
    RETURN_FALSE;
  }

  /*  mongo::BSONObj keys = php_array_to_bson( Z_ARRVAL_P(zarray) );
  conn_ptr->ensureIndex( collection, keys );
  RETURN_TRUE;*/
}

PHP_FUNCTION(mongo_find) {
  zval *zconn, *zquery, *zfields;
  char *collection;
  int limit, skip, collection_len, opts;

  int num_args = ZEND_NUM_ARGS();
  switch( num_args ) {
  case 0:
  case 1:
  // TODO: add find( db, ns ) (find all)
  case 2:
    php_printf( "mongo_find: too few args\n" );
    RETURN_FALSE;
    break;
  case 3:
    if (zend_parse_parameters(num_args TSRMLS_CC, "rsa", &zconn, &collection, &collection_len, &zquery) == FAILURE) {
      php_printf( "mongo_find: parameter parse failure\n" );
      RETURN_FALSE;
    }
    break;
  case 4:
    if (zend_parse_parameters(num_args TSRMLS_CC, "rsa", &zconn, &collection, &collection_len, &zquery, &limit) == FAILURE) {
      php_printf( "mongo_find: parameter parse failure\n" );
      RETURN_FALSE;
    }
    break;
  case 5:
    if (zend_parse_parameters(num_args TSRMLS_CC, "rsa", &zconn, &collection, &collection_len, &zquery, &limit, &skip) == FAILURE) {
      php_printf( "mongo_find: parameter parse failure\n" );
      RETURN_FALSE;
    }
    break;
  case 6:
    if (zend_parse_parameters(num_args TSRMLS_CC, "rsa", &zconn, &collection, &collection_len, &zquery, &limit, &skip, &zfields) == FAILURE) {
      php_printf( "mongo_find: parameter parse failure\n" );
      RETURN_FALSE;
    }
    break;
  case 7:
    if (zend_parse_parameters(num_args TSRMLS_CC, "rsa", &zconn, &collection, &collection_len, &zquery, &limit, &skip, &zfields, &opts) == FAILURE) {
      php_printf( "mongo_find: parameter parse failure\n" );
      RETURN_FALSE;
    }
    break;
  default:
    php_printf( "mongo_find: too many args\n" );
    RETURN_FALSE;
    break;
  }

  mongo::DBClientConnection *conn_ptr = (mongo::DBClientConnection*)zend_fetch_resource(&zconn TSRMLS_CC, -1, PHP_DB_CLIENT_CONNECTION_RES_NAME, NULL, 1, le_db_client_connection);
  if (!conn_ptr) {
    php_printf( "mongo_find: no db connection\n" );
    RETURN_FALSE;
  }

  /*mongo::BSONObj bquery = php_array_to_bson( Z_ARRVAL_P( zquery ) );
  if( zfields )
    mongo::BSONObj bfields = php_array_to_bson( Z_ARRVAL_P( zfields ) );
  
    conn_ptr->query( collection, bquery, limit, skip, new mongo::BSONObj(), opts);*/
  RETURN_TRUE;
}

PHP_FUNCTION(mongo_find_one) {
  zval *zconn, *zquery, *zfields;
  char *collection;
  int collection_len, opts;

  int num_args = ZEND_NUM_ARGS();
  switch( num_args ) {
  case 0:
  case 1:
  case 2:
    php_printf( "mongo_find_one: too few args\n" );
    RETURN_FALSE;
    break;
  case 3:
    if (zend_parse_parameters(num_args TSRMLS_CC, "rsa", &zconn, &collection, &collection_len, &zquery) == FAILURE) {
      php_printf( "mongo_find_one: parameter parse failure\n" );
      RETURN_FALSE;
    }
    break;
  case 4:
    if (zend_parse_parameters(num_args TSRMLS_CC, "rsa", &zconn, &collection, &collection_len, &zquery, &zfields) == FAILURE) {
      php_printf( "mongo_find_one: parameter parse failure\n" );
      RETURN_FALSE;
    }
    break;
  case 5:
    if (zend_parse_parameters(num_args TSRMLS_CC, "rsa", &zconn, &collection, &collection_len, &zquery, &zfields, &opts) == FAILURE) {
      php_printf( "mongo_find_one: parameter parse failure\n" );
      RETURN_FALSE;
    }
    break;
  default:
    php_printf( "mongo_find_one: too many args\n" );
    RETURN_FALSE;
    break;
  }
  
  mongo::DBClientConnection *conn_ptr = (mongo::DBClientConnection*)zend_fetch_resource(&zconn TSRMLS_CC, -1, PHP_DB_CLIENT_CONNECTION_RES_NAME, NULL, 1, le_db_client_connection);
  if (!conn_ptr) {
    php_printf( "mongo_find_one: no db connection\n" );
    RETURN_FALSE;
  }
  /*
  mongo::BSONObj bquery = php_array_to_bson( Z_ARRVAL_P( zquery ) );
  if( !zfields ) {
    mongo::BSONObj bfields = php_array_to_bson( Z_ARRVAL_P( zfields ) );
    conn_ptr->findOne( collection, bquery );
  }
  else {
    mongo::BSONObj bfields = php_array_to_bson( Z_ARRVAL_P( zfields ) );
    if( !opts ) {
      conn_ptr->findOne( collection, bquery, &bfields );
    }
    else {
      conn_ptr->findOne( collection, bquery, &bfields, opts ); //TODO: return this!
    }
    }*/
  RETURN_TRUE;
}

PHP_FUNCTION(mongo_has_next) {
  php_printf( "not yet implemented." );
  RETURN_TRUE;
}

PHP_FUNCTION(mongo_kill_cursors) {
  php_printf( "not yet implemented." );
  RETURN_TRUE;
}

PHP_FUNCTION(mongo_next) {
  php_printf( "not yet implemented." );
  RETURN_TRUE;
}

PHP_FUNCTION(mongo_remove) {
  zval *zconn, *zarray;
  char *collection;
  int collection_len;
  zend_bool justOne = 0;

  if ( ZEND_NUM_ARGS() == 3 ) {
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rsa", &zconn, &collection, &collection_len, &zarray) == FAILURE) {
      php_printf( "mongo_remove: parameter parse failure (3)\n" );
      RETURN_FALSE;
    }
  }
  else if( ZEND_NUM_ARGS() == 4 ) {
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rsab", &zconn, &collection, &collection_len, &zarray, &justOne) == FAILURE) {
      php_printf( "mongo_remove: parameter parse failure (4)\n" );
      RETURN_FALSE;
    }
  }

  mongo::DBClientConnection *conn_ptr = (mongo::DBClientConnection*)zend_fetch_resource(&zconn TSRMLS_CC, -1, PHP_DB_CLIENT_CONNECTION_RES_NAME, NULL, 1, le_db_client_connection);
  if (!conn_ptr) {
    RETURN_FALSE;
  }

  /*  mongo::BSONObj rarray = php_array_to_bson( Z_ARRVAL_P(zarray) );
  conn_ptr->remove( collection, rarray, justOne );
  RETURN_TRUE;*/
}

PHP_FUNCTION(mongo_insert) {
  zval *zconn, *zarray;
  char *collection;
  int collection_len;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rsa", &zconn, &collection, &collection_len, &zarray) == FAILURE) {
    php_printf( "mongo_insert: parameter parse failure\n" );
    RETURN_FALSE;
  }

  mongo::DBClientConnection *conn_ptr = (mongo::DBClientConnection*)zend_fetch_resource(&zconn TSRMLS_CC, -1, PHP_DB_CLIENT_CONNECTION_RES_NAME, NULL, 1, le_db_client_connection);
  if (!conn_ptr) {
    RETURN_FALSE;
  }

  mongo::BSONObjBuilder *obj_builder = new mongo::BSONObjBuilder();
  HashTable *php_array = Z_ARRVAL_P(zarray);
  if( !zend_hash_exists( php_array, "_id", 3 ) )
      prep_obj_for_db( obj_builder );
  php_array_to_bson( obj_builder, php_array );
  conn_ptr->insert( collection, obj_builder->doneAndDecouple() );
  php_printf("kisses! xoxoxoxo\n");
}

PHP_FUNCTION(mongo_update) {
  zval *zconn, *zquery, *zobj;
  char *collection;
  int collection_len;
  zend_bool upsert = 0;

  int num_args = ZEND_NUM_ARGS();
  switch( num_args ) {
  case 0:
  case 1:
  case 2:
  case 3:
    php_printf( "mongo_update: too few args\n" );
    RETURN_FALSE;
    break;
  case 4:
    if (zend_parse_parameters(num_args TSRMLS_CC, "rsa", &zconn, &collection, &collection_len, &zquery, &zobj) == FAILURE) {
      php_printf( "mongo_update: parameter parse failure\n" );
      RETURN_FALSE;
    }
    break;
  case 5:
    if (zend_parse_parameters(num_args TSRMLS_CC, "rsa", &zconn, &collection, &collection_len, &zquery, &zobj, &upsert) == FAILURE) {
      php_printf( "mongo_update: parameter parse failure\n" );
      RETURN_FALSE;
    }
    break;
  default:
    php_printf( "mongo_update: too many args\n" );
    RETURN_FALSE;
    break;
  }

  mongo::DBClientConnection *conn_ptr = (mongo::DBClientConnection*)zend_fetch_resource(&zconn TSRMLS_CC, -1, PHP_DB_CLIENT_CONNECTION_RES_NAME, NULL, 1, le_db_client_connection);
  if (!conn_ptr) {
    RETURN_FALSE;
  }
  /*
  mongo::BSONObj bquery = php_array_to_bson( Z_ARRVAL_P( zquery ) );
  mongo::BSONObj bobj = php_array_to_bson( Z_ARRVAL_P( zobj ) );
  conn_ptr->update( collection, bquery, bobj, upsert );
  RETURN_TRUE;*/
}

PHP_FUNCTION(mongo_limit) {
  php_printf( "not yet implemented." );
  RETURN_TRUE;
}

PHP_FUNCTION(mongo_sort) {
  php_printf( "not yet implemented." );
  RETURN_TRUE;
}

PHP_FUNCTION( temp ) {
  /*zval *zarray;
  HashTable *arr_hash;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &zarray) == FAILURE) {
    RETURN_FALSE;
  }

  arr_hash = Z_ARRVAL_P(zarray);
  mongo::BSONObj obj = php_array_to_bson( arr_hash );
  zval *ret_array = bson_to_php_array( obj );
  RETURN_ZVAL( ret_array, 0, 1 );*/
}



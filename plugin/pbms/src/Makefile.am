# Used to build Makefile.in
#SUBDIRS =				cslib

MYSQL_OPTIONS =			$(ENG_DRIZZLE_FLAG) -DSIGNALS_DONT_BREAK_READ -DIGNORE_SIGHUP_SIGQUIT -D_P1003_1B_VISIBLE -DUNIV_MUST_NOT_INLINE -DFORCE_INIT_OF_VARS 

INCLUDES =				$(ENG_MYSQL_INC) -I/usr/include/libxml2 -Icslib -I/System/Library/Frameworks/CoreFoundation.framework/Headers

LIBS =					-L/usr/local/lib

#noinst_PROGRAMS = transtest
noinst_PROGRAMS = prottest

LDADD =

noinst_HEADERS			= pbms.h ha_pbms.h ConnectionHandler_ms.h Network_ms.h OpenTable_ms.h Table_ms.h ms_mysql.h \
						  Repository_ms.h Database_ms.h Util_ms.h TempLog_ms.h Compactor_ms.h SystemTable_ms.h \
						  Engine_ms.h Defs_ms.h Discover_ms.h cloud.h SysTab_util.h SysTab_dump.h SysTab_variable.h \
						  SysTab_cloud.h SysTab_backup.h SysTab_enabled.h SysTab_httpheader.h backup_ms.h TransCache_ms.h \
						  TransLog_ms.h Transaction_ms.h pbms_enabled.h

mysqlincludedir			= $(includedir)/$(MYSQL_DIR)
mysqllibdir				= $(libdir)/$(MYSQL_DIR)
plugindir				= $(libdir)/$(MYSQ_SERVER_TYPE)/plugin
#mysqlinclude_HEADERS	= pbmslib.h
mysqllib_LTLIBRARIES	= libpbmscl.la 
plugin_LTLIBRARIES		= libpbms.la 

CSLIB_SOURCES =			 cslib/CSStorage.cc		cslib/CSObject.cc  		cslib/CSException.cc  	cslib/CSLog.cc  		cslib/CSString.cc \
						 cslib/CSThread.cc 		cslib/CSStrUtil.cc  	cslib/CSMemory.cc  		cslib/CSUTF8.cc    		cslib/CSMutex.cc   \
						 cslib/CSTest.cc		cslib/CSPath.cc  		cslib/CSFile.cc  		cslib/CSStream.cc 		cslib/CSTime.cc   \
						 cslib/CSDirectory.cc	cslib/CSSocket.cc  		cslib/CSTokenStream.cc  cslib/CSProperties.cc  	cslib/CSHTTPStream.cc \
						 cslib/CSMd5.cc			cslib/CSEncode.cc		cslib/CSSha1.cc			cslib/CSS3Protocol.cc 
						 
#transtest_SOURCES = TransTest.cc TransLog_ms.cc TransCache_ms.cc $(CSLIB_SOURCES)
#transtest_CXXFLAGS		= $(AM_CFLAGS)  -DMYSQL_SERVER
#transtest_LDADD =  -L../src/.libs -L$(libdir)/$(MYSQL_DIR)  -l$(MYSQ_CLIENT_LIB)

prottest_SOURCES = $(CSLIB_SOURCES)
prottest_CXXFLAGS		= $(AM_CFLAGS)  -DMYSQL_SERVER -DS3_UNIT_TEST
prottest_LDADD =  -L../src/.libs -L$(libdir)/$(MYSQL_DIR)  -l$(MYSQ_CLIENT_LIB) -lcurl -lxml2

libpbmscl_la_SOURCES	= pbmslib.cc  Util_ms.cc $(CSLIB_SOURCES)

libpbms_la_SOURCES		= ha_pbms.cc ConnectionHandler_ms.cc Network_ms.cc OpenTable_ms.cc Table_ms.cc ms_mysql.cc \
						  Repository_ms.cc Database_ms.cc Util_ms.cc TempLog_ms.cc Compactor_ms.cc SystemTable_ms.cc \
						  Engine_ms.cc ms_api.cc  Discover_ms.cc udf_ms.cc blobcontainer_ms.cc cloud.cc \
						  SysTab_dump.cc SysTab_util.cc SysTab_variable.cc SysTab_cloud.cc SysTab_backup.cc SysTab_enabled.cc \
						  SysTab_httpheader.cc backup_ms.cc TransCache_ms.cc TransLog_ms.cc Transaction_ms.cc pbms_enabled.cc \
						  $(CSLIB_SOURCES)

# libpbms_la_SOURCES		= blobcontainer_ms.cc

#libpbms_la_LDFLAGS =	-ls3 -module -rpath $(plugindir)
libpbms_la_LDFLAGS =	-module -rpath $(plugindir)  -lcurl -lxml2
libpbmscl_la_LDFLAGS =	-lcurl 

# NOTE: -DPBMS_VERSION=$(VERSION) is required because MySQL headers will set VERSION to the MySQL version. 

libpbms_la_CXXFLAGS		= $(AM_CFLAGS) $(ENG_DRIZZLE_FLAG)   -DMYSQL_DYNAMIC_PLUGIN -DDRIZZLE_DYNAMIC_PLUGIN -DMYSQL_SERVER -DPBMS_VERSION=$(VERSION)
libpbms_la_CFLAGS		= $(AM_CFLAGS) $(ENG_DRIZZLE_FLAG) -DMYSQL_DYNAMIC_PLUGIN -DDRIZZLE_DYNAMIC_PLUGIN -DMYSQL_SERVER -DPBMS_VERSION=$(VERSION) -std=c99

libpbmscl_la_CXXFLAGS	= $(libpbms_la_CXXFLAGS)  $(ENG_DRIZZLE_FLAG) -DNO_XML2

EXTRA_LIBRARIES =		libpbms.a libpbmscl.a
noinst_LIBRARIES = 		libpbms.a
libpbms_a_SOURCES =		$(libpbms_la_SOURCES)
libpbms_a_CXXFLAGS =	$(AM_CFLAGS) $(ENG_DRIZZLE_FLAG) -DMYSQL_SERVER -DPBMS_VERSION=$(VERSION)
libpbms_a_CFLAGS =		$(AM_CFLAGS) $(ENG_DRIZZLE_FLAG) -DMYSQL_SERVER  -DPBMS_VERSION=$(VERSION) -std=c99

libpbmscl_a_SOURCES =		$(libpbmscl_la_SOURCES)
libpbmscl_a_CXXFLAGS =	$(AM_CFLAGS) $(ENG_DRIZZLE_FLAG) -DMYSQL_SERVER -DNO_XML2

EXTRA_DIST				= CMakeFiles.txt

install-data-hook:
	if test $(MYSQ_SERVER_TYPE) = "drizzle" ; then\
		cat pbmslib.h | sed "s:/\*<DRIZZLE>\*/:#define DRIZZLED:" > $(mysqlincludedir)/pbmslib.h; \
	else \
		cat pbmslib.h | sed "s:/\*<DRIZZLE>\*/::" > $(mysqlincludedir)/pbmslib.h; \
	fi
